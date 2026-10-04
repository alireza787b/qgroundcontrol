#include "QGCWebSocketVideoSource.h"

#include <atomic>
#include <cmath>
#include <gst/app/gstappsrc.h>
#include <limits>
#include <utility>

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslError>
#include <QtWebSockets/QWebSocket>

#include "QGCLoggingCategory.h"
#include "QGCNetworkHelper.h"
#include "QGCVideoFrameContextStore.h"

QGC_LOGGING_CATEGORY(QGCWebSocketVideoSourceLog, "Video.GStreamer.WebSocketVideoSource")

namespace {

constexpr int kThreadLifecycleTimeoutMs = 3000;
constexpr qsizetype kMaximumMetadataBytes = 64 * 1024;
constexpr int kMaximumPairingDelayMs = 3000;

bool nonnegativeNumber(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble()) && value.toDouble() >= 0;
}

bool integerInRange(const QJsonValue& value, qint64 minimum, qint64 maximum)
{
    const qint64 integer = value.toInteger(-1);
    return value.isDouble() && integer >= minimum && integer <= maximum && value == QJsonValue(integer);
}

bool isStartOfFrameMarker(quint8 marker)
{
    switch (marker) {
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xCD:
        case 0xCE:
        case 0xCF:
            return true;
        default:
            return false;
    }
}

bool parseSingleJpeg(QByteArrayView message, QSize& dimensions)
{
    const auto* data = reinterpret_cast<const quint8*>(message.data());
    const qsizetype size = message.size();
    if ((size < 4) || (data[0] != 0xFF) || (data[1] != 0xD8)) {
        return false;
    }

    bool foundStartOfFrame = false;
    bool foundStartOfScan = false;
    bool inEntropyData = false;
    qsizetype offset = 2;

    while (offset < size) {
        quint8 marker = 0;

        if (inEntropyData) {
            bool foundMarker = false;
            while (offset < size) {
                if (data[offset++] != 0xFF) {
                    continue;
                }
                while ((offset < size) && (data[offset] == 0xFF)) {
                    ++offset;
                }
                if (offset >= size) {
                    return false;
                }

                marker = data[offset++];
                if ((marker == 0x00) || (marker == 0x01) || ((marker >= 0xD0) && (marker <= 0xD7))) {
                    continue;
                }
                foundMarker = true;
                break;
            }
            if (!foundMarker) {
                return false;
            }
            inEntropyData = false;
        } else {
            if (data[offset++] != 0xFF) {
                return false;
            }
            while ((offset < size) && (data[offset] == 0xFF)) {
                ++offset;
            }
            if (offset >= size) {
                return false;
            }
            marker = data[offset++];
        }

        if (marker == 0xD9) {
            return foundStartOfFrame && foundStartOfScan && (offset == size);
        }
        if ((marker == 0xD8) || (marker == 0x00) || ((marker >= 0xD0) && (marker <= 0xD7))) {
            return false;
        }
        if (marker == 0x01) {
            continue;
        }

        if ((offset + 2) > size) {
            return false;
        }
        const quint16 segmentLength = (static_cast<quint16>(data[offset]) << 8) | data[offset + 1];
        if ((segmentLength < 2) || (static_cast<qsizetype>(segmentLength) > (size - offset))) {
            return false;
        }
        const qsizetype segmentEnd = offset + segmentLength;

        if (isStartOfFrameMarker(marker)) {
            if (foundStartOfFrame || (segmentLength < 8)) {
                return false;
            }
            const quint8 componentCount = data[offset + 7];
            if ((componentCount == 0) || (segmentLength != static_cast<quint16>(8 + (3 * componentCount)))) {
                return false;
            }

            const int height = (static_cast<int>(data[offset + 3]) << 8) | data[offset + 4];
            const int width = (static_cast<int>(data[offset + 5]) << 8) | data[offset + 6];
            dimensions = QSize(width, height);
            foundStartOfFrame = true;
        } else if (marker == 0xDA) {
            if (!foundStartOfFrame || (segmentLength < 6)) {
                return false;
            }
            const quint8 componentCount = data[offset + 2];
            if ((componentCount == 0) || (segmentLength != static_cast<quint16>(6 + (2 * componentCount)))) {
                return false;
            }
            foundStartOfScan = true;
            inEntropyData = true;
        }

        offset = segmentEnd;
    }

    return false;
}

class WebSocketWorker final : public QObject
{
public:
    WebSocketWorker(const QUrl& url, GstElement* appsrc, GstBus* bus,
                    std::shared_ptr<const QGCWebSocketVideoOptions> options)
        : _url(url)
        , _appsrc(appsrc)
        , _bus(bus)
        , _options(std::move(options))
    {}

    ~WebSocketWorker() override { stop(); }

    void start()
    {
        if (_running || !_appsrc || !_accepting.load(std::memory_order_acquire)) {
            return;
        }

        if (_nativeMode()) {
            if (!_options->frameContexts) {
                return;
            }
            _epoch = _options->frameContexts->beginEpoch();
        }
        if (!_accepting.load(std::memory_order_acquire)) {
            _endEpoch();
            return;
        }
        _webSocket = new QWebSocket(_options ? _options->origin : QString(), QWebSocketProtocol::VersionLatest, this);
        _webSocket->setMaxAllowedIncomingFrameSize(static_cast<quint64>(QGCWebSocketVideoSource::kMaximumJpegBytes));
        _webSocket->setMaxAllowedIncomingMessageSize(static_cast<quint64>(QGCWebSocketVideoSource::kMaximumJpegBytes));
        _webSocket->setReadBufferSize(static_cast<qint64>(QGCWebSocketVideoSource::kMaximumJpegBytes));

        if (_url.scheme() == QLatin1String("wss")) {
            _webSocket->setSslConfiguration(QGCNetworkHelper::createSslConfig());
        }

        (void) connect(_webSocket, &QWebSocket::connected, this, [this]() {
            qCDebug(QGCWebSocketVideoSourceLog) << "Connected to WebSocket video source";
            if (_nativeMode()) {
                _sendDeliveryToken(true);
            }
        });
        (void) connect(_webSocket, &QWebSocket::disconnected, this, [this]() {
            qCDebug(QGCWebSocketVideoSourceLog) << "Disconnected from WebSocket video source";
            _failStream(GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ, "WebSocket video connection closed",
                        QStringLiteral("Connection closed unexpectedly"));
        });
        (void) connect(_webSocket, &QWebSocket::binaryMessageReceived, this,
                       [this](const QByteArray& message) { _handleBinaryMessage(message); });
        (void) connect(_webSocket, &QWebSocket::textMessageReceived, this,
                       [this](const QString& message) { _handleTextMessage(message); });
        (void) connect(_webSocket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
            if (!_accepting.load(std::memory_order_acquire) || _terminalError) {
                return;
            }
            qCWarning(QGCWebSocketVideoSourceLog) << "WebSocket transport error code" << static_cast<int>(error);
            _failStream(GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_OPEN_READ, "WebSocket video connection failed",
                        QStringLiteral("Socket error code %1").arg(static_cast<int>(error)));
            if (_webSocket) {
                _webSocket->abort();
            }
        });
        (void) connect(_webSocket, &QWebSocket::sslErrors, this, [](const QList<QSslError>& errors) {
            if (!errors.isEmpty()) {
                qCWarning(QGCWebSocketVideoSourceLog)
                    << "WebSocket TLS verification failed with" << errors.size() << "error(s); first code"
                    << static_cast<int>(errors.constFirst().error());
            }
        });

        QNetworkRequest request(_url);
        request.setRawHeader("User-Agent", QGCNetworkHelper::defaultUserAgent().toUtf8());
        if (_options && !_options->cookie.isEmpty()) {
            request.setRawHeader("Cookie", _options->cookie);
        }
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

        _running = true;
        _terminalError = false;
        qCDebug(QGCWebSocketVideoSourceLog) << "Opening WebSocket video source";
        _webSocket->open(request);
    }

    void stop()
    {
        _accepting.store(false, std::memory_order_release);
        _endEpoch();
        _pendingMetadata = {};
        _running = false;
        _terminalError = true;
        if (_webSocket) {
            _webSocket->disconnect(this);
            _webSocket->abort();
            delete _webSocket;
            _webSocket = nullptr;
        }
    }

    void requestStop()
    {
        _accepting.store(false, std::memory_order_release);
        _endEpoch();
    }

private:
    bool _nativeMode() const { return _options && _options->requireFrameMetadata; }

    void _sendDeliveryToken(bool capabilities = false)
    {
        _deliveryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        _tokenSentMonotonicMs = QGCVideoFrameContextStore::monotonicMs();
        QJsonObject message{{"type", capabilities ? "stream_capabilities" : "frame_ack"},
                            {"delivery_token", _deliveryToken}};
        if (capabilities) {
            message.insert("latest_frame_ack", true);
        } else {
            message.insert("frame_id", _lastFrameId);
        }
        _webSocket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }

    void _endEpoch()
    {
        if (_nativeMode() && _options->frameContexts) {
            _options->frameContexts->endEpoch(_epoch.load(std::memory_order_acquire));
        }
    }

    void _rejectFrame()
    {
        _failStream(GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE, "WebSocket frame context was rejected",
                    QStringLiteral("Missing, ambiguous, or invalid frame metadata"));
        if (_webSocket) {
            _webSocket->close(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Invalid frame context"));
        }
    }

    void _handleTextMessage(const QString& message)
    {
        if (!_nativeMode() || !_accepting.load(std::memory_order_acquire) || !_running || _terminalError) {
            return;
        }
        if (message.size() > kMaximumMetadataBytes) {
            _rejectFrame();
            return;
        }
        const QByteArray bytes = message.toUtf8();
        if (bytes.size() > kMaximumMetadataBytes) {
            _rejectFrame();
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            _rejectFrame();
            return;
        }
        const QJsonObject metadata = document.object();
        if (!metadata.value("type").isString()) {
            _rejectFrame();
            return;
        }
        // Control messages can interleave with a metadata/JPEG pair (for example a pong).
        if (metadata.value("type").toString() != QLatin1String("frame")) {
            return;
        }
        const QJsonObject provenance = metadata.value("provenance").toObject();
        const QJsonValue frameId = metadata.value("frame_id");
        if (!_pendingMetadata.isEmpty() || !integerInRange(frameId, 0, std::numeric_limits<qint64>::max()) ||
            frameId.toInteger() <= _lastFrameId ||
            provenance.value("frame_id").toString() != QString::number(frameId.toInteger()) ||
            !integerInRange(metadata.value("size"), 4, QGCWebSocketVideoSource::kMaximumJpegBytes) ||
            !integerInRange(provenance.value("encoded_width"), 1, QGCWebSocketVideoSource::kMaximumJpegDimension) ||
            !integerInRange(provenance.value("encoded_height"), 1, QGCWebSocketVideoSource::kMaximumJpegDimension) ||
            !nonnegativeNumber(metadata.value("timestamp")) || !integerInRange(metadata.value("quality"), 0, 100) ||
            (!metadata.value("frame_age_ms").isNull() && !nonnegativeNumber(metadata.value("frame_age_ms")))) {
            _rejectFrame();
            return;
        }
        _pendingDeliveryVerified = metadata.value("delivery_token").isString() &&
                                   metadata.value("delivery_token").toString() == _deliveryToken &&
                                   !_deliveryToken.isEmpty();
        if (!_pendingDeliveryVerified && (metadata.contains("delivery_token") || _deliveryConfirmed)) {
            _rejectFrame();
            return;
        }
        _pendingMetadata = metadata;
        // The echo places metadata sampling after this local token was sent. Adding that
        // interval to the reported ages bounds transit without comparing companion clocks.
        _pendingReceivedMonotonicMs =
            _pendingDeliveryVerified ? _tokenSentMonotonicMs : QGCVideoFrameContextStore::monotonicMs();
        _pairingTimer.start();
    }

    void _failStream(GQuark domain, gint code, const char* message, const QString& debug)
    {
        if (!_accepting.load(std::memory_order_acquire) || !_running || _terminalError || !_appsrc || !_bus) {
            return;
        }

        _terminalError = true;
        _endEpoch();
        GError* error = g_error_new_literal(domain, code, message);
        const QByteArray debugUtf8 = debug.toUtf8();
        GstMessage* errorMessage = gst_message_new_error(GST_OBJECT(_appsrc), error, debugUtf8.constData());
        g_clear_error(&error);
        if (!gst_bus_post(_bus, errorMessage)) {
            qCWarning(QGCWebSocketVideoSourceLog) << "Failed to post WebSocket terminal error to the pipeline bus";
        }
    }

    void _handleBinaryMessage(const QByteArray& message)
    {
        if (!_accepting.load(std::memory_order_acquire) || !_running || _terminalError) {
            return;
        }

        if (!QGCWebSocketVideoSource::isCompleteJpeg(message)) {
            qCWarning(QGCWebSocketVideoSourceLog)
                << "Rejecting WebSocket message that is not exactly one complete JPEG; bytes:" << message.size();
            _failStream(GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE, "WebSocket JPEG stream was rejected",
                        QStringLiteral("Invalid JPEG message (%1 bytes)").arg(message.size()));
            if (_webSocket) {
                _webSocket->close(QWebSocketProtocol::CloseCodeProtocolError, QStringLiteral("Invalid JPEG frame"));
            }
            return;
        }

        qint64 ptsUs = -1;
        QJsonObject metadata;
        if (_nativeMode()) {
            const QJsonObject provenance = _pendingMetadata.value("provenance").toObject();
            QSize dimensions;
            if (_pendingMetadata.isEmpty() || !_pairingTimer.isValid() ||
                _pairingTimer.elapsed() > kMaximumPairingDelayMs ||
                _pendingMetadata.value("size").toInteger() != message.size() || !parseSingleJpeg(message, dimensions) ||
                dimensions !=
                    QSize(provenance.value("encoded_width").toInt(), provenance.value("encoded_height").toInt())) {
                _rejectFrame();
                return;
            }
            metadata = std::exchange(_pendingMetadata, {});
            if (!_pendingDeliveryVerified) {
                // A frame may already be in flight when capability negotiation reaches the server.
                // Acknowledge the complete pair to unblock it, but never present that frame.
                _lastFrameId = metadata.value("frame_id").toInteger();
                // Its ACK may be ignored if it predates server-side flow control. Retain the
                // original token and age bound so both accepted and ignored ACKs converge.
                _webSocket->sendTextMessage(
                    QString::fromUtf8(QJsonDocument(QJsonObject{{"type", "frame_ack"},
                                                                {"frame_id", _lastFrameId},
                                                                {"delivery_token", _deliveryToken}})
                                          .toJson(QJsonDocument::Compact)));
                return;
            }
            _deliveryConfirmed = true;
            ptsUs = _options->frameContexts->insert(_epoch.load(std::memory_order_acquire), metadata,
                                                    _pendingReceivedMonotonicMs);
            if (ptsUs < 0) {
                _rejectFrame();
                return;
            }
        }

        GstBuffer* buffer = gst_buffer_new_allocate(nullptr, static_cast<gsize>(message.size()), nullptr);
        if (!buffer) {
            qCWarning(QGCWebSocketVideoSourceLog) << "Failed to allocate WebSocket JPEG buffer";
            _failStream(GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NO_SPACE_LEFT, "WebSocket JPEG buffer allocation failed",
                        QStringLiteral("Requested %1 bytes").arg(message.size()));
            if (_webSocket) {
                _webSocket->abort();
            }
            return;
        }

        (void) gst_buffer_fill(buffer, 0, message.constData(), static_cast<gsize>(message.size()));
        GST_BUFFER_PTS(buffer) = ptsUs >= 0 ? static_cast<GstClockTime>(ptsUs) * GST_USECOND : GST_CLOCK_TIME_NONE;
        GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
        GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

        const GstFlowReturn result = gst_app_src_push_buffer(GST_APP_SRC(_appsrc), buffer);
        if (_nativeMode() && result == GST_FLOW_OK && _accepting.load(std::memory_order_acquire) &&
            _options->frameContexts->isCurrentEpoch(_epoch.load(std::memory_order_acquire))) {
            _lastFrameId = metadata.value("frame_id").toInteger();
            _sendDeliveryToken();
        }
        if ((result != GST_FLOW_OK) && (result != GST_FLOW_FLUSHING)) {
            qCWarning(QGCWebSocketVideoSourceLog) << "Failed to push WebSocket JPEG buffer:" << result;
            _failStream(GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED, "WebSocket JPEG pipeline rejected a frame",
                        QStringLiteral("GStreamer flow result %1").arg(static_cast<int>(result)));
            if (_webSocket) {
                _webSocket->abort();
            }
        }
    }

    const QUrl _url;
    GstElement* _appsrc = nullptr;
    GstBus* _bus = nullptr;
    QWebSocket* _webSocket = nullptr;
    const std::shared_ptr<const QGCWebSocketVideoOptions> _options;
    QJsonObject _pendingMetadata;
    QElapsedTimer _pairingTimer;
    qint64 _lastFrameId = -1;
    qint64 _pendingReceivedMonotonicMs = -1;
    qint64 _tokenSentMonotonicMs = -1;
    QString _deliveryToken;
    bool _pendingDeliveryVerified = false;
    bool _deliveryConfirmed = false;
    std::atomic<quint64> _epoch{0};
    std::atomic<bool> _accepting{true};
    bool _running = false;
    bool _terminalError = false;
};

class WebSocketThread final : public QThread
{
public:
    WebSocketThread(const QUrl& url, GstElement* appsrc, std::shared_ptr<const QGCWebSocketVideoOptions> options)
        : _url(url)
        , _appsrc(appsrc ? GST_ELEMENT(gst_object_ref(appsrc)) : nullptr)
        , _options(std::move(options))
    {
        setObjectName(QStringLiteral("QGCWebSocketVideo"));
    }

    ~WebSocketThread() override
    {
        gst_clear_object(&_bus);
        gst_clear_object(&_appsrc);
    }

    bool startSession(GstBus* bus)
    {
        if (!bus) {
            qCCritical(QGCWebSocketVideoSourceLog) << "WebSocket source activation requires a pipeline bus";
            return false;
        }
        if (_started.exchange(true, std::memory_order_acq_rel)) {
            return isRunning() && !_stopRequested.load(std::memory_order_acquire);
        }

        _bus = GST_BUS(gst_object_ref(bus));
        start();
        if (!_initialized.tryAcquire(1, kThreadLifecycleTimeoutMs)) {
            qCCritical(QGCWebSocketVideoSourceLog) << "Timed out starting the WebSocket video worker";
            stopSession();
            return false;
        }
        return !_stopRequested.load(std::memory_order_acquire);
    }

    void stopSession()
    {
        _stopRequested.store(true, std::memory_order_release);

        {
            QMutexLocker lock(&_workerMutex);
            if (_worker) {
                _worker->requestStop();
                if (!QMetaObject::invokeMethod(
                        _worker,
                        [this, worker = _worker]() {
                            worker->stop();
                            quit();
                        },
                        Qt::QueuedConnection)) {
                    quit();
                }
            } else {
                quit();
            }
        }

        if (QThread::currentThread() == this) {
            qCCritical(QGCWebSocketVideoSourceLog) << "WebSocket source shutdown must run on its receiver owner thread";
            return;
        }

        if (!wait(kThreadLifecycleTimeoutMs)) {
            qCWarning(QGCWebSocketVideoSourceLog)
                << "WebSocket video worker exceeded its shutdown budget; waiting for ownership-safe completion";
            (void) wait();
        }
    }

protected:
    void run() final
    {
        WebSocketWorker worker(_url, _appsrc, _bus, _options);
        {
            QMutexLocker lock(&_workerMutex);
            _worker = &worker;
        }

        if (!_stopRequested.load(std::memory_order_acquire)) {
            worker.start();
        } else {
            worker.requestStop();
        }
        _initialized.release();

        if (!_stopRequested.load(std::memory_order_acquire)) {
            (void) exec();
        }

        worker.stop();
        {
            QMutexLocker lock(&_workerMutex);
            _worker = nullptr;
        }
    }

private:
    const QUrl _url;
    GstElement* _appsrc = nullptr;
    GstBus* _bus = nullptr;
    const std::shared_ptr<const QGCWebSocketVideoOptions> _options;
    QMutex _workerMutex;
    WebSocketWorker* _worker = nullptr;
    QSemaphore _initialized;
    std::atomic<bool> _started{false};
    std::atomic<bool> _stopRequested{false};
};

}  // namespace

class QGCWebSocketVideoSource::Impl
{
public:
    Impl(const QUrl& url, GstElement* appsrc, std::shared_ptr<const QGCWebSocketVideoOptions> options)
        : _thread(std::make_unique<WebSocketThread>(
              url, appsrc, options ? std::make_shared<const QGCWebSocketVideoOptions>(*options) : nullptr))
    {}

    ~Impl() { stop(); }

    bool start(GstBus* bus) { return _thread && _thread->startSession(bus); }

    void stop()
    {
        if (_thread) {
            _thread->stopSession();
            if (!_thread->isRunning()) {
                _thread.reset();
            }
        }
    }

private:
    std::unique_ptr<WebSocketThread> _thread;
};

QGCWebSocketVideoSource::QGCWebSocketVideoSource(const QUrl& url, GstElement* appsrc,
                                                 std::shared_ptr<const QGCWebSocketVideoOptions> options)
    : _impl(std::make_unique<Impl>(url, appsrc, std::move(options)))
{}

QGCWebSocketVideoSource::~QGCWebSocketVideoSource() = default;

bool QGCWebSocketVideoSource::start(GstBus* bus)
{
    return _impl && _impl->start(bus);
}

void QGCWebSocketVideoSource::stop()
{
    if (_impl) {
        _impl->stop();
    }
}

bool QGCWebSocketVideoSource::isCompleteJpeg(QByteArrayView message)
{
    if (message.size() < 4 || message.size() > kMaximumJpegBytes) {
        return false;
    }

    QSize parsedDimensions;
    if (!parseSingleJpeg(message, parsedDimensions)) {
        return false;
    }

    if (!parsedDimensions.isValid() || (parsedDimensions.width() > kMaximumJpegDimension) ||
        (parsedDimensions.height() > kMaximumJpegDimension)) {
        return false;
    }
    return (static_cast<quint64>(parsedDimensions.width()) * static_cast<quint64>(parsedDimensions.height())) <=
           kMaximumDecodedPixels;
}
