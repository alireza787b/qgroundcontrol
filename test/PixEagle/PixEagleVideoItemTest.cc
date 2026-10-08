#include "PixEagleVideoItemTest.h"

#include <atomic>
#include <cstring>
#include <gst/gst.h>
#include <limits>

#include <QtCore/QBuffer>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QMutexLocker>
#include <QtCore/QTextStream>
#include <QtGui/QPainter>
#include <QtMultimedia/QVideoFrame>
#include <QtQuick/QQuickWindow>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include "GStreamer.h"
#include "GstSourceFactory.h"
#include "PixEagleClient.h"
#include "PixEagleVideoItem.h"
#include "QGCVideoFrameContextStore.h"
#include "QGCWebSocketVideoSource.h"

namespace {
constexpr int IMAGE_WIDTH = 32;
constexpr int IMAGE_HEIGHT = 24;

QJsonObject expectedContext()
{
    return {{"instance_id", "companion-one"},
            {"runtime_id", "runtime-one"},
            {"video", QJsonObject{{"stream_id", "processed"},
                                  {"stream_epoch", "stream-one"},
                                  {"source_epoch", "camera-one"},
                                  {"variant", "processed_osd"},
                                  {"width", IMAGE_WIDTH},
                                  {"height", IMAGE_HEIGHT}}}};
}

QJsonObject metadata(int number, const QJsonObject& expected)
{
    auto provenance = expected.value("video").toObject();
    provenance.remove("width");
    provenance.remove("height");
    provenance.insert("version", "1");
    provenance.insert("instance_id", expected.value("instance_id"));
    provenance.insert("runtime_id", expected.value("runtime_id"));
    provenance.insert("frame_id", QString::number(number));
    provenance.insert("capture_id", QString::number(number));
    provenance.insert("capture_state", "fresh");
    provenance.insert("capture_age_ms", 0);
    provenance.insert("publication_age_ms", 0);
    provenance.insert("encoded_width", IMAGE_WIDTH);
    provenance.insert("encoded_height", IMAGE_HEIGHT);
    provenance.insert("geometry_verified", false);
    return {{"type", "frame"}, {"frame_id", number}, {"provenance", provenance}};
}

QJsonObject selectionMetadata(int number, const QJsonObject& expected)
{
    auto envelope = metadata(number, expected);
    envelope.insert("selection_geometry", QJsonObject{{"version", "1"},
                                                      {"verified", true},
                                                      {"geometry_id", "geometry-one"},
                                                      {"mapping", "full_frame_scale"},
                                                      {"encoded_width", IMAGE_WIDTH},
                                                      {"encoded_height", IMAGE_HEIGHT},
                                                      {"analysis_width", IMAGE_WIDTH * 2},
                                                      {"analysis_height", IMAGE_HEIGHT * 2},
                                                      {"target_revision", "7"},
                                                      {"token", QStringLiteral("retained-frame-%1").arg(number)},
                                                      {"max_age_ms", 1500}});
    return envelope;
}

QImage numberedImage(int number, const QColor& color)
{
    QImage image(IMAGE_WIDTH, IMAGE_HEIGHT, QImage::Format_RGBA8888);
    image.fill(color);
    QPainter painter(&image);
    painter.setPen(Qt::black);
    painter.drawText(image.rect(), Qt::AlignCenter, QString::number(number));
    return image;
}

class FailingTextureItem : public PixEagleVideoItem
{
public:
    using PixEagleVideoItem::PixEagleVideoItem;
    std::atomic_bool failTexture = false;

protected:
    QSGTexture* createFrameTexture(QQuickWindow* window, const QImage& image) override
    {
        return failTexture.load() ? nullptr : PixEagleVideoItem::createFrameTexture(window, image);
    }
};

struct Surface
{
    QQuickWindow window;
    FailingTextureItem item{window.contentItem()};
    std::shared_ptr<QGCVideoFrameContextStore> store = std::make_shared<QGCVideoFrameContextStore>(2);
    QJsonObject expected = expectedContext();
    quint64 epoch = store->beginEpoch();

    bool expose()
    {
        window.setColor(Qt::black);
        window.resize(128, 96);
        item.setSize(QSizeF(128, 96));
        window.show();
        if (!QTest::qWaitForWindowExposed(&window, TestTimeout::mediumMs())) {
            return false;
        }
        QCoreApplication::processEvents();
        item.setStream(store, expected);
        return true;
    }

    void submit(int number, const QColor& color, QJsonObject envelope = {})
    {
        QVideoFrame video(numberedImage(number, color));
        video.setStartTime(store->insert(epoch, envelope.isEmpty() ? metadata(number, expected) : envelope));
        item.videoSink()->setVideoFrame(video);
    }

    QString presentedNumber() const
    {
        return item.capturePresentedContext().value(QStringLiteral("frame_id")).toString();
    }

    QColor pixel() { return window.grabWindow().pixelColor(8, 8); }
};

class NativeSurfacePipeline
{
public:
    ~NativeSurfacePipeline()
    {
        if (_pipeline) {
            GStreamer::SourceFactory::deactivate(_source);
            gst_element_set_state(_pipeline, GST_STATE_NULL);
            gst_object_unref(_pipeline);
        }
    }

    bool start(const QString& url, Surface& surface, const QByteArray& cookie = "session=surface-test",
               const QString& origin = QStringLiteral("https://surface.example.test"))
    {
        auto options = std::make_shared<QGCWebSocketVideoOptions>();
        options->cookie = cookie;
        options->origin = origin;
        options->requireFrameMetadata = true;
        options->frameContexts = surface.store;
        GStreamer::SourceFactory::Config config;
        config.webSocketOptions = options;
        _source = GStreamer::SourceFactory::create(url, config);
        auto* decoder = gst_element_factory_make("jpegdec", nullptr);
        auto* sink = static_cast<GstElement*>(GStreamer::createVideoSink({}));
        _pipeline = gst_pipeline_new(nullptr);
        if (!_pipeline || !_source || !decoder || !sink) {
            gst_clear_object(&_source);
            gst_clear_object(&decoder);
            gst_clear_object(&sink);
            gst_clear_object(&_pipeline);
            return false;
        }
        gst_bin_add_many(GST_BIN(_pipeline), _source, decoder, sink, nullptr);
        return gst_element_link_many(_source, decoder, sink, nullptr) &&
               GStreamer::setupQVideoSinkElement(sink, surface.item.videoSink(), &_controllerOwner) &&
               gst_element_set_state(_pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE &&
               GStreamer::SourceFactory::activate(_source);
    }

private:
    QObject _controllerOwner;
    GstElement* _pipeline = nullptr;
    GstElement* _source = nullptr;
};

QByteArray codedJpeg(int number)
{
    auto image = numberedImage(number, Qt::white);
    QPainter painter(&image);
    for (int bit = 0; bit < 8; ++bit) {
        painter.fillRect(bit * 4, 0, 4, 8, number & (1 << bit) ? Qt::white : Qt::black);
    }
    painter.end();
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    if (!image.save(&buffer, "JPEG", 100)) {
        return {};
    }
    return jpeg;
}

int renderedCode(QQuickWindow& window)
{
    const QImage image = window.grabWindow();
    const qreal ratio = qreal(image.width()) / window.width();
    int result = 0;
    for (int bit = 0; bit < 8; ++bit) {
        if (qGray(image.pixel(qRound((2 + bit * 4) * 4 * ratio), qRound(16 * ratio))) > 127) {
            result |= 1 << bit;
        }
    }
    return result;
}
}  // namespace

void PixEagleVideoItemTest::_renderedPixelsMatchContextAfterCoalescing()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red);
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("1"), TestTimeout::mediumMs());
    QCOMPARE(surface.pixel(), QColor(Qt::red));

    QMutex mutex;
    QVariantMap directCapture;
    const auto connection = connect(
        &surface.window, &QQuickWindow::frameSwapped, &surface.item,
        [&]() {
            const QMutexLocker lock(&mutex);
            directCapture = surface.item.capturePresentedContext();
        },
        Qt::DirectConnection);
    for (int number = 2; number <= 20; ++number) {
        surface.submit(number, number == 20 ? Qt::green : Qt::blue);
    }
    // The selected GUI candidate and received metadata must not replace an unswapped presentation.
    QCOMPARE(surface.presentedNumber(), QStringLiteral("1"));
    // Eviction after the sink callback must not erase the immutable metadata paired with its decoded frame.
    for (int number = 21; number <= 25; ++number) {
        surface.store->insert(surface.epoch, metadata(number, surface.expected));
    }
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("20"), TestTimeout::mediumMs());
    QCOMPARE(surface.pixel(), QColor(Qt::green));
    {
        const QMutexLocker lock(&mutex);
        QCOMPARE(directCapture.value(QStringLiteral("frame_id")).toString(), QStringLiteral("20"));
    }
    disconnect(connection);
}

void PixEagleVideoItemTest::_streamAndStoreEpochsDiscardOldFrames()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(7, Qt::red);
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("7"), TestTimeout::mediumMs());
    auto* oldSink = surface.item.videoSink();
    surface.submit(8, Qt::yellow);
    surface.expected.insert("runtime_id", "runtime-two");
    surface.item.setStream(surface.store, surface.expected);
    QVERIFY(surface.item.videoSink() != oldSink);
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    QVERIFY(surface.item.sourceSize().isEmpty());
    QVideoFrame delayed(numberedImage(9, Qt::magenta));
    delayed.setStartTime(surface.store->insert(surface.epoch, metadata(9, expectedContext())));
    oldSink->setVideoFrame(delayed);
    surface.submit(1, Qt::blue);
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("1"), TestTimeout::mediumMs());
    QCOMPARE(surface.pixel(), QColor(Qt::blue));
    QCOMPARE(surface.item.displayedContext().value(QStringLiteral("runtime_id")).toString(), QString("runtime-two"));

    surface.store->endEpoch(surface.epoch);
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    surface.epoch = surface.store->beginEpoch();
    surface.submit(1, Qt::green);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    QCOMPARE(surface.presentedNumber(), QStringLiteral("1"));
    QCOMPARE(surface.pixel(), QColor(Qt::green));
    surface.item.clearStream();
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    QVERIFY(surface.item.sourceSize().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(surface.pixel(), QColor(Qt::black), TestTimeout::mediumMs());
}

void PixEagleVideoItemTest::_invalidMetadataBlanks_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QJsonValue>("value");
    QTest::newRow("runtime") << QString("runtime_id") << QJsonValue("other-runtime");
    QTest::newRow("instance") << QString("instance_id") << QJsonValue("other-instance");
    QTest::newRow("stream") << QString("stream_id") << QJsonValue("other-stream");
    QTest::newRow("stream-epoch") << QString("stream_epoch") << QJsonValue("other-epoch");
    QTest::newRow("source-epoch") << QString("source_epoch") << QJsonValue("other-camera");
    QTest::newRow("dimensions") << QString("encoded_width") << QJsonValue(IMAGE_WIDTH + 1);
    QTest::newRow("frame-id-type") << QString("frame_id") << QJsonValue(2);
    QTest::newRow("negative-age") << QString("publication_age_ms") << QJsonValue(-1);
    QTest::newRow("unknown-geometry-contract") << QString("geometry_verified") << QJsonValue(true);
    QTest::newRow("missing-provenance") << QString() << QJsonValue();
}

void PixEagleVideoItemTest::_invalidMetadataBlanks()
{
    QFETCH(QString, field);
    QFETCH(QJsonValue, value);
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    auto envelope = metadata(2, surface.expected);
    auto provenance = envelope.value("provenance").toObject();
    provenance.insert(field, value);
    envelope.insert("provenance", field.isEmpty() ? QJsonObject() : provenance);
    surface.submit(2, Qt::yellow, envelope);
    QTRY_VERIFY_WITH_TIMEOUT(!surface.item.presentationKnown(), TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(surface.pixel(), QColor(Qt::black), TestTimeout::mediumMs());
}

void PixEagleVideoItemTest::_negotiatedDeliverySize_data()
{
    QTest::addColumn<QSize>("deliveredSize");
    QTest::addColumn<QString>("scalingVersion");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("legacy-exact") << QSize(32, 24) << QString() << true;
    QTest::newRow("legacy-smaller") << QSize(16, 12) << QString() << false;
    QTest::newRow("unknown-version") << QSize(16, 12) << QString("2") << false;
    QTest::newRow("negotiated-exact") << QSize(32, 24) << QString("1") << true;
    QTest::newRow("negotiated-half") << QSize(16, 12) << QString("1") << true;
    QTest::newRow("negotiated-rounded") << QSize(21, 16) << QString("1") << true;
    QTest::newRow("oversized") << QSize(64, 48) << QString("1") << false;
    QTest::newRow("distorted") << QSize(16, 16) << QString("1") << false;
    QTest::newRow("one-pixel-distortion") << QSize(32, 23) << QString("1") << false;
}

void PixEagleVideoItemTest::_negotiatedDeliverySize()
{
    QFETCH(QSize, deliveredSize);
    QFETCH(QString, scalingVersion);
    QFETCH(bool, accepted);
    Surface surface;
    auto expectedVideo = surface.expected.value("video").toObject();
    if (!scalingVersion.isEmpty()) {
        expectedVideo.insert("delivery_scaling_version", scalingVersion);
    }
    surface.expected.insert("video", expectedVideo);
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red, selectionMetadata(1, surface.expected));
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.selectionReady(), TestTimeout::mediumMs());
    auto envelope = selectionMetadata(2, surface.expected);
    for (const auto* field : {"provenance", "selection_geometry"}) {
        auto context = envelope.value(field).toObject();
        context.insert("encoded_width", deliveredSize.width());
        context.insert("encoded_height", deliveredSize.height());
        envelope.insert(field, context);
    }
    QImage image(deliveredSize, QImage::Format_RGBA8888);
    image.fill(Qt::green);
    QVideoFrame video(image);
    video.setStartTime(surface.store->insert(surface.epoch, envelope));
    surface.item.videoSink()->setVideoFrame(video);
    if (!accepted) {
        QTRY_VERIFY_WITH_TIMEOUT(!surface.item.presentationKnown(), TestTimeout::mediumMs());
        QVERIFY(!surface.item.selectionReady());
        return;
    }
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("2"), TestTimeout::mediumMs());
    QVERIFY(surface.item.selectionReady());
    const auto token = surface.item.beginSelection(&surface.item, {64, 48});
    QVERIFY(!token.isEmpty());
    const auto selection = surface.item.finishSelection(token, {64, 48}, false);
    QVERIFY(selection.value("valid").toBool());
    const auto geometry = selection.value("selection_geometry").toMap();
    QCOMPARE(geometry.value("encoded_width").toInt(), deliveredSize.width());
    QCOMPARE(geometry.value("encoded_height").toInt(), deliveredSize.height());
    QCOMPARE(geometry.value("analysis_width").toInt(), IMAGE_WIDTH * 2);
    QCOMPARE(geometry.value("token").toString(), QStringLiteral("retained-frame-2"));
    const auto point = selection.value("point").toMap();
    QCOMPARE(point.value("x").toDouble(), 0.5);
    QCOMPARE(point.value("y").toDouble(), 0.5);
}

void PixEagleVideoItemTest::_freshness_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<int>("captureAge");
    QTest::addColumn<int>("publicationAge");
    QTest::addColumn<bool>("fresh");
    QTest::newRow("fresh") << QString("fresh") << 0 << 0 << true;
    QTest::newRow("cached") << QString("cached") << 0 << 0 << false;
    QTest::newRow("unknown") << QString("unknown") << 0 << 0 << false;
    QTest::newRow("unavailable") << QString("unavailable") << 0 << 0 << false;
    QTest::newRow("old-capture") << QString("fresh") << 1600 << 0 << false;
    QTest::newRow("old-publication") << QString("fresh") << 0 << 1600 << false;
    QTest::newRow("ages-not-added") << QString("fresh") << 800 << 800 << true;
}

void PixEagleVideoItemTest::_freshness()
{
    QFETCH(QString, state);
    QFETCH(int, captureAge);
    QFETCH(int, publicationAge);
    QFETCH(bool, fresh);
    Surface surface;
    QVERIFY(surface.expose());
    auto envelope = selectionMetadata(1, surface.expected);
    auto provenance = envelope.value("provenance").toObject();
    provenance.insert("capture_state", state);
    provenance.insert("capture_age_ms", captureAge);
    provenance.insert("publication_age_ms", publicationAge);
    envelope.insert("provenance", provenance);
    surface.submit(1, Qt::red, envelope);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    QCOMPARE(surface.item.frameFresh(), fresh);
    QCOMPARE(surface.item.selectionReady(), fresh);
    if (!fresh) {
        QVERIFY(surface.item.beginSelection(&surface.item, {64, 48}).isEmpty());
    }
    QCOMPARE(surface.presentedNumber(), QStringLiteral("1"));
    QCOMPARE(surface.pixel(), QColor(Qt::red));
}

void PixEagleVideoItemTest::_freshnessExpiresWithoutChangingIdentity()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.frameFresh(), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(!surface.item.frameFresh(), TestTimeout::mediumMs());
    QVERIFY(surface.item.presentationKnown());
    QCOMPARE(surface.presentedNumber(), QStringLiteral("1"));
    QCOMPARE(surface.pixel(), QColor(Qt::red));
}

void PixEagleVideoItemTest::_surfaceChangesInvalidatePresentation_data()
{
    QTest::addColumn<QString>("change");
    QTest::newRow("item-hidden") << QString("item");
    QTest::newRow("window-hidden") << QString("window");
    QTest::newRow("reparent-window") << QString("reparent");
    QTest::newRow("ancestor-hidden") << QString("ancestor");
    QTest::newRow("item-offscreen") << QString("item-position");
    QTest::newRow("ancestor-offscreen") << QString("ancestor-position");
    QTest::newRow("ancestor-fully-clipped") << QString("ancestor-clip");
    QTest::newRow("zero-scale") << QString("scale");
}

void PixEagleVideoItemTest::_surfaceChangesInvalidatePresentation()
{
    QFETCH(QString, change);
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    surface.submit(2, Qt::yellow);
    QQuickItem clippingParent(surface.window.contentItem());
    clippingParent.setSize(QSizeF(128, 96));
    surface.item.setParentItem(&clippingParent);
    QQuickWindow otherWindow;
    otherWindow.setColor(Qt::black);
    otherWindow.resize(128, 96);
    if (change == "item") {
        surface.item.setVisible(false);
    } else if (change == "window") {
        surface.window.hide();
    } else if (change == "ancestor") {
        clippingParent.setVisible(false);
    } else if (change == "item-position") {
        surface.item.setX(surface.window.width());
    } else if (change == "ancestor-position") {
        clippingParent.setX(surface.window.width());
    } else if (change == "ancestor-clip") {
        clippingParent.setClip(true);
        clippingParent.setWidth(0);
    } else if (change == "scale") {
        surface.item.setScale(0);
    } else {
        otherWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&otherWindow, TestTimeout::mediumMs()));
        surface.item.setParentItem(otherWindow.contentItem());
    }
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    QCoreApplication::processEvents();
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    if (change == "item") {
        surface.item.setVisible(true);
    } else if (change == "window") {
        surface.window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&surface.window, TestTimeout::mediumMs()));
    } else if (change == "ancestor") {
        clippingParent.setVisible(true);
    } else if (change == "item-position") {
        surface.item.setX(0);
    } else if (change == "ancestor-position") {
        clippingParent.setX(0);
    } else if (change == "ancestor-clip") {
        clippingParent.setWidth(128);
    } else if (change == "scale") {
        surface.item.setScale(1);
    }
    QCoreApplication::processEvents();
    QVERIFY(surface.item.capturePresentedContext().isEmpty());
    surface.submit(3, Qt::green);
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("3"), TestTimeout::mediumMs());
    auto* window = change == "reparent" ? &otherWindow : &surface.window;
    QCOMPARE(window->grabWindow().pixelColor(8, 8), QColor(Qt::green));
    // Keep the stack-owned item out of otherWindow's child destruction.
    surface.item.setParentItem(surface.window.contentItem());
}

void PixEagleVideoItemTest::_textureFailureCannotPublishNewContext()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    surface.item.failTexture = true;
    surface.submit(2, Qt::green);
    QTRY_VERIFY_WITH_TIMEOUT(!surface.item.presentationKnown(), TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(surface.pixel(), QColor(Qt::black), TestTimeout::mediumMs());
    surface.item.failTexture = false;
    surface.submit(3, Qt::blue);
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("3"), TestTimeout::mediumMs());
    QCOMPARE(surface.pixel(), QColor(Qt::blue));
}

void PixEagleVideoItemTest::_rotationAndMirroring_data()
{
    QTest::addColumn<int>("surfaceRotation");
    QTest::addColumn<bool>("surfaceMirror");
    QTest::addColumn<int>("frameRotation");
    QTest::addColumn<bool>("frameMirror");
    QTest::addColumn<bool>("bottomToTop");
    QTest::addColumn<QSize>("size");
    QTest::addColumn<QList<QColor>>("corners");
    const QColor red(Qt::red), green(Qt::green), blue(Qt::blue), yellow(Qt::yellow);
    QTest::newRow("identity") << 0 << false << 0 << false << false << QSize(32, 24)
                              << QList<QColor>{red, green, blue, yellow};
    QTest::newRow("frame-90") << 0 << false << 90 << false << false << QSize(24, 32)
                              << QList<QColor>{blue, red, yellow, green};
    QTest::newRow("surface-90") << 90 << false << 0 << false << false << QSize(24, 32)
                                << QList<QColor>{blue, red, yellow, green};
    QTest::newRow("surface-and-frame-90")
        << 90 << false << 90 << false << false << QSize(32, 24) << QList<QColor>{yellow, blue, green, red};
    QTest::newRow("frame-mirror") << 0 << false << 0 << true << false << QSize(32, 24)
                                  << QList<QColor>{green, red, yellow, blue};
    QTest::newRow("surface-mirror-between-rotations")
        << 90 << true << 90 << false << false << QSize(32, 24) << QList<QColor>{green, red, yellow, blue};
    QTest::newRow("bottom-to-top") << 0 << false << 0 << false << true << QSize(32, 24)
                                   << QList<QColor>{blue, yellow, red, green};
    QTest::newRow("bottom-to-top-and-surface-90")
        << 90 << false << 0 << false << true << QSize(24, 32) << QList<QColor>{red, blue, green, yellow};
}

void PixEagleVideoItemTest::_rotationAndMirroring()
{
    QFETCH(int, surfaceRotation);
    QFETCH(bool, surfaceMirror);
    QFETCH(int, frameRotation);
    QFETCH(bool, frameMirror);
    QFETCH(bool, bottomToTop);
    QFETCH(QSize, size);
    QFETCH(QList<QColor>, corners);
    Surface surface;
    QVERIFY(surface.expose());
    QImage image(32, 24, QImage::Format_RGBA8888);
    QPainter painter(&image);
    painter.fillRect(0, 0, 16, 12, Qt::red);
    painter.fillRect(16, 0, 16, 12, Qt::green);
    painter.fillRect(0, 12, 16, 12, Qt::blue);
    painter.fillRect(16, 12, 16, 12, Qt::yellow);
    painter.end();
    QVideoFrameFormat format(image.size(), QVideoFrameFormat::Format_RGBA8888);
    format.setRotation(static_cast<QtVideo::Rotation>(surfaceRotation));
    format.setMirrored(surfaceMirror);
    format.setScanLineDirection(bottomToTop ? QVideoFrameFormat::BottomToTop : QVideoFrameFormat::TopToBottom);
    QVideoFrame video(format);
    QVERIFY(video.map(QVideoFrame::WriteOnly));
    for (int row = 0; row < image.height(); ++row) {
        std::memcpy(video.bits(0) + row * video.bytesPerLine(0), image.constScanLine(row), image.bytesPerLine());
    }
    video.unmap();
    video.setRotation(static_cast<QtVideo::Rotation>(frameRotation));
    video.setMirrored(frameMirror);
    video.setStartTime(surface.store->insert(surface.epoch, selectionMetadata(1, surface.expected)));
    surface.item.videoSink()->setVideoFrame(video);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    QCOMPARE(surface.item.sourceSize(), size);
    const auto rendered = surface.window.grabWindow();
    const qreal ratio = qreal(rendered.width()) / surface.window.width();
    const QList<QColor> actual{rendered.pixelColor(qRound(8 * ratio), qRound(8 * ratio)),
                               rendered.pixelColor(qRound(120 * ratio), qRound(8 * ratio)),
                               rendered.pixelColor(qRound(8 * ratio), qRound(88 * ratio)),
                               rendered.pixelColor(qRound(120 * ratio), qRound(88 * ratio))};
    QCOMPARE(actual, corners);
    QCOMPARE(surface.presentedNumber(), QStringLiteral("1"));
    QVERIFY(surface.item.selectionReady());
    const auto token = surface.item.beginSelection(&surface.item, {32, 24});
    QVERIFY(!token.isEmpty());
    const auto result = surface.item.finishSelection(token, {32, 24}, false);
    QVERIFY(result.value("valid").toBool());
    const auto point = result.value("point").toMap();
    const auto color = corners.first();
    QCOMPARE(point.value("x").toDouble(), color == Qt::red || color == Qt::blue ? 0.25 : 0.75);
    QCOMPARE(point.value("y").toDouble(), color == Qt::red || color == Qt::green ? 0.25 : 0.75);
}

void PixEagleVideoItemTest::_nativeJpegDecodeRetainsPresentedIdentity()
{
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
    QSKIP("GStreamer init deadlocks under AddressSanitizer (bindtextdomain lock)");
#endif
    if (!gst_is_initialized()) {
        GStreamer::prepareEnvironment();
        QVERIFY(gst_init_check(nullptr, nullptr, nullptr));
    }
    QVERIFY(GStreamer::completeInit());
    Surface surface;
    QVERIFY(surface.expose());
    QWebSocketServer server(QStringLiteral("Surface provenance test"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QPointer<QWebSocket> peer;
    QString token;
    qint64 acknowledged = -1;
    bool adaptiveDimensions = false;
    connect(&server, &QWebSocketServer::newConnection, &server, [&]() {
        peer = server.nextPendingConnection();
        connect(peer, &QWebSocket::textMessageReceived, &server, [&](const QString& text) {
            const auto message = QJsonDocument::fromJson(text.toUtf8()).object();
            if (message.contains("delivery_token")) {
                token = message.value("delivery_token").toString();
            }
            if (message.value("type").toString() == "stream_capabilities") {
                adaptiveDimensions = message.value("adaptive_dimensions").toBool();
            }
            if (message.value("type").toString() == "frame_ack") {
                acknowledged = message.value("frame_id").toInteger(-1);
            }
        });
    });
    NativeSurfacePipeline pipeline;
    QVERIFY(pipeline.start(QStringLiteral("ws://127.0.0.1:%1/ws/video_feed").arg(server.serverPort()), surface));
    QTRY_VERIFY_WITH_TIMEOUT(peer && !token.isEmpty(), TestTimeout::mediumMs());
    QVERIFY(adaptiveDimensions);
    QCOMPARE(peer->request().rawHeader("Cookie"), QByteArray("session=surface-test"));
    QCOMPARE(peer->origin(), QStringLiteral("https://surface.example.test"));
    const auto send = [&](int number, int captureAge = 0) {
        const auto jpeg = codedJpeg(number);
        auto envelope = selectionMetadata(number, surface.expected);
        auto provenance = envelope.value("provenance").toObject();
        provenance.insert("capture_age_ms", captureAge);
        envelope.insert("provenance", provenance);
        envelope.insert("size", jpeg.size());
        envelope.insert("quality", 100);
        envelope.insert("timestamp", 0);
        envelope.insert("frame_age_ms", 0);
        envelope.insert("delivery_token", token);
        peer->sendTextMessage(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
        peer->sendBinaryMessage(jpeg);
    };
    for (int number = 1; number <= 8; ++number) {
        send(number);
        QTRY_COMPARE_WITH_TIMEOUT(acknowledged, number, TestTimeout::mediumMs());
    }
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("8"), TestTimeout::mediumMs());
    QCOMPARE(renderedCode(surface.window), 8);
    QVERIFY(surface.item.frameFresh());
    QVERIFY(surface.item.selectionReady());
    const auto selectionToken = surface.item.beginSelection(&surface.item, {64, 48});
    QVERIFY(!selectionToken.isEmpty());
    const auto selection = surface.item.finishSelection(selectionToken, {64, 48}, false);
    QVERIFY(selection.value("valid").toBool());
    QCOMPARE(selection.value("context").toMap().value("frame_id").toString(), QStringLiteral("8"));
    QCOMPARE(selection.value("selection_geometry").toMap().value("token").toString(),
             QStringLiteral("retained-frame-8"));
    const auto context = surface.item.capturePresentedContext();
    const auto entry = surface.store->lookup(context.value(QStringLiteral("pts_us")).toString().toLongLong());
    QVERIFY(entry.has_value());
    QCOMPARE(entry->metadata.value("provenance").toObject().value("capture_id").toString(), QStringLiteral("8"));

    send(9, 1600);
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("9"), TestTimeout::mediumMs());
    QCOMPARE(renderedCode(surface.window), 9);
    QVERIFY(!surface.item.frameFresh());
    peer->close();
    QTRY_VERIFY_WITH_TIMEOUT(!surface.item.presentationKnown(), TestTimeout::mediumMs());
}

void PixEagleVideoItemTest::_selectionRejectsUnverifiedGeometry_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QJsonValue>("value");
    QTest::newRow("absent") << QString() << QJsonValue();
    QTest::newRow("unverified") << QString("verified") << QJsonValue(false);
    QTest::newRow("wrong-verified-type") << QString("verified") << QJsonValue("true");
    QTest::newRow("unknown-version") << QString("version") << QJsonValue("2");
    QTest::newRow("unknown-mapping") << QString("mapping") << QJsonValue("cropped");
    QTest::newRow("wrong-encoded-width") << QString("encoded_width") << QJsonValue(IMAGE_WIDTH + 1);
    QTest::newRow("fractional-width") << QString("encoded_width") << QJsonValue(IMAGE_WIDTH + 0.5);
    QTest::newRow("missing-analysis-height") << QString("analysis_height") << QJsonValue();
    QTest::newRow("missing-geometry-id") << QString("geometry_id") << QJsonValue();
    QTest::newRow("empty-token") << QString("token") << QJsonValue("");
    QTest::newRow("revision-number") << QString("target_revision") << QJsonValue(7);
    QTest::newRow("unknown-age-limit") << QString("max_age_ms") << QJsonValue(3000);
}

void PixEagleVideoItemTest::_selectionRejectsUnverifiedGeometry()
{
    QFETCH(QString, field);
    QFETCH(QJsonValue, value);
    Surface surface;
    QVERIFY(surface.expose());
    auto envelope = selectionMetadata(1, surface.expected);
    auto geometry = envelope.value("selection_geometry").toObject();
    geometry.insert(field, value);
    envelope.insert("selection_geometry", field.isEmpty() ? QJsonObject() : geometry);
    surface.submit(1, Qt::red, envelope);
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.presentationKnown(), TestTimeout::mediumMs());
    QVERIFY(surface.item.frameFresh());
    QVERIFY(!surface.item.selectionReady());
    QVERIFY(surface.item.beginSelection(&surface.item, {64, 48}).isEmpty());
    QVERIFY(!surface.item.selectionError().isEmpty());
    const auto result = surface.item.finishSelection({}, {64, 48}, false);
    QCOMPARE(result.size(), 2);
    QVERIFY(!result.value("valid").toBool());
    QVERIFY(!result.value("reason").toString().isEmpty());
}

void PixEagleVideoItemTest::_selectionPinsFrameAndMapsLetterbox()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.item.setSize({96, 48});
    surface.item.setPosition({16, 24});
    surface.submit(1, Qt::red, selectionMetadata(1, surface.expected));
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.selectionReady(), TestTimeout::mediumMs());
    auto* input = surface.window.contentItem();
    QVERIFY(surface.item.beginSelection(input, {0, 0}).isEmpty());
    const auto token = surface.item.beginSelection(input, {88, 60});
    QVERIFY(!token.isEmpty());
    surface.submit(2, Qt::blue, selectionMetadata(2, surface.expected));
    surface.submit(3, Qt::green, selectionMetadata(3, surface.expected));
    QTRY_COMPARE_WITH_TIMEOUT(surface.presentedNumber(), QStringLiteral("3"), TestTimeout::mediumMs());
    const auto result = surface.item.finishSelection(token, {40, 36}, true);
    QVERIFY(result.value("valid").toBool());
    QCOMPARE(result.value("context").toMap(),
             selectionMetadata(1, surface.expected).value("provenance").toObject().toVariantMap());
    QCOMPARE(result.value("selection_geometry").toMap(),
             selectionMetadata(1, surface.expected).value("selection_geometry").toObject().toVariantMap());
    const auto rectangle = result.value("rectangle").toMap();
    QCOMPARE(rectangle.value("x").toDouble(), 0.25);
    QCOMPARE(rectangle.value("y").toDouble(), 0.25);
    QCOMPARE(rectangle.value("width").toDouble(), 0.5);
    QCOMPARE(rectangle.value("height").toDouble(), 0.5);
    QVERIFY(!surface.item.finishSelection(token, {40, 36}, true).value("valid").toBool());
    const auto outside = surface.item.beginSelection(input, {40, 36});
    const auto rejected = surface.item.finishSelection(outside, {0, 0}, true);
    QCOMPARE(rejected.size(), 2);
    QVERIFY(!rejected.value("valid").toBool());
}

void PixEagleVideoItemTest::_selectionRejectsUnswappedLayout()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red, selectionMetadata(1, surface.expected));
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.selectionReady(), TestTimeout::mediumMs());
    surface.item.setX(1);
    QVERIFY(!surface.item.selectionReady());
    QVERIFY(surface.item.beginSelection(surface.window.contentItem(), {64, 48}).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.selectionReady(), TestTimeout::mediumMs());
    const auto token = surface.item.beginSelection(surface.window.contentItem(), {65, 48});
    QVERIFY(!token.isEmpty());
    const auto point = surface.item.finishSelection(token, {65, 48}, false).value("point").toMap();
    QCOMPARE(point.value("x").toDouble(), 0.5);
    QCOMPARE(point.value("y").toDouble(), 0.5);
}

void PixEagleVideoItemTest::_selectionCancelledByChanges_data()
{
    QTest::addColumn<QString>("change");
    for (const auto* change : {"hide", "resize", "stream", "store-epoch", "geometry", "target-revision",
                               "input-destroyed", "cancel", "nonfinite", "degenerate"}) {
        QTest::newRow(change) << QString::fromLatin1(change);
    }
}

void PixEagleVideoItemTest::_selectionCancelledByChanges()
{
    QFETCH(QString, change);
    Surface surface;
    QVERIFY(surface.expose());
    auto input = std::make_unique<QQuickItem>(surface.window.contentItem());
    input->setSize(surface.item.size());
    surface.submit(1, Qt::red, selectionMetadata(1, surface.expected));
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.selectionReady(), TestTimeout::mediumMs());
    const auto token = surface.item.beginSelection(input.get(), {32, 24});
    QVERIFY(!token.isEmpty());
    QPointF end{96, 72};
    if (change == "hide") {
        surface.item.setVisible(false);
    } else if (change == "resize") {
        surface.item.setWidth(96);
    } else if (change == "stream") {
        surface.item.setStream(surface.store, surface.expected);
    } else if (change == "store-epoch") {
        surface.store->endEpoch(surface.epoch);
    } else if (change == "input-destroyed") {
        input.reset();
    } else if (change == "cancel") {
        surface.item.cancelSelection();
    } else if (change == "nonfinite") {
        end.setX(std::numeric_limits<qreal>::quiet_NaN());
    } else if (change == "degenerate") {
        end.setX(32);
    } else {
        auto envelope = selectionMetadata(2, surface.expected);
        auto geometry = envelope.value("selection_geometry").toObject();
        geometry.insert(change == "geometry" ? "geometry_id" : "target_revision", change == "geometry" ? "other" : "8");
        envelope.insert("selection_geometry", geometry);
        surface.submit(2, Qt::green, envelope);
        QTRY_VERIFY_WITH_TIMEOUT(!surface.item.selectionActive(), TestTimeout::mediumMs());
    }
    const auto result = surface.item.finishSelection(token, end, true);
    QCOMPARE(result.size(), 2);
    QVERIFY(!result.value("valid").toBool());
    QVERIFY(!result.value("reason").toString().isEmpty());
}

void PixEagleVideoItemTest::_selectionAgeDoesNotFollowNewFrames()
{
    Surface surface;
    QVERIFY(surface.expose());
    surface.submit(1, Qt::red, selectionMetadata(1, surface.expected));
    QTRY_VERIFY_WITH_TIMEOUT(surface.item.selectionReady(), TestTimeout::mediumMs());
    const auto token = surface.item.beginSelection(&surface.item, {32, 24});
    QVERIFY(!token.isEmpty());
    int number = 1;
    QTimer publisher;
    publisher.setInterval(50);
    connect(&publisher, &QTimer::timeout, &surface.item, [&]() {
        ++number;
        surface.submit(number, Qt::green, selectionMetadata(number, surface.expected));
    });
    publisher.start();
    QTRY_VERIFY_WITH_TIMEOUT(!surface.item.selectionActive(), TestTimeout::mediumMs());
    QVERIFY(surface.item.frameFresh());
    QVERIFY(surface.presentedNumber() != "1");
    const auto result = surface.item.finishSelection(token, {96, 72}, true);
    QCOMPARE(result.size(), 2);
    QVERIFY(!result.value("valid").toBool());
    QVERIFY(result.value("reason").toString().contains("too long"));
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleVideoItemTest, TestLabel::Integration)

void PixEagleLiveVideoTest::_authenticatedBackendReachesPresentedFrame()
{
    const auto credentialsPath = qEnvironmentVariable("PIXEAGLE_VIDEO_DIAGNOSTIC_CREDENTIALS");
    if (credentialsPath.isEmpty()) {
        QSKIP("Set PIXEAGLE_VIDEO_DIAGNOSTIC_CREDENTIALS to opt into a live read-only backend test");
    }
    if (!gst_is_initialized()) {
        GStreamer::prepareEnvironment();
        QVERIFY(gst_init_check(nullptr, nullptr, nullptr));
    }
    QVERIFY(GStreamer::completeInit());
    QFile credentialsFile(credentialsPath);
    QVERIFY2(credentialsFile.open(QIODevice::ReadOnly), "Could not read private diagnostic credentials file");
    const auto credentials = QJsonDocument::fromJson(credentialsFile.readAll()).object();
    const auto endpoint = credentials.value("endpoint").toString();
    const auto username = credentials.value("username").toString();
    const auto password = credentials.value("password").toString();
    QVERIFY(!endpoint.isEmpty() && !username.isEmpty() && !password.isEmpty());

    PixEagleClient client(nullptr, true);
    client.setRememberSignIn(false);
    client.setEnabled(true);
    Surface surface;
    int decodedFrames = 0;
    QSize decodedSize;
    int decodedHandle = -1;
    const auto report = [&]() {
        const auto presented = surface.item.capturePresentedContext();
        QJsonObject evidence{{"authenticated", client.authenticated()},
                             {"media_available", client.mediaAvailable()},
                             {"decoded_frames", decodedFrames},
                             {"decoded_width", decodedSize.width()},
                             {"decoded_height", decodedSize.height()},
                             {"decoded_handle", decodedHandle},
                             {"presentation_known", surface.item.presentationKnown()},
                             {"frame_fresh", surface.item.frameFresh()},
                             {"instance_id", client.instanceId()},
                             {"runtime_id", client.connectionContext().value("runtime_id")}};
        for (const auto* field :
             {"frame_id", "stream_id", "stream_epoch", "source_epoch", "encoded_width", "encoded_height"}) {
            evidence.insert(QLatin1String(field), QJsonValue::fromVariant(presented.value(QLatin1String(field))));
        }
        const auto serialized = QJsonDocument(evidence).toJson(QJsonDocument::Compact);
        QTextStream(stdout) << "PixEagle live video diagnostic: " << serialized << Qt::endl;
        const auto outputPath = qEnvironmentVariable("PIXEAGLE_VIDEO_DIAGNOSTIC_OUTPUT");
        if (!outputPath.isEmpty()) {
            QFile output(outputPath);
            if (output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                output.write(serialized + '\n');
            }
        }
    };
    client.signInAt(endpoint, username, password);
    const bool authenticated = UnitTest::waitForCondition(
        [&]() { return client.authenticated(); }, TestTimeout::longMs(), QStringLiteral("live backend authentication"));
    report();
    QVERIFY(authenticated);
    const bool mediaReady = UnitTest::waitForCondition([&]() { return client.mediaAvailable(); }, TestTimeout::longMs(),
                                                       QStringLiteral("live backend media context"));
    report();
    QVERIFY(mediaReady);
    surface.expected = client.connectionContext();
    QVERIFY(surface.expose());
    connect(surface.item.videoSink(), &QVideoSink::videoFrameChanged, &client, [&](const QVideoFrame& frame) {
        if (frame.isValid()) {
            ++decodedFrames;
            decodedSize = frame.size();
            decodedHandle = static_cast<int>(frame.handleType());
        }
    });
    NativeSurfacePipeline pipeline;
    QVERIFY(pipeline.start(client.mediaUrl().toString(), surface, client.mediaCookie(), client.mediaOrigin()));
    const bool presented = UnitTest::waitForCondition(
        [&]() { return decodedFrames >= 2 && surface.item.presentationKnown() && surface.item.frameFresh(); },
        TestTimeout::longMs(), QStringLiteral("live native decoded and presented frame"));
    report();
    QVERIFY(presented);
    const auto image = surface.window.grabWindow();
    QVERIFY(!image.isNull());
    const auto imagePath = qEnvironmentVariable("PIXEAGLE_VIDEO_DIAGNOSTIC_IMAGE");
    if (!imagePath.isEmpty()) {
        QVERIFY(image.save(imagePath));
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleLiveVideoTest, TestLabel::Network)
