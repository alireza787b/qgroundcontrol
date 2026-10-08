#include "PixEagleVideoItem.h"

#include <cmath>
#include <optional>
#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QRegularExpression>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtGui/QGuiApplication>
#include <QtGui/QTransform>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/private/qvideoframeconverter_p.h>
#include <QtMultimedia/private/qvideotransformation_p.h>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGSimpleTextureNode>

#include "QGCVideoFrameContextStore.h"
#include "QGCWebSocketVideoSource.h"

namespace {
constexpr qint64 FRESHNESS_LIMIT_MS = 1500;

struct Frame
{
    QImage image;
    QVariantMap context;
    std::shared_ptr<QGCVideoFrameContextStore> store;
    quint64 storeEpoch = 0;
    quint64 streamGeneration = 0;
    quint64 surfaceGeneration = 0;
    qint64 receivedMonotonicMs = 0;
    QTransform displayToEncoded;
    QJsonObject selectionGeometry;
};

bool axisAligned(const QTransform& transform)
{
    for (qreal value : {transform.m11(), transform.m12(), transform.m13(), transform.m21(), transform.m22(),
                        transform.m23(), transform.m31(), transform.m32(), transform.m33()}) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return transform.isAffine() && transform.isInvertible() &&
           ((qFuzzyIsNull(transform.m12()) && qFuzzyIsNull(transform.m21())) ||
            (qFuzzyIsNull(transform.m11()) && qFuzzyIsNull(transform.m22())));
}

struct RenderGeometry
{
    QQuickWindow* window = nullptr;
    QSizeF size;
    QTransform itemToScene;
    QRectF visibleSceneRect;
    qreal devicePixelRatio = 0;
    bool valid = false;

    bool operator==(const RenderGeometry&) const = default;
};

RenderGeometry renderGeometry(const QQuickItem* item)
{
    RenderGeometry result;
    result.window = item->window();
    if (!result.window) {
        return result;
    }
    result.size = item->size();
    result.itemToScene = item->itemTransform(nullptr, nullptr);
    result.devicePixelRatio = result.window->devicePixelRatio();
    result.visibleSceneRect = QRectF(0, 0, result.window->width(), result.window->height());
    result.valid = !result.size.isEmpty() && axisAligned(result.itemToScene) &&
                   std::isfinite(result.devicePixelRatio) && result.devicePixelRatio > 0;
    for (const auto* ancestor = item; ancestor; ancestor = ancestor->parentItem()) {
        if (ancestor->clip()) {
            const auto transform = ancestor->itemTransform(nullptr, nullptr);
            result.valid = result.valid && axisAligned(transform);
            result.visibleSceneRect = result.visibleSceneRect.intersected(transform.mapRect(ancestor->boundingRect()));
        }
    }
    return result;
}

bool freshFrame(const Frame& frame)
{
    const auto& context = frame.context;
    const qint64 elapsed = QGCVideoFrameContextStore::monotonicMs() - frame.receivedMonotonicMs;
    const auto captureAge = context.value(QStringLiteral("capture_age_ms"));
    return context.value(QStringLiteral("capture_state")).toString() == "fresh" &&
           !context.value(QStringLiteral("capture_id")).isNull() &&
           !context.value(QStringLiteral("source_epoch")).isNull() && !captureAge.isNull() && elapsed >= 0 &&
           elapsed + captureAge.toDouble() <= FRESHNESS_LIMIT_MS &&
           elapsed + context.value(QStringLiteral("publication_age_ms")).toDouble() <= FRESHNESS_LIMIT_MS;
}

QPointF rotateNormalized(QPointF point, QtVideo::Rotation rotation)
{
    switch (rotation) {
        case QtVideo::Rotation::Clockwise90:
            return {1 - point.y(), point.x()};
        case QtVideo::Rotation::Clockwise180:
            return {1 - point.x(), 1 - point.y()};
        case QtVideo::Rotation::Clockwise270:
            return {point.y(), 1 - point.x()};
        default:
            return point;
    }
}

QTransform displayToEncoded(const QVideoFrame& video)
{
    const auto transform = [&video](QPointF point) {
        const auto format = video.surfaceFormat();
        if (format.scanLineDirection() == QVideoFrameFormat::BottomToTop) {
            point.setY(1 - point.y());
        }
        point = rotateNormalized(point, format.rotation());
        if (format.isMirrored()) {
            point.setX(1 - point.x());
        }
        point = rotateNormalized(point, video.rotation());
        if (video.mirrored()) {
            point.setX(1 - point.x());
        }
        return point;
    };
    const auto origin = transform({0, 0});
    const auto xAxis = transform({1, 0}) - origin;
    const auto yAxis = transform({0, 1}) - origin;
    return QTransform(xAxis.x(), xAxis.y(), yAxis.x(), yAxis.y(), origin.x(), origin.y()).inverted();
}

struct Selection
{
    QString token;
    std::shared_ptr<const Frame> frame;
    RenderGeometry geometry;
    QPointer<QQuickItem> inputItem;
    QTransform inputToItem;
    QPointF start;
};

std::optional<QPointF> encodedPoint(const Selection& selection, QPointF point)
{
    if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
        return std::nullopt;
    }
    point = selection.inputToItem.map(point);
    if (!QRectF(QPointF(), selection.geometry.size).contains(point) ||
        !selection.geometry.visibleSceneRect.contains(selection.geometry.itemToScene.map(point))) {
        return std::nullopt;
    }
    point = {point.x() / selection.geometry.size.width(), point.y() / selection.geometry.size.height()};
    return selection.frame->displayToEncoded.map(point);
}

struct IncomingFrame
{
    QVideoFrame video;
    std::optional<QGCVideoFrameContextStore::Entry> entry;
    std::shared_ptr<QGCVideoFrameContextStore> store;
    QJsonObject expected;
    quint64 streamGeneration = 0;
    quint64 surfaceGeneration = 0;
};

class FrameNode final : public QSGSimpleTextureNode
{
public:
    std::shared_ptr<const Frame> frame;
};

bool decimalIdentifier(const QJsonValue& value)
{
    static const QRegularExpression pattern(QStringLiteral("^(0|[1-9][0-9]*)$"));
    return value.isString() && pattern.match(value.toString()).hasMatch();
}

bool nonnegativeAge(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= 0;
}

QJsonObject validatedSelectionGeometry(const IncomingFrame& incoming)
{
    if (!incoming.entry) {
        return {};
    }
    const auto geometry = incoming.entry->metadata.value("selection_geometry").toObject();
    if (geometry.value("version").toString() != "1" || !geometry.value("verified").isBool() ||
        !geometry.value("verified").toBool() || geometry.value("mapping").toString() != "full_frame_scale" ||
        geometry.value("max_age_ms") != QJsonValue(FRESHNESS_LIMIT_MS) ||
        !decimalIdentifier(geometry.value("target_revision"))) {
        return {};
    }
    for (const auto* key : {"geometry_id", "token"}) {
        if (!geometry.value(key).isString() || geometry.value(key).toString().isEmpty() ||
            geometry.value(key).toString().size() > 256) {
            return {};
        }
    }
    for (const auto* key : {"encoded_width", "encoded_height", "analysis_width", "analysis_height"}) {
        const auto value = geometry.value(key);
        const int dimension = value.toInt(-1);
        if (dimension <= 0 || dimension > QGCWebSocketVideoSource::kMaximumJpegDimension ||
            value != QJsonValue(dimension)) {
            return {};
        }
    }
    if (geometry.value("encoded_width").toInt() != incoming.video.width() ||
        geometry.value("encoded_height").toInt() != incoming.video.height()) {
        return {};
    }
    return geometry;
}

bool compatibleDeliverySize(const QJsonObject& video, const QSize& size)
{
    const int baseWidth = video.value("width").toInt();
    const int baseHeight = video.value("height").toInt();
    if (video.value("delivery_scaling_version").toString() != "1") {
        return (baseWidth <= 0 || size.width() == baseWidth) && (baseHeight <= 0 || size.height() == baseHeight);
    }
    if (baseWidth <= 0 || baseHeight <= 0 || size.width() <= 0 || size.height() <= 0 || size.width() > baseWidth ||
        size.height() > baseHeight) {
        return false;
    }
    // Both encoded dimensions may round by half a pixel after uniform scaling.
    const qint64 aspectError = std::abs(qint64(size.width()) * baseHeight - qint64(size.height()) * baseWidth);
    return aspectError * 2 <= qint64(baseWidth) + baseHeight;
}

QVariantMap validatedContext(const IncomingFrame& incoming)
{
    if (!incoming.entry || !incoming.store->isCurrentEpoch(incoming.entry->epoch)) {
        return {};
    }
    const auto& metadata = incoming.entry->metadata;
    const auto provenance = metadata.value("provenance").toObject();
    const auto video = incoming.expected.value("video").toObject();
    if (metadata.value("type").toString() != "frame" || provenance.value("version").toString() != "1" ||
        !decimalIdentifier(provenance.value("frame_id")) ||
        provenance.value("encoded_width").toInt(-1) != incoming.video.width() ||
        provenance.value("encoded_height").toInt(-1) != incoming.video.height() ||
        !nonnegativeAge(provenance.value("publication_age_ms")) || !provenance.value("geometry_verified").isBool() ||
        provenance.value("geometry_verified").toBool()) {
        return {};
    }
    for (const auto* key : {"instance_id", "runtime_id"}) {
        if (provenance.value(key).toString().isEmpty() || provenance.value(key) != incoming.expected.value(key)) {
            return {};
        }
    }
    for (const auto* key : {"stream_id", "stream_epoch", "variant"}) {
        if (provenance.value(key).toString().isEmpty() || provenance.value(key) != video.value(key)) {
            return {};
        }
    }
    const auto sourceEpoch = provenance.value("source_epoch");
    const auto captureId = provenance.value("capture_id");
    const auto captureAge = provenance.value("capture_age_ms");
    const QString captureState = provenance.value("capture_state").toString();
    if ((!sourceEpoch.isNull() && (!sourceEpoch.isString() || sourceEpoch.toString().isEmpty())) ||
        sourceEpoch != video.value("source_epoch") || (!captureId.isNull() && !decimalIdentifier(captureId)) ||
        (!captureAge.isNull() && !nonnegativeAge(captureAge)) ||
        (captureState != "fresh" && captureState != "cached" && captureState != "unavailable" &&
         captureState != "unknown") ||
        !compatibleDeliverySize(video, incoming.video.size())) {
        return {};
    }
    auto result = provenance.toVariantMap();
    result.insert(QStringLiteral("pts_us"), QString::number(incoming.video.startTime()));
    return result;
}

QImage presentationImage(const QVideoFrame& video)
{
    // JPEG's dedicated receiver supplies CPU frames. Never force GPU texture readback.
    if (!video.isValid() || video.handleType() != QVideoFrame::NoHandle) {
        return {};
    }
    // Pinned Qt 6.11.1 private CPU path: toImage() may upload and read back even a CPU-backed frame.
    QImage image = qImageFromVideoFrame(video, VideoTransformation{}, true).copy();
    if (image.isNull()) {
        return {};
    }
    const auto format = video.surfaceFormat();
    if (format.scanLineDirection() == QVideoFrameFormat::BottomToTop) {
        image = image.flipped(Qt::Vertical);
    }
    image = image.transformed(QTransform().rotate(static_cast<int>(format.rotation())));
    if (format.isMirrored()) {
        image = image.flipped(Qt::Horizontal);
    }
    image = image.transformed(QTransform().rotate(static_cast<int>(video.rotation())));
    if (video.mirrored()) {
        image = image.flipped(Qt::Horizontal);
    }
    return image;
}
}  // namespace

struct PixEagleVideoItem::State
{
    QMutex mutex;
    std::shared_ptr<QGCVideoFrameContextStore> store;
    QJsonObject expected;
    std::optional<IncomingFrame> incoming;
    std::shared_ptr<const Frame> pending;
    std::shared_ptr<const Frame> nodeFrame;
    std::shared_ptr<const Frame> swapping;
    std::shared_ptr<const Frame> presented;
    RenderGeometry swappingGeometry;
    RenderGeometry presentedGeometry;
    std::optional<Selection> selection;
    QString selectionError;
    quint64 selectionInvalidation = 0;
    QQuickWindow* window = nullptr;
    quint64 streamGeneration = 0;
    quint64 surfaceGeneration = 0;
    bool active = false;
    bool deliveryPosted = false;

    bool current(const std::shared_ptr<const Frame>& frame) const
    {
        return active && frame && frame->streamGeneration == streamGeneration &&
               frame->surfaceGeneration == surfaceGeneration && frame->store &&
               frame->store->isCurrentEpoch(frame->storeEpoch);
    }

    void invalidate()
    {
        ++surfaceGeneration;
        incoming.reset();
        pending.reset();
        nodeFrame.reset();
        swapping.reset();
        presented.reset();
        presentedGeometry = {};
        swappingGeometry = {};
        cancelSelection(PixEagleVideoItem::tr("Video changed. Select again."));
    }

    void cancelSelection(const QString& reason)
    {
        if (selection) {
            selection.reset();
            selectionError = reason;
            ++selectionInvalidation;
        }
    }
};

PixEagleVideoItem::PixEagleVideoItem(QQuickItem* parent)
    : QQuickItem(parent)
    , _state(std::make_shared<State>())
{
    setFlag(ItemHasContents, true);
    _freshnessTimer.setInterval(100);
    connect(&_freshnessTimer, &QTimer::timeout, this, &PixEagleVideoItem::_publishProperties);
    connect(this, &QQuickItem::windowChanged, this, &PixEagleVideoItem::_attachWindow);
    connect(this, &QQuickItem::parentChanged, this, &PixEagleVideoItem::_observeAncestors);
    connect(this, &QQuickItem::visibleChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::opacityChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::widthChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::heightChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::xChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::yChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::scaleChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::rotationChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::transformOriginChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(this, &QQuickItem::clipChanged, this, &PixEagleVideoItem::_refreshSurface);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &PixEagleVideoItem::_refreshSurface);
    _replaceSink();
    _observeAncestors();
    _attachWindow(window());
}

PixEagleVideoItem::~PixEagleVideoItem()
{
    // QQuickItem emits parent/window changes from its destructor, after our state is destroyed.
    disconnect(this, nullptr, this, nullptr);
    disconnect(qGuiApp, nullptr, this, nullptr);
    _freshnessTimer.stop();
    for (const auto& connection : std::as_const(_ancestorConnections)) {
        disconnect(connection);
    }
    for (const auto& connection : std::as_const(_windowConnections)) {
        disconnect(connection);
    }
    if (_attachedWindow) {
        _attachedWindow->removeEventFilter(this);
    }
    disconnect(_videoSink, nullptr, _videoSink, nullptr);
    const QMutexLocker lock(&_state->mutex);
    _state->active = false;
    _state->invalidate();
}

void PixEagleVideoItem::setStream(std::shared_ptr<QGCVideoFrameContextStore> store, const QJsonObject& expectedContext)
{
    {
        const QMutexLocker lock(&_state->mutex);
        ++_state->streamGeneration;
        _state->invalidate();
        _state->store = std::move(store);
        _state->expected = expectedContext;
        _state->deliveryPosted = false;
    }
    _replaceSink();
    _refreshSurface();
    update();
    _publishProperties();
}

void PixEagleVideoItem::clearStream()
{
    setStream({}, {});
}

void PixEagleVideoItem::_replaceSink()
{
    if (_videoSink) {
        disconnect(_videoSink, nullptr, _videoSink, nullptr);
        _videoSink->deleteLater();
    }
    _videoSink = new QVideoSink(this);
    const auto weak = std::weak_ptr<State>(_state);
    const quint64 generation = _state->streamGeneration;
    auto* sink = _videoSink;
    connect(
        sink, &QVideoSink::videoFrameChanged, sink,
        [this, weak, sink, generation](const QVideoFrame& video) {
            const auto state = weak.lock();
            if (!state) {
                return;
            }
            const QMutexLocker lock(&state->mutex);
            if (generation != state->streamGeneration || !state->active || !state->store) {
                return;
            }
            state->incoming = IncomingFrame{video,        state->store->lookup(video.startTime()),
                                            state->store, state->expected,
                                            generation,   state->surfaceGeneration};
            if (!state->deliveryPosted) {
                state->deliveryPosted = true;
                // The sink owns this queued delivery; replacing it cannot deliver an old stream to the new one.
                QMetaObject::invokeMethod(
                    sink, [this, generation]() { _consumeFrame(generation); }, Qt::QueuedConnection);
            }
        },
        Qt::DirectConnection);
    emit videoSinkChanged();
}

void PixEagleVideoItem::_consumeFrame(quint64 streamGeneration)
{
    std::optional<IncomingFrame> incoming;
    {
        const QMutexLocker lock(&_state->mutex);
        if (streamGeneration != _state->streamGeneration) {
            return;
        }
        _state->deliveryPosted = false;
        incoming = std::move(_state->incoming);
        _state->incoming.reset();
    }
    if (!incoming) {
        return;
    }
    auto frame = std::make_shared<Frame>();
    frame->context = validatedContext(*incoming);
    if (incoming->entry && frame->context.isEmpty()) {
        // A camera/source restart can rotate stream provenance while the
        // authenticated QGC context still describes the previous epoch. Let
        // the controller refresh that authoritative context; keep the frame
        // rejected for presentation and target selection until it is current.
        emit frameContextRejected();
    }
    if (!frame->context.isEmpty()) {
        frame->image = presentationImage(incoming->video);
    }
    frame->store = incoming->store;
    frame->storeEpoch = incoming->entry ? incoming->entry->epoch : 0;
    frame->receivedMonotonicMs = incoming->entry ? incoming->entry->receivedMonotonicMs : 0;
    frame->streamGeneration = incoming->streamGeneration;
    frame->surfaceGeneration = incoming->surfaceGeneration;
    frame->displayToEncoded = displayToEncoded(incoming->video);
    frame->selectionGeometry = validatedSelectionGeometry(*incoming);
    {
        const QMutexLocker lock(&_state->mutex);
        if (frame->streamGeneration != _state->streamGeneration ||
            frame->surfaceGeneration != _state->surfaceGeneration) {
            return;
        }
        _state->pending = frame;
        if (frame->image.isNull()) {
            _state->presented.reset();
            _state->swapping.reset();
            _state->cancelSelection(tr("The displayed frame cannot be verified. Select again when video is live."));
        } else if (_state->selection && (_state->selection->frame->displayToEncoded != frame->displayToEncoded ||
                                         _state->selection->frame->image.size() != frame->image.size() ||
                                         _state->selection->frame->selectionGeometry.value("geometry_id") !=
                                             frame->selectionGeometry.value("geometry_id") ||
                                         _state->selection->frame->selectionGeometry.value("target_revision") !=
                                             frame->selectionGeometry.value("target_revision"))) {
            _state->cancelSelection(tr("Video geometry changed. Select again."));
        }
    }
    update();
    _publishProperties();
}

QSGTexture* PixEagleVideoItem::createFrameTexture(QQuickWindow* window, const QImage& image)
{
    return window->createTextureFromImage(image);
}

QSGNode* PixEagleVideoItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<FrameNode*>(oldNode);
    const QMutexLocker lock(&_state->mutex);
    const auto candidate = _state->pending;
    if (!_surfaceUsable() || !_state->current(candidate) || candidate->image.isNull()) {
        delete node;
        _state->nodeFrame.reset();
        _state->presented.reset();
        return nullptr;
    }
    if (!node || node->frame != candidate) {
        // Never change a texture in place: failed allocation cannot pair old pixels with new provenance.
        auto* texture = createFrameTexture(window(), candidate->image);
        delete node;
        if (!texture || texture->textureSize().isEmpty()) {
            delete texture;
            _state->pending.reset();
            _state->nodeFrame.reset();
            _state->presented.reset();
            return nullptr;
        }
        node = new FrameNode();
        node->setTexture(texture);
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        node->frame = candidate;
    }
    node->setRect(boundingRect());
    _state->nodeFrame = node->frame;
    return node;
}

bool PixEagleVideoItem::_surfaceUsable() const
{
    const auto* surface = window();
    if (!surface || !surface->isVisible() || !surface->isExposed() || width() <= 0 || height() <= 0 ||
        surface->visibility() == QWindow::Minimized || qGuiApp->applicationState() == Qt::ApplicationSuspended ||
        qGuiApp->applicationState() == Qt::ApplicationHidden) {
        return false;
    }
    QRectF visibleRect = mapRectToScene(boundingRect()).intersected(QRectF(0, 0, surface->width(), surface->height()));
    for (const QQuickItem* item = this; item; item = item->parentItem()) {
        if (!item->isVisible() || item->opacity() <= 0) {
            return false;
        }
        if (item->clip()) {
            visibleRect = visibleRect.intersected(item->mapRectToScene(item->boundingRect()));
        }
    }
    return !visibleRect.isEmpty();
}

void PixEagleVideoItem::_observeAncestors()
{
    for (const auto& connection : std::as_const(_ancestorConnections)) {
        disconnect(connection);
    }
    _ancestorConnections.clear();
    for (auto* item = parentItem(); item; item = item->parentItem()) {
        _ancestorConnections.append(
            connect(item, &QQuickItem::parentChanged, this, &PixEagleVideoItem::_observeAncestors));
        _ancestorConnections.append(
            connect(item, &QQuickItem::visibleChanged, this, &PixEagleVideoItem::_refreshSurface));
        _ancestorConnections.append(
            connect(item, &QQuickItem::opacityChanged, this, &PixEagleVideoItem::_refreshSurface));
        for (const auto signal :
             {&QQuickItem::xChanged, &QQuickItem::yChanged, &QQuickItem::widthChanged, &QQuickItem::heightChanged,
              &QQuickItem::scaleChanged, &QQuickItem::rotationChanged}) {
            _ancestorConnections.append(connect(item, signal, this, &PixEagleVideoItem::_refreshSurface));
        }
        _ancestorConnections.append(
            connect(item, &QQuickItem::transformOriginChanged, this, &PixEagleVideoItem::_refreshSurface));
        _ancestorConnections.append(connect(item, &QQuickItem::clipChanged, this, &PixEagleVideoItem::_refreshSurface));
    }
    _refreshSurface();
}

void PixEagleVideoItem::_invalidateSurface()
{
    {
        const QMutexLocker lock(&_state->mutex);
        _state->active = false;
        _state->invalidate();
    }
    _freshnessTimer.stop();
    update();
    _publishProperties();
}

void PixEagleVideoItem::_refreshSurface()
{
    const bool usable = _surfaceUsable();
    const auto geometry = renderGeometry(this);
    bool active;
    {
        const QMutexLocker lock(&_state->mutex);
        active = usable && bool(_state->store);
        if (_state->active != active) {
            _state->invalidate();
            _state->active = active;
        }
        if (_state->selection && _state->selection->geometry != geometry) {
            _state->cancelSelection(tr("Video layout changed. Select again."));
        }
    }
    if (active) {
        _freshnessTimer.start();
    } else {
        _freshnessTimer.stop();
    }
    update();
    _publishProperties();
}

void PixEagleVideoItem::_attachWindow(QQuickWindow* surface)
{
    for (const auto& connection : std::as_const(_windowConnections)) {
        disconnect(connection);
    }
    _windowConnections.clear();
    if (_attachedWindow) {
        _attachedWindow->removeEventFilter(this);
    }
    _invalidateSurface();
    _attachedWindow = surface;
    {
        const QMutexLocker lock(&_state->mutex);
        _state->window = surface;
    }
    if (!surface) {
        return;
    }
    surface->installEventFilter(this);
    _windowConnections.append(connect(surface, &QWindow::visibleChanged, this, &PixEagleVideoItem::_refreshSurface));
    _windowConnections.append(connect(surface, &QWindow::visibilityChanged, this, &PixEagleVideoItem::_refreshSurface));
    _windowConnections.append(connect(
        surface, &QQuickWindow::afterSynchronizing, this,
        [this, surface]() {
            const bool usable = _surfaceUsable();
            const auto geometry = renderGeometry(this);
            const QMutexLocker lock(&_state->mutex);
            if (_state->window == surface) {
                _state->swapping = usable && _state->current(_state->nodeFrame) ? _state->nodeFrame : nullptr;
                _state->swappingGeometry = geometry;
            }
        },
        Qt::DirectConnection));
    _windowConnections.append(connect(
        surface, &QQuickWindow::frameSwapped, this,
        [this, surface]() {
            {
                const QMutexLocker lock(&_state->mutex);
                if (_state->window != surface) {
                    return;
                }
                _state->presented = _state->current(_state->swapping) ? _state->swapping : nullptr;
                _state->presentedGeometry = _state->swappingGeometry;
            }
            QMetaObject::invokeMethod(this, &PixEagleVideoItem::_publishProperties, Qt::QueuedConnection);
        },
        Qt::DirectConnection));
    const auto invalidate = [this, surface]() {
        {
            const QMutexLocker lock(&_state->mutex);
            if (_state->window != surface) {
                return;
            }
            _state->active = false;
            _state->invalidate();
        }
        QMetaObject::invokeMethod(this, &PixEagleVideoItem::_refreshSurface, Qt::QueuedConnection);
    };
    _windowConnections.append(
        connect(surface, &QQuickWindow::sceneGraphInvalidated, this, invalidate, Qt::DirectConnection));
    _windowConnections.append(
        connect(surface, &QQuickWindow::sceneGraphAboutToStop, this, invalidate, Qt::DirectConnection));
    _windowConnections.append(connect(surface, &QQuickWindow::sceneGraphError, this, invalidate));
    _refreshSurface();
}

bool PixEagleVideoItem::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _attachedWindow && (event->type() == QEvent::Expose || event->type() == QEvent::PlatformSurface)) {
        _invalidateSurface();
        QMetaObject::invokeMethod(this, &PixEagleVideoItem::_refreshSurface, Qt::QueuedConnection);
    }
    return QQuickItem::eventFilter(watched, event);
}

void PixEagleVideoItem::releaseResources()
{
    _invalidateSurface();
}

void PixEagleVideoItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);
    if (_state && (change == ItemTransformHasChanged || change == ItemDevicePixelRatioHasChanged)) {
        _refreshSurface();
    }
}

QVariantMap PixEagleVideoItem::capturePresentedContext() const
{
    if (QThread::currentThread() == thread() && !_surfaceUsable()) {
        return {};
    }
    const QMutexLocker lock(&_state->mutex);
    const auto frame = _state->presented;
    if (!_state->current(frame) || frame->context.isEmpty() || frame->image.isNull()) {
        return {};
    }
    auto context = frame->context;
    context.insert(QStringLiteral("presentation_known"), true);
    context.insert(QStringLiteral("frame_fresh"), freshFrame(*frame));
    context.insert(QStringLiteral("display_width"), frame->image.width());
    context.insert(QStringLiteral("display_height"), frame->image.height());
    return context;
}

bool PixEagleVideoItem::presentationKnown() const
{
    return !capturePresentedContext().isEmpty();
}

bool PixEagleVideoItem::frameFresh() const
{
    return capturePresentedContext().value(QStringLiteral("frame_fresh")).toBool();
}

QSize PixEagleVideoItem::sourceSize() const
{
    const auto context = capturePresentedContext();
    return context.isEmpty() ? QSize()
                             : QSize(context.value(QStringLiteral("display_width")).toInt(),
                                     context.value(QStringLiteral("display_height")).toInt());
}

QVariantMap PixEagleVideoItem::displayedContext() const
{
    return capturePresentedContext();
}

bool PixEagleVideoItem::selectionReady() const
{
    if (QThread::currentThread() != thread() || !_surfaceUsable()) {
        return false;
    }
    const auto geometry = renderGeometry(this);
    const QMutexLocker lock(&_state->mutex);
    const auto frame = _state->presented;
    return geometry.valid && geometry == _state->presentedGeometry && _state->current(frame) &&
           !frame->image.isNull() && !frame->selectionGeometry.isEmpty() && freshFrame(*frame);
}

bool PixEagleVideoItem::selectionActive() const
{
    const QMutexLocker lock(&_state->mutex);
    return _state->selection.has_value();
}

QString PixEagleVideoItem::selectionError() const
{
    const QMutexLocker lock(&_state->mutex);
    return _state->selectionError;
}

QString PixEagleVideoItem::beginSelection(QQuickItem* inputItem, const QPointF& start)
{
    if (QThread::currentThread() != thread()) {
        return {};
    }
    const auto geometry = renderGeometry(this);
    const bool usable = _surfaceUsable();
    QString token;
    {
        const QMutexLocker lock(&_state->mutex);
        _state->selection.reset();
        _lastSelectionInvalidation = _state->selectionInvalidation;
        const auto frame = _state->presented;
        if (!usable || !_state->current(frame) || frame->image.isNull()) {
            _state->selectionError = tr("Wait for live video before selecting a target.");
        } else if (frame->selectionGeometry.isEmpty()) {
            _state->selectionError = tr("This video does not provide verified selection geometry.");
        } else if (!freshFrame(*frame)) {
            _state->selectionError = tr("The displayed frame is too old. Select again when video is live.");
        } else if (!geometry.valid || geometry != _state->presentedGeometry || !inputItem ||
                   inputItem->window() != window() || !inputItem->isVisible() || !inputItem->isEnabled()) {
            _state->selectionError = tr("The video layout is changing. Select again.");
        } else {
            Selection selection;
            selection.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
            selection.frame = frame;
            selection.geometry = geometry;
            selection.inputItem = inputItem;
            selection.inputToItem = inputItem->itemTransform(this, nullptr);
            selection.start = start;
            if (!axisAligned(selection.inputToItem)) {
                _state->selectionError = tr("This video layout cannot be used for target selection.");
            } else if (!encodedPoint(selection, start)) {
                _state->selectionError = tr("Select inside the video image.");
            } else {
                token = selection.token;
                _state->selection = std::move(selection);
                _state->selectionError.clear();
            }
        }
    }
    _publishProperties();
    return token;
}

QVariantMap PixEagleVideoItem::finishSelection(const QString& token, const QPointF& end, bool rectangle)
{
    const auto geometry = renderGeometry(this);
    const bool usable = _surfaceUsable();
    QVariantMap result;
    {
        const QMutexLocker lock(&_state->mutex);
        QString error;
        if (!_state->selection || _state->selection->token != token || token.isEmpty()) {
            error = _state->selectionError.isEmpty() ? tr("This selection is no longer active. Select again.")
                                                     : _state->selectionError;
        } else {
            const auto selection = std::move(*_state->selection);
            _state->selection.reset();
            if (!usable || !_state->current(selection.frame) || !_state->current(_state->presented) ||
                _state->presented->image.isNull()) {
                error = tr("Video changed. Select again.");
            } else if (!freshFrame(*selection.frame)) {
                error = tr("The selection took too long. Select again on a live frame.");
            } else if (geometry != selection.geometry || geometry != _state->presentedGeometry ||
                       !selection.inputItem || selection.inputItem->window() != window() ||
                       !selection.inputItem->isVisible() || !selection.inputItem->isEnabled() ||
                       selection.inputItem->itemTransform(this, nullptr) != selection.inputToItem) {
                error = tr("Video layout changed. Select again.");
            } else {
                const auto start = encodedPoint(selection, selection.start);
                const auto finish = encodedPoint(selection, end);
                if (!start || !finish) {
                    error = tr("Keep the whole selection inside the video image.");
                } else if (rectangle &&
                           (qFuzzyCompare(start->x(), finish->x()) || qFuzzyCompare(start->y(), finish->y()))) {
                    error = tr("Draw a rectangle with a width and height.");
                } else {
                    result.insert(QStringLiteral("valid"), true);
                    auto context = selection.frame->context;
                    context.remove(QStringLiteral("pts_us"));
                    result.insert(QStringLiteral("context"), context);
                    result.insert(QStringLiteral("selection_geometry"),
                                  selection.frame->selectionGeometry.toVariantMap());
                    if (rectangle) {
                        const auto bounds = QRectF(*start, *finish).normalized();
                        result.insert(QStringLiteral("rectangle"),
                                      QVariantMap{{QStringLiteral("x"), bounds.x()},
                                                  {QStringLiteral("y"), bounds.y()},
                                                  {QStringLiteral("width"), bounds.width()},
                                                  {QStringLiteral("height"), bounds.height()}});
                    } else {
                        result.insert(QStringLiteral("point"), QVariantMap{{QStringLiteral("x"), start->x()},
                                                                           {QStringLiteral("y"), start->y()}});
                    }
                }
            }
        }
        _state->selectionError = error;
        if (result.isEmpty()) {
            result = {{QStringLiteral("valid"), false}, {QStringLiteral("reason"), error}};
        }
    }
    _publishProperties();
    return result;
}

void PixEagleVideoItem::cancelSelection()
{
    {
        const QMutexLocker lock(&_state->mutex);
        _state->cancelSelection(tr("Selection cancelled."));
    }
    _publishProperties();
}

void PixEagleVideoItem::_publishProperties()
{
    {
        const QMutexLocker lock(&_state->mutex);
        if (_state->selection && !freshFrame(*_state->selection->frame)) {
            _state->cancelSelection(tr("The selection took too long. Select again on a live frame."));
        }
    }
    const auto context = capturePresentedContext();
    const auto size = sourceSize();
    if (context != _lastContext || size != _lastSize) {
        _lastContext = context;
        _lastSize = size;
        emit presentationChanged();
    }
    const bool ready = selectionReady();
    const bool active = selectionActive();
    const QString error = selectionError();
    quint64 invalidation;
    {
        const QMutexLocker lock(&_state->mutex);
        invalidation = _state->selectionInvalidation;
    }
    if (ready != _lastSelectionReady || active != _lastSelectionActive || error != _lastSelectionError) {
        _lastSelectionReady = ready;
        _lastSelectionActive = active;
        _lastSelectionError = error;
        emit selectionChanged();
    }
    if (invalidation != _lastSelectionInvalidation) {
        _lastSelectionInvalidation = invalidation;
        emit selectionInvalidated();
    }
}
