#include "QGCWebSocketVideoSource.h"

#include <QtCore/QBuffer>
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtGui/QImageReader>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslError>
#include <QtWebSockets/QWebSocket>
#include <atomic>
#include <gst/app/gstappsrc.h>

#include "QGCLoggingCategory.h"
#include "QGCNetworkHelper.h"

QGC_LOGGING_CATEGORY(QGCWebSocketVideoSourceLog, "Video.GStreamer.WebSocketVideoSource")

namespace {

constexpr int kThreadLifecycleTimeoutMs = 3000;

class WebSocketWorker final : public QObject
{
public:
    WebSocketWorker(const QUrl& url, const QString& origin, GstElement* appsrc)
        : _url(url), _origin(origin), _appsrc(appsrc ? GST_ELEMENT(gst_object_ref(appsrc)) : nullptr)
    {}

    ~WebSocketWorker() override
    {
        stop();
        gst_clear_object(&_appsrc);
    }

    void start()
    {
        if (_running || !_appsrc || !_accepting.load(std::memory_order_acquire)) {
            return;
        }

        _webSocket = new QWebSocket(_origin, QWebSocketProtocol::VersionLatest, this);
        _webSocket->setMaxAllowedIncomingFrameSize(static_cast<quint64>(QGCWebSocketVideoSource::kMaximumJpegBytes));
        _webSocket->setMaxAllowedIncomingMessageSize(static_cast<quint64>(QGCWebSocketVideoSource::kMaximumJpegBytes));
        _webSocket->setReadBufferSize(static_cast<qint64>(QGCWebSocketVideoSource::kMaximumJpegBytes));

        if (_url.scheme() == QLatin1String("wss")) {
            _webSocket->setSslConfiguration(QGCNetworkHelper::createSslConfig());
        }

        (void) connect(_webSocket, &QWebSocket::connected, this, [this]() {
            qCDebug(QGCWebSocketVideoSourceLog) << "Connected" << QGCNetworkHelper::redactedUrlForLogging(_url);
        });
        (void) connect(_webSocket, &QWebSocket::disconnected, this, [this]() {
            qCDebug(QGCWebSocketVideoSourceLog) << "Disconnected" << QGCNetworkHelper::redactedUrlForLogging(_url);
            _failStream(GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ, "WebSocket video connection closed",
                        QStringLiteral("Connection closed unexpectedly"));
        });
        (void) connect(_webSocket, &QWebSocket::binaryMessageReceived, this,
                       [this](const QByteArray& message) { _handleBinaryMessage(message); });
        (void) connect(_webSocket, &QWebSocket::textMessageReceived, this, [](const QString&) {});
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
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

        _running = true;
        _terminalError = false;
        qCDebug(QGCWebSocketVideoSourceLog) << "Opening" << QGCNetworkHelper::redactedUrlForLogging(_url);
        _webSocket->open(request);
    }

    void stop()
    {
        _accepting.store(false, std::memory_order_release);
        _running = false;
        _terminalError = true;
        if (_webSocket) {
            _webSocket->disconnect(this);
            _webSocket->abort();
            delete _webSocket;
            _webSocket = nullptr;
        }
    }

    void requestStop() { _accepting.store(false, std::memory_order_release); }

private:
    void _failStream(GQuark domain, gint code, const char* message, const QString& debug)
    {
        if (!_accepting.load(std::memory_order_acquire) || !_running || _terminalError || !_appsrc) {
            return;
        }

        _terminalError = true;
        GError* error = g_error_new_literal(domain, code, message);
        const QByteArray debugUtf8 = debug.toUtf8();
        GstMessage* errorMessage = gst_message_new_error(GST_OBJECT(_appsrc), error, debugUtf8.constData());
        g_clear_error(&error);
        (void) gst_element_post_message(_appsrc, errorMessage);
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
        GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
        GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
        GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

        const GstFlowReturn result = gst_app_src_push_buffer(GST_APP_SRC(_appsrc), buffer);
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
    const QString _origin;
    GstElement* _appsrc = nullptr;
    QWebSocket* _webSocket = nullptr;
    std::atomic<bool> _accepting{true};
    bool _running = false;
    bool _terminalError = false;
};

}  // namespace

class QGCWebSocketVideoSource::Impl
{
public:
    Impl(const QUrl& url, const QString& origin, GstElement* appsrc)
        : _worker(new WebSocketWorker(url, origin, appsrc)), _thread(new QThread)
    {
        _thread->setObjectName(QStringLiteral("QGCWebSocketVideo"));
    }

    ~Impl() { stop(); }

    bool start()
    {
        if (_started) {
            return true;
        }
        if (!_worker) {
            return false;
        }

        _worker->moveToThread(_thread);
        const auto initialized = std::make_shared<QSemaphore>();
        (void) QObject::connect(_thread, &QThread::started, _worker.data(), [worker = _worker, initialized]() {
            if (worker) {
                worker->start();
            }
            initialized->release();
        });
        (void) QObject::connect(_thread, &QThread::finished, _worker.data(), &QObject::deleteLater);
        _started = true;
        _thread->start();
        if (!initialized->tryAcquire(1, kThreadLifecycleTimeoutMs)) {
            qCCritical(QGCWebSocketVideoSourceLog) << "Timed out starting the WebSocket video worker";
            stop();
            return false;
        }
        return true;
    }

    void stop()
    {
        if (!_started) {
            if (_worker) {
                delete _worker.data();
                _worker.clear();
            }
            delete _thread;
            _thread = nullptr;
            return;
        }

        WebSocketWorker* const worker = _worker.data();
        QThread* thread = _thread;
        _started = false;
        if (worker) {
            worker->requestStop();
        }

        if (thread && thread->isRunning() && worker) {
            if (QThread::currentThread() == thread) {
                worker->stop();
            } else {
                const auto stopped = std::make_shared<QSemaphore>();
                (void) QMetaObject::invokeMethod(
                    worker,
                    [worker, stopped]() {
                        worker->stop();
                        stopped->release();
                    },
                    Qt::QueuedConnection);
                if (!stopped->tryAcquire(1, kThreadLifecycleTimeoutMs)) {
                    qCCritical(QGCWebSocketVideoSourceLog) << "Timed out stopping the WebSocket video worker";
                }
            }
        }

        if (thread) {
            thread->quit();
        }
        if (thread && (QThread::currentThread() == thread)) {
            qCCritical(QGCWebSocketVideoSourceLog)
                << "WebSocket video source stopped from its worker thread; preserving guarded resources";
            _worker.clear();
            _thread = nullptr;
            return;
        }
        if (thread && !thread->wait(kThreadLifecycleTimeoutMs)) {
            qCCritical(QGCWebSocketVideoSourceLog)
                << "WebSocket video thread did not stop; preserving its guarded resources";
            // requestStop() makes callbacks inert. Keeping the thread and worker alive is safer
            // than deleting a QObject that may still be processing an event in its owning thread.
            _worker.clear();
            _thread = nullptr;
            return;
        }

        if (_worker && (!thread || !thread->isRunning())) {
            delete _worker.data();
            _worker.clear();
        }
        delete thread;
        _thread = nullptr;
    }

private:
    QPointer<WebSocketWorker> _worker;
    QThread* _thread = nullptr;
    bool _started = false;
};

QGCWebSocketVideoSource::QGCWebSocketVideoSource(const QUrl& url, const QString& origin, GstElement* appsrc)
    : _impl(std::make_unique<Impl>(url, origin, appsrc))
{}

QGCWebSocketVideoSource::~QGCWebSocketVideoSource() = default;

bool QGCWebSocketVideoSource::start()
{
    return _impl && _impl->start();
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

    const auto* data = reinterpret_cast<const unsigned char*>(message.data());
    if ((data[0] != 0xFF) || (data[1] != 0xD8) || (data[message.size() - 2] != 0xFF) ||
        (data[message.size() - 1] != 0xD9)) {
        return false;
    }

    QByteArray jpeg = QByteArray::fromRawData(message.data(), message.size());
    QBuffer buffer(&jpeg);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return false;
    }
    QImageReader reader(&buffer, "JPEG");
    if (!reader.canRead()) {
        return false;
    }

    const QSize size = reader.size();
    if (!size.isValid() || (size.width() > kMaximumJpegDimension) || (size.height() > kMaximumJpegDimension)) {
        return false;
    }
    return (static_cast<quint64>(size.width()) * static_cast<quint64>(size.height())) <= kMaximumDecodedPixels;
}

QString QGCWebSocketVideoSource::normalizedOrigin(const QString& origin)
{
    const QString candidate = origin.trimmed();
    if (candidate.isEmpty()) {
        return {};
    }

    const QUrl url(candidate, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.isRelative() ||
        ((scheme != QLatin1String("http")) && (scheme != QLatin1String("https"))) || url.host().isEmpty() ||
        !url.userInfo().isEmpty() || !url.query().isEmpty() || !url.fragment().isEmpty() ||
        (!url.path().isEmpty() && (url.path() != QLatin1String("/")))) {
        return {};
    }

    QUrl normalized(url);
    normalized.setScheme(scheme);
    normalized.setPath(QString());
    return normalized.toString(QUrl::FullyEncoded | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo);
}
