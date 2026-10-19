#include "QGCWebRtcVideoSource.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/sdp/sdp.h>
#include <gst/video/video-event.h>
#include <gst/webrtc/webrtc.h>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QSemaphore>
#include <QtCore/QSize>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QWaitCondition>
#include <QtNetwork/QNetworkRequest>
#include <QtWebSockets/QWebSocket>

#include "GstSourceFactory.h"
#include "QGCLoggingCategory.h"
#include "QGCNetworkHelper.h"
#include "QGCVideoFrameContextStore.h"
#include "gstqgc/GstQgcFrameContext.h"

QGC_LOGGING_CATEGORY(QGCWebRtcVideoSourceLog, "Video.GStreamer.WebRtcVideoSource")

namespace {

constexpr char CONTEXT_KEY[] = "qgc-webrtc-video-source";
constexpr int MAX_CONTEXTS = 128;
constexpr int MAX_CONTEXT_AGE_MS = 2000;
constexpr int METADATA_JOIN_BUDGET_MS = 100;

struct PacketContext
{
    quint64 identity = 0;
    qint64 observedMs = 0;
    bool valid = true;
};

struct FrameContext
{
    QJsonObject metadata;
    qint64 challengeMs = 0;
    qint64 observedMs = 0;
};

struct MediaState
{
    std::atomic<bool> accepting{true};
    GstElement* peer = nullptr;
    GstElement* parser = nullptr;
    GstBus* bus = nullptr;
    std::shared_ptr<const QGCWebRtcVideoOptions> options;
    QMutex mutex;
    QWaitCondition metadataReady;
    QMap<GstClockTime, PacketContext> packets;
    QMap<quint64, FrameContext> metadata;
    QMap<quint64, qint64> consumed;
    QString providerEpoch;
    quint64 epoch = 0;
    QSize requestedGeometry;

    void fail(const char* stage)
    {
        if (!accepting.exchange(false) || !bus) {
            return;
        }
        qCWarning(QGCWebRtcVideoSourceLog) << "WebRTC failed at stage" << stage;
        metadataReady.wakeAll();
        GError* error = g_error_new_literal(GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ, stage);
        gst_bus_post(bus, gst_message_new_error(GST_OBJECT(peer), error, stage));
        g_error_free(error);
    }

    void prune(qint64 now)
    {
        auto pruneMap = [now](auto& map, auto timestamp) {
            for (auto it = map.begin(); it != map.end();) {
                if (now - timestamp(it.value()) > MAX_CONTEXT_AGE_MS) {
                    it = map.erase(it);
                } else {
                    ++it;
                }
            }
            while (map.size() > MAX_CONTEXTS) {
                auto oldest = map.begin();
                for (auto it = map.begin(); it != map.end(); ++it) {
                    if (timestamp(it.value()) < timestamp(oldest.value())) {
                        oldest = it;
                    }
                }
                map.erase(oldest);
            }
        };
        pruneMap(packets, [](const auto& entry) { return entry.observedMs; });
        pruneMap(metadata, [](const auto& entry) { return entry.observedMs; });
        pruneMap(consumed, [](qint64 timestamp) { return timestamp; });
    }
};

quint64 packetIdentity(guint32 ssrc, guint32 timestamp)
{
    return (static_cast<quint64>(ssrc) << 32) | timestamp;
}

bool uint32Value(const QJsonValue& value)
{
    const double number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) && number >= 0 && number <= G_MAXUINT32 &&
           std::floor(number) == number;
}

GstPadProbeReturn observeRtp(GstPad* pad, GstPadProbeInfo* info, gpointer data)
{
    auto* state = static_cast<MediaState*>(data);
    if (!state->accepting.load()) {
        return GST_PAD_PROBE_OK;
    }
    GstBuffer* buffer = gst_pad_probe_info_get_buffer(info);
    GstRTPBuffer packet = GST_RTP_BUFFER_INIT;
    if (!buffer || !gst_rtp_buffer_map(buffer, GST_MAP_READ, &packet)) {
        return GST_PAD_PROBE_OK;
    }
    if (gst_rtp_buffer_get_marker(&packet)) {
        GstEvent* event = gst_pad_get_sticky_event(pad, GST_EVENT_SEGMENT, 0);
        if (event) {
            const GstSegment* segment = nullptr;
            gst_event_parse_segment(event, &segment);
            const auto running = gst_segment_to_running_time(segment, GST_FORMAT_TIME, GST_BUFFER_PTS(buffer));
            if (GST_CLOCK_TIME_IS_VALID(running)) {
                QMutexLocker lock(&state->mutex);
                const auto identity =
                    packetIdentity(gst_rtp_buffer_get_ssrc(&packet), gst_rtp_buffer_get_timestamp(&packet));
                const auto now = QGCVideoFrameContextStore::monotonicMs();
                state->prune(now);
                if (state->packets.contains(running)) {
                    state->consumed.insert(identity, now);
                    state->consumed.insert(state->packets.value(running).identity, now);
                    state->packets[running].valid = false;
                } else {
                    state->packets.insert(running, PacketContext{identity, now});
                }
            }
            gst_event_unref(event);
        }
    }
    gst_rtp_buffer_unmap(&packet);
    return GST_PAD_PROBE_OK;
}

void configureLossRecovery(GstBin*, GstBin*, GstElement* element, gpointer)
{
    GstElementFactory* factory = gst_element_get_factory(element);
    if (!factory || !gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_DEPAYLOADER)) {
        return;
    }
    for (const auto* property : {"request-keyframe", "wait-for-keyframe"}) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(element), property)) {
            g_object_set(element, property, TRUE, nullptr);
        }
    }
}

gboolean requireAccessUnits(GstElement*, GstPad*, GstElement* element, GstQuery* query, gpointer)
{
    GstElementFactory* factory = gst_element_get_factory(element);
    if (GST_QUERY_TYPE(query) != GST_QUERY_CAPS || !factory ||
        g_strcmp0(gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory)), "rtph264depay") != 0) {
        return FALSE;
    }
    // H.264's wait-for-keyframe guard has no effect when depayloading individual NAL units.
    GstCaps* caps = gst_caps_from_string("video/x-h264,stream-format=avc,alignment=au");
    GstCaps* filter = nullptr;
    gst_query_parse_caps(query, &filter);
    GstCaps* allowed = filter ? gst_caps_intersect_full(caps, filter, GST_CAPS_INTERSECT_FIRST) : gst_caps_ref(caps);
    gst_query_set_caps_result(query, allowed);
    gst_caps_unref(allowed);
    gst_caps_unref(caps);
    return TRUE;
}

void exposeParsedPad(GstElement* parser, GstPad* pad, gpointer data)
{
    auto* state = static_cast<MediaState*>(data);
    if (!state->accepting.load() || GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
        return;
    }
    GstObject* parent = gst_object_get_parent(GST_OBJECT(parser));
    if (parent) {
        GstPad* ghost = gst_ghost_pad_new("src", pad);
        if (ghost) {
            gst_pad_set_active(ghost, TRUE);
            if (!gst_element_add_pad(GST_ELEMENT(parent), ghost)) {
                gst_object_unref(ghost);
                state->fail("parser_pad");
            }
        }
        gst_object_unref(parent);
    }
}

void receiveRtpPad(GstElement*, GstPad* pad, gpointer data)
{
    auto* state = static_cast<MediaState*>(data);
    if (!state->accepting.load() || GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
        return;
    }
    GstPad* sink = gst_element_get_static_pad(state->parser, "sink");
    if (sink) {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, observeRtp, state, nullptr);
        if (gst_pad_is_linked(sink) || gst_pad_link(pad, sink) != GST_PAD_LINK_OK) {
            state->fail("rtp_pad");
        }
        gst_object_unref(sink);
    }
}

class SignalingWorker;

struct CallbackTarget
{
    QMutex mutex;
    SignalingWorker* worker = nullptr;
};

using CallbackTargetPtr = std::shared_ptr<CallbackTarget>;

class SignalingWorker final : public QObject
{
public:
    SignalingWorker(QUrl url, MediaState* state)
        : _url(std::move(url))
        , _state(state)
        , _target(std::make_shared<CallbackTarget>())
    {
        _target->worker = this;
    }

    void start()
    {
        if (!_configureIce()) {
            _state->fail("ice_configuration");
            return;
        }
        _socket = new QWebSocket(_state->options->origin, QWebSocketProtocol::VersionLatest, this);
        _socket->setMaxAllowedIncomingMessageSize(131072);
        _socket->setMaxAllowedIncomingFrameSize(131072);
        _socket->setReadBufferSize(131072);
        if (_url.scheme() == QLatin1String("wss")) {
            _socket->setSslConfiguration(QGCNetworkHelper::createSslConfig());
        }
        connect(_socket, &QWebSocket::connected, this, [this]() { _createOffer(); });
        connect(_socket, &QWebSocket::textMessageReceived, this, [this](const QString& text) { _message(text); });
        connect(_socket, &QWebSocket::disconnected, this, [this]() { _state->fail("signaling_closed"); });
        connect(_socket, &QWebSocket::errorOccurred, this,
                [this](QAbstractSocket::SocketError) { _state->fail("signaling_transport"); });
        auto destroyTarget = [](gpointer data, GClosure*) { delete static_cast<CallbackTargetPtr*>(data); };
        _iceSignal = g_signal_connect_data(_state->peer, "on-ice-candidate", G_CALLBACK(_iceCandidate),
                                           new CallbackTargetPtr(_target), destroyTarget, GConnectFlags(0));
        _connectionSignal =
            g_signal_connect_data(_state->peer, "notify::connection-state", G_CALLBACK(_connectionChanged),
                                  new CallbackTargetPtr(_target), destroyTarget, GConnectFlags(0));
        auto* challenge = new QTimer(this);
        challenge->setInterval(250);
        connect(challenge, &QTimer::timeout, this, [this]() { _sendChallenge(); });
        challenge->start();
        QTimer::singleShot(8000, this, [this]() {
            if (!_answered || !_connected) {
                _state->fail(_answered ? "ice_timeout" : "signaling_timeout");
            }
        });
        QNetworkRequest request(_url);
        request.setRawHeader("Cookie", _state->options->cookie);
        request.setRawHeader("User-Agent", QGCNetworkHelper::defaultUserAgent().toUtf8());
        _socket->open(request);
    }

    void stop()
    {
        {
            QMutexLocker lock(&_target->mutex);
            _target->worker = nullptr;
        }
        for (auto* promise : _promises) {
            gst_promise_interrupt(promise);
            gst_promise_unref(promise);
        }
        _promises.clear();
        if (_iceSignal) {
            g_signal_handler_disconnect(_state->peer, _iceSignal);
            _iceSignal = 0;
        }
        if (_connectionSignal) {
            g_signal_handler_disconnect(_state->peer, _connectionSignal);
            _connectionSignal = 0;
        }
        if (_socket) {
            _socket->blockSignals(true);
            _socket->abort();
        }
    }

private:
    static void _post(const CallbackTargetPtr& target, std::function<void(SignalingWorker*)> work)
    {
        QMutexLocker lock(&target->mutex);
        if (target->worker) {
            QMetaObject::invokeMethod(
                target->worker,
                [self = target->worker, work = std::move(work)]() {
                    if (self->_state->accepting.load()) {
                        work(self);
                    }
                },
                Qt::QueuedConnection);
        }
    }

    bool _configureIce()
    {
        for (const auto& entry : _state->options->iceServers) {
            const auto server = entry.toObject();
            QJsonArray urls = server.value("urls").toArray();
            if (server.value("urls").isString()) {
                urls.append(server.value("urls"));
            }
            for (const auto& value : urls) {
                QString text = value.toString();
                const auto colon = text.indexOf(':');
                if (colon < 1 || text.size() > 2048) {
                    return false;
                }
                if (text.mid(colon + 1, 2) != QLatin1String("//")) {
                    text.insert(colon + 1, "//");
                }
                QUrl url(text);
                if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()) {
                    return false;
                }
                if (url.scheme() == QLatin1String("stun")) {
                    g_object_set(_state->peer, "stun-server", url.toEncoded().constData(), nullptr);
                } else if (url.scheme() == QLatin1String("turn") || url.scheme() == QLatin1String("turns")) {
                    url.setUserName(server.value("username").toString());
                    url.setPassword(server.value("credential").toString());
                    gboolean accepted = FALSE;
                    g_signal_emit_by_name(_state->peer, "add-turn-server", url.toEncoded().constData(), &accepted);
                    if (!accepted) {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }
        return true;
    }

    static void _iceCandidate(GstElement*, guint index, gchar* candidate, gpointer data)
    {
        const auto target = *static_cast<CallbackTargetPtr*>(data);
        const QString value = QString::fromUtf8(candidate);
        _post(target, [index, value](SignalingWorker* self) {
            if (!self->_offerSent) {
                if (self->_localCandidates.size() >= 64) {
                    self->_state->fail("local_ice_backlog");
                    return;
                }
                self->_localCandidates.append({index, value});
            } else {
                self->_sendIce(index, value);
            }
        });
    }

    static void _connectionChanged(GObject* peer, GParamSpec*, gpointer data)
    {
        GstWebRTCPeerConnectionState state;
        g_object_get(peer, "connection-state", &state, nullptr);
        const auto target = *static_cast<CallbackTargetPtr*>(data);
        _post(target, [state](SignalingWorker* self) {
            self->_connected = state == GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTED;
            if (state == GST_WEBRTC_PEER_CONNECTION_STATE_FAILED) {
                self->_state->fail("ice_failed");
            }
        });
    }

    void _send(const QJsonObject& message)
    {
        if (!_state->accepting.load() || !_socket || _socket->state() != QAbstractSocket::ConnectedState) {
            return;
        }
        if (_socket->bytesToWrite() > 65536) {
            _state->fail("signaling_backlog");
            return;
        }
        _socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }

    void _sendIce(guint index, const QString& candidate)
    {
        _send({{"type", "ice-candidate"},
               {"payload", QJsonObject{{"candidate", candidate}, {"sdpMLineIndex", static_cast<int>(index)}}}});
    }

    void _createOffer()
    {
        GstCaps* caps = gst_caps_new_empty();
        QStringList seen;
        for (const auto& codec : _state->options->codecPreferences) {
            if (seen.contains(codec)) {
                continue;
            }
            seen.append(codec);
            if (codec == QLatin1String("H264")) {
                gst_caps_append(
                    caps,
                    gst_caps_from_string("application/x-rtp,media=video,encoding-name=H264,clock-rate=90000,payload=96,"
                                         "packetization-mode=(string)1,profile-level-id=(string)42e01f"));
            } else if (codec == QLatin1String("VP8")) {
                gst_caps_append(caps,
                                gst_caps_from_string(
                                    "application/x-rtp,media=video,encoding-name=VP8,clock-rate=90000,payload=97"));
            }
        }
        if (gst_caps_is_empty(caps)) {
            gst_caps_unref(caps);
            _state->fail("video_codec");
            return;
        }
        for (guint i = 0; i < gst_caps_get_size(caps); ++i) {
            gst_structure_set(gst_caps_get_structure(caps, i), "extmap-1", G_TYPE_STRING,
                              "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
                              "rtcp-fb-nack", G_TYPE_BOOLEAN, TRUE, "rtcp-fb-nack-pli", G_TYPE_BOOLEAN, TRUE, nullptr);
        }
        GstWebRTCRTPTransceiver* transceiver = nullptr;
        g_signal_emit_by_name(_state->peer, "add-transceiver", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, caps,
                              &transceiver);
        gst_caps_unref(caps);
        if (!transceiver) {
            _state->fail("transceiver");
            return;
        }
        if (!g_object_class_find_property(G_OBJECT_GET_CLASS(transceiver), "do-nack")) {
            gst_object_unref(transceiver);
            _state->fail("packet_recovery");
            return;
        }
        g_object_set(transceiver, "do-nack", TRUE, nullptr);
        gst_object_unref(transceiver);
        // The promise callback runs on a GStreamer task; never wait on the Qt event loop.
        auto* callbackTarget = new CallbackTargetPtr(_target);
        GstPromise* promise = gst_promise_new_with_change_func(
            [](GstPromise* completed, gpointer data) {
                const auto target = *static_cast<CallbackTargetPtr*>(data);
                GstWebRTCSessionDescription* offer = nullptr;
                const GstStructure* reply = gst_promise_wait(completed) == GST_PROMISE_RESULT_REPLIED
                                                ? gst_promise_get_reply(completed)
                                                : nullptr;
                if (reply) {
                    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, nullptr);
                }
                if (offer) {
                    gchar* text = gst_sdp_message_as_text(offer->sdp);
                    const QString sdp = QString::fromUtf8(text);
                    g_free(text);
                    _post(target, [sdp](SignalingWorker* self) { self->_offerReady(sdp); });
                }
                if (offer) {
                    gst_webrtc_session_description_free(offer);
                }
                gst_promise_unref(completed);
            },
            callbackTarget, [](gpointer data) { delete static_cast<CallbackTargetPtr*>(data); });
        _promises.append(gst_promise_ref(promise));
        g_signal_emit_by_name(_state->peer, "create-offer", nullptr, promise);
    }

    void _offerReady(const QString& sdp)
    {
        _setDescription(sdp, GST_WEBRTC_SDP_TYPE_OFFER, [sdp](SignalingWorker* self) {
            self->_send({{"type", "offer"},
                         {"provider", "gstreamer"},
                         {"payload", QJsonObject{{"type", "offer"}, {"sdp", sdp}}}});
            self->_offerSent = true;
            for (const auto& candidate : self->_localCandidates) {
                self->_sendIce(candidate.first, candidate.second);
            }
            self->_localCandidates.clear();
            self->_sendChallenge();
        });
    }

    void _setDescription(const QString& sdp, GstWebRTCSDPType type, std::function<void(SignalingWorker*)> ready)
    {
        GstSDPMessage* message = nullptr;
        const auto bytes = sdp.toUtf8();
        if (gst_sdp_message_new_from_text(bytes.constData(), &message) != GST_SDP_OK) {
            _state->fail("session_description");
            return;
        }
        auto* description = gst_webrtc_session_description_new(type, message);
        using Completion = std::pair<CallbackTargetPtr, std::function<void(SignalingWorker*)>>;
        auto* completionState = new Completion(_target, std::move(ready));
        auto* promise = gst_promise_new_with_change_func(
            [](GstPromise* completed, gpointer data) {
                const auto completion = *static_cast<Completion*>(data);
                bool succeeded = gst_promise_wait(completed) == GST_PROMISE_RESULT_REPLIED;
                const auto* reply = succeeded ? gst_promise_get_reply(completed) : nullptr;
                succeeded = succeeded && (!reply || !gst_structure_has_field(reply, "error"));
                _post(completion.first, [ready = completion.second, succeeded](SignalingWorker* self) {
                    if (succeeded) {
                        ready(self);
                    } else {
                        self->_state->fail("session_description");
                    }
                });
                gst_promise_unref(completed);
            },
            completionState, [](gpointer data) { delete static_cast<Completion*>(data); });
        _promises.append(gst_promise_ref(promise));
        g_signal_emit_by_name(_state->peer,
                              type == GST_WEBRTC_SDP_TYPE_OFFER ? "set-local-description" : "set-remote-description",
                              description, promise);
        gst_webrtc_session_description_free(description);
    }

    void _sendChallenge()
    {
        if (!_offerSent) {
            return;
        }
        const auto now = QGCVideoFrameContextStore::monotonicMs();
        for (auto it = _challenges.begin(); it != _challenges.end();) {
            if (now - it.value() > MAX_CONTEXT_AGE_MS) {
                it = _challenges.erase(it);
            } else {
                ++it;
            }
        }
        const QString token = QUuid::createUuid().toString(QUuid::Id128);
        _challenges.insert(token, now);
        _send({{"type", "frame-challenge"},
               {"payload", QJsonObject{{"token", token},
                                       {"presentation", _state->options->frameContexts->presentationStatistics()}}}});
    }

    void _message(const QString& text)
    {
        if (!_state->accepting.load()) {
            return;
        }
        const auto document = QJsonDocument::fromJson(text.toUtf8());
        if (!document.isObject()) {
            _state->fail("signaling_json");
            return;
        }
        const auto message = document.object();
        const auto type = message.value("type").toString();
        const auto payload = message.value("payload").toObject();
        if (type == QLatin1String("answer")) {
            if (_answerPending) {
                _state->fail("duplicate_answer");
                return;
            }
            const auto bytes = payload.value("sdp").toString().toUtf8();
            if (bytes.isEmpty() || bytes.size() > 65536) {
                _state->fail("remote_description");
                return;
            }
            _answerPending = true;
            _setDescription(QString::fromUtf8(bytes), GST_WEBRTC_SDP_TYPE_ANSWER, [](SignalingWorker* self) {
                self->_answered = true;
                for (const auto& candidate : self->_pendingCandidates) {
                    g_signal_emit_by_name(self->_state->peer, "add-ice-candidate", 0u, candidate.constData());
                }
                self->_pendingCandidates.clear();
            });
        } else if (type == QLatin1String("ice-candidate")) {
            const auto candidate = payload.value("candidate").toString().toUtf8();
            if (candidate.size() > 4096 || payload.value("sdpMLineIndex").toInt(-1) != 0) {
                _state->fail("ice_candidate");
                return;
            }
            if (!_answered) {
                if (_pendingCandidates.size() >= 64) {
                    _state->fail("ice_backlog");
                    return;
                }
                _pendingCandidates.append(candidate);
            } else {
                g_signal_emit_by_name(_state->peer, "add-ice-candidate", 0u, candidate.constData());
            }
        } else if (type == QLatin1String("frame-context")) {
            _frameContext(payload);
        } else if (type == QLatin1String("error")) {
            _state->fail("remote_media");
        }
    }

    void _frameContext(const QJsonObject& payload)
    {
        if (!uint32Value(payload.value("ssrc")) || !uint32Value(payload.value("rtp_timestamp"))) {
            _state->fail("frame_identity");
            return;
        }
        const auto context = payload.value("context").toObject();
        const auto challenge = _challenges.constFind(context.value("age_bound_token").toString());
        const auto now = QGCVideoFrameContextStore::monotonicMs();
        const QString epoch = payload.value("provider_epoch").toString();
        if (challenge == _challenges.cend() || epoch.isEmpty() || epoch.size() > 64 ||
            now - challenge.value() > MAX_CONTEXT_AGE_MS) {
            return;
        }
        QMutexLocker lock(&_state->mutex);
        if (!_state->providerEpoch.isEmpty() && _state->providerEpoch != epoch) {
            _state->fail("provider_epoch");
            return;
        }
        _state->providerEpoch = epoch;
        const auto identity =
            packetIdentity(payload.value("ssrc").toInteger(), payload.value("rtp_timestamp").toInteger());
        _state->prune(now);
        if (_state->metadata.contains(identity) || _state->consumed.contains(identity)) {
            _state->metadata.remove(identity);
            _state->consumed.insert(identity, now);
            return;
        }
        auto provenance = context;
        const auto geometry = provenance.take("selection_geometry");
        provenance.remove("age_bound_token");
        QJsonObject envelope{{"type", "frame"}, {"provenance", provenance}};
        if (geometry.isObject()) {
            envelope.insert("selection_geometry", geometry);
        }
        _state->metadata.insert(identity, FrameContext{envelope, challenge.value(), now});
        _state->metadataReady.wakeAll();
    }

    const QUrl _url;
    MediaState* _state;
    QWebSocket* _socket = nullptr;
    QMap<QString, qint64> _challenges;
    CallbackTargetPtr _target;
    QList<QByteArray> _pendingCandidates;
    QList<std::pair<guint, QString>> _localCandidates;
    QList<GstPromise*> _promises;
    gulong _iceSignal = 0;
    gulong _connectionSignal = 0;
    bool _answered = false;
    bool _offerSent = false;
    bool _answerPending = false;
    bool _connected = false;
};

class SignalingThread final : public QThread
{
public:
    SignalingThread(QUrl url, MediaState* state)
        : _url(std::move(url))
        , _state(state)
    {}

    bool activate()
    {
        start();
        return _ready.tryAcquire(1, 3000);
    }

    void stop()
    {
        _state->accepting.store(false);
        quit();
        if (!wait(3000)) {
            qCWarning(QGCWebRtcVideoSourceLog) << "Waiting for signaling ownership to stop";
            wait();
        }
    }

protected:
    void run() final
    {
        SignalingWorker worker(_url, _state);
        if (_state->accepting.load()) {
            worker.start();
        }
        _ready.release();
        if (_state->accepting.load()) {
            exec();
        }
        worker.stop();
    }

private:
    const QUrl _url;
    MediaState* _state;
    QSemaphore _ready;
};

}  // namespace

class QGCWebRtcVideoSource::Impl
{
public:
    explicit Impl(std::shared_ptr<const QGCWebRtcVideoOptions> options) { state.options = std::move(options); }

    ~Impl()
    {
        stop();
        gst_clear_object(&state.bus);
        gst_clear_object(&state.peer);
        gst_clear_object(&state.parser);
    }

    void stop()
    {
        state.accepting.store(false);
        {
            QMutexLocker lock(&state.mutex);
            state.metadataReady.wakeAll();
        }
        if (thread) {
            thread->stop();
            thread.reset();
        }
        if (state.epoch) {
            QMutexLocker lock(&state.mutex);
            state.options->frameContexts->endEpoch(state.epoch);
            state.epoch = 0;
        }
    }

    MediaState state;
    QUrl url;
    std::unique_ptr<SignalingThread> thread;
};

QGCWebRtcVideoSource::QGCWebRtcVideoSource(std::unique_ptr<Impl> impl)
    : _impl(std::move(impl))
{}

QGCWebRtcVideoSource::~QGCWebRtcVideoSource() = default;

QJsonObject QGCWebRtcVideoStatistics::snapshot() const
{
    return {{"missing_packet_context", static_cast<qint64>(missingPacketContext.load())},
            {"missing_metadata", static_cast<qint64>(missingMetadata.load())},
            {"metadata_join_timeouts", static_cast<qint64>(metadataJoinTimeouts.load())},
            {"keyframe_requests", static_cast<qint64>(keyframeRequests.load())},
            {"rejected_dimensions", static_cast<qint64>(rejectedDimensions.load())},
            {"corrupted_frames", static_cast<qint64>(corruptedFrames.load())},
            {"associated_frames", static_cast<qint64>(associatedFrames.load())}};
}

bool QGCWebRtcVideoSource::available()
{
    for (const auto* name : {"webrtcbin", "nicesrc", "nicesink", "dtlssrtpdec", "dtlssrtpenc", "rtph264depay",
                             "h264parse", "rtpvp8depay", "parsebin"}) {
        GstElementFactory* factory = gst_element_factory_find(name);
        if (!factory) {
            return false;
        }
        gst_object_unref(factory);
    }
    return true;
}

GstElement* QGCWebRtcVideoSource::create(const QUrl& signalingUrl, std::shared_ptr<const QGCWebRtcVideoOptions> options)
{
    if (!available() || !options || !options->frameContexts || !options->statistics || signalingUrl.host().isEmpty() ||
        (signalingUrl.scheme() != QLatin1String("ws") && signalingUrl.scheme() != QLatin1String("wss")) ||
        !signalingUrl.userInfo().isEmpty() || options->cookie.contains('\r') || options->cookie.contains('\n') ||
        options->origin.contains('\r') || options->origin.contains('\n')) {
        return nullptr;
    }
    auto impl = std::make_unique<Impl>(std::make_shared<const QGCWebRtcVideoOptions>(*options));
    GstElement* bin = gst_bin_new("sourcebin");
    impl->state.peer = gst_element_factory_make("webrtcbin", "native-webrtc");
    impl->state.parser = gst_element_factory_make("parsebin", "webrtc-parser");
    if (!bin || !impl->state.peer || !impl->state.parser) {
        gst_clear_object(&impl->state.peer);
        gst_clear_object(&impl->state.parser);
        gst_clear_object(&bin);
        return nullptr;
    }
    g_signal_connect(impl->state.parser, "autoplug-query", G_CALLBACK(requireAccessUnits), nullptr);
    GStreamer::SourceFactory::configureParseBin(impl->state.parser);
    g_signal_connect(impl->state.parser, "deep-element-added", G_CALLBACK(configureLossRecovery), nullptr);
    impl->url = signalingUrl;
    g_object_set(impl->state.peer, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, "latency", 80u, nullptr);
    gst_bin_add_many(GST_BIN(bin), impl->state.peer, impl->state.parser, nullptr);
    gst_object_ref(impl->state.peer);
    gst_object_ref(impl->state.parser);
    g_signal_connect(impl->state.peer, "pad-added", G_CALLBACK(receiveRtpPad), &impl->state);
    g_signal_connect(impl->state.parser, "pad-added", G_CALLBACK(exposeParsedPad), &impl->state);
    auto* source = new QGCWebRtcVideoSource(std::move(impl));
    g_object_set_data_full(G_OBJECT(bin), CONTEXT_KEY, source,
                           [](gpointer data) { delete static_cast<QGCWebRtcVideoSource*>(data); });
    return bin;
}

bool QGCWebRtcVideoSource::activate(GstElement* source, GstBus* bus)
{
    auto* context = static_cast<QGCWebRtcVideoSource*>(g_object_get_data(G_OBJECT(source), CONTEXT_KEY));
    if (!context) {
        return true;
    }
    auto& impl = *context->_impl;
    if (!bus || impl.thread || !impl.state.accepting.load()) {
        return false;
    }
    impl.state.bus = GST_BUS(gst_object_ref(bus));
    impl.state.epoch = impl.state.options->frameContexts->beginEpoch();
    impl.thread = std::make_unique<SignalingThread>(impl.url, &impl.state);
    return impl.thread->activate();
}

void QGCWebRtcVideoSource::deactivate(GstElement* source)
{
    if (auto* context = static_cast<QGCWebRtcVideoSource*>(g_object_get_data(G_OBJECT(source), CONTEXT_KEY))) {
        context->_impl->stop();
    }
}

void QGCWebRtcVideoSource::configureDecoder(GstElement* source, GstElement* decoder)
{
    if (!source || !decoder || !g_object_get_data(G_OBJECT(source), CONTEXT_KEY)) {
        return;
    }
    // Concealed pixels cannot establish interactive frame identity after packet loss.
    for (const auto* property : {"discard-corrupted-frames", "automatic-request-sync-points"}) {
        if (g_object_class_find_property(G_OBJECT_GET_CLASS(decoder), property)) {
            g_object_set(decoder, property, TRUE, nullptr);
        }
    }
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(decoder), "output-corrupt")) {
        g_object_set(decoder, "output-corrupt", FALSE, nullptr);
    }
}

void QGCWebRtcVideoSource::associatePresentedBuffer(GstElement* source, GstPad* pad, GstPadProbeInfo* info)
{
    auto* context = static_cast<QGCWebRtcVideoSource*>(g_object_get_data(G_OBJECT(source), CONTEXT_KEY));
    if (!context) {
        return;
    }
    auto& state = context->_impl->state;
    GstBuffer* buffer = gst_pad_probe_info_get_buffer(info);
    GstEvent* event = gst_pad_get_sticky_event(pad, GST_EVENT_SEGMENT, 0);
    if (!buffer) {
        if (event) {
            gst_event_unref(event);
        }
        return;
    }
    GstClockTime running = GST_CLOCK_TIME_NONE;
    if (event) {
        const GstSegment* segment = nullptr;
        gst_event_parse_segment(event, &segment);
        running = gst_segment_to_running_time(segment, GST_FORMAT_TIME, GST_BUFFER_PTS(buffer));
        gst_event_unref(event);
    }
    qint64 timestamp = -1;
    bool requestKeyframe = false;
    int width = 0;
    int height = 0;
    if (GstCaps* caps = gst_pad_get_current_caps(pad)) {
        if (!gst_caps_is_empty(caps)) {
            const GstStructure* format = gst_caps_get_structure(caps, 0);
            gst_structure_get_int(format, "width", &width);
            gst_structure_get_int(format, "height", &height);
        }
        gst_caps_unref(caps);
    }
    {
        QMutexLocker lock(&state.mutex);
        const auto now = QGCVideoFrameContextStore::monotonicMs();
        state.prune(now);
        auto packet = state.packets.find(running);
        if (state.accepting.load() && packet != state.packets.end() && packet->valid) {
            const auto identity = packet->identity;
            state.packets.erase(packet);
            if (!state.consumed.contains(identity) && !state.metadata.contains(identity)) {
                // Metadata and RTP travel independently. Hold this exact decoded buffer briefly;
                // never authorize an image retroactively after it has been presented.
                const QDeadlineTimer deadline(METADATA_JOIN_BUDGET_MS);
                while (state.accepting.load() && !state.metadata.contains(identity) &&
                       !state.consumed.contains(identity) && !deadline.hasExpired()) {
                    state.metadataReady.wait(&state.mutex, deadline);
                }
                if (state.accepting.load() && !state.metadata.contains(identity)) {
                    ++state.options->statistics->metadataJoinTimeouts;
                }
            }
            auto metadata = state.metadata.find(identity);
            if (state.accepting.load() && !state.consumed.contains(identity) && metadata != state.metadata.end()) {
                const auto provenance = metadata->metadata.value("provenance").toObject();
                // A lost resize keyframe can leave a decoder concealing at its previous size.
                if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_CORRUPTED)) {
                    ++state.options->statistics->corruptedFrames;
                } else if (width <= 0 || height <= 0 || provenance.value("encoded_width").toInt() != width ||
                           provenance.value("encoded_height").toInt() != height) {
                    ++state.options->statistics->rejectedDimensions;
                    const QSize expected(provenance.value("encoded_width").toInt(),
                                         provenance.value("encoded_height").toInt());
                    if (!expected.isEmpty() && state.requestedGeometry != expected) {
                        state.requestedGeometry = expected;
                        requestKeyframe = true;
                    }
                } else {
                    state.requestedGeometry = {};
                    timestamp =
                        state.options->frameContexts->insert(state.epoch, metadata->metadata, metadata->challengeMs);
                    ++state.options->statistics->associatedFrames;
                }
                state.metadata.erase(metadata);
            } else {
                ++state.options->statistics->missingMetadata;
            }
            state.consumed.insert(identity, now);
        } else {
            ++state.options->statistics->missingPacketContext;
        }
    }
    if (requestKeyframe && state.accepting.load()) {
        ++state.options->statistics->keyframeRequests;
        // Ask through negotiated RTCP feedback once per unresolved resize, not on every delta frame.
        gst_pad_push_event(pad, gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0));
    }
    buffer = gst_buffer_make_writable(buffer);
    GST_PAD_PROBE_INFO_DATA(info) = buffer;
    // An explicit invalid identity also prevents an old media PTS matching a context.
    if (!GStreamer::attachPresentedFrameContext(buffer, timestamp)) {
        state.fail("presentation_context");
    }
}
