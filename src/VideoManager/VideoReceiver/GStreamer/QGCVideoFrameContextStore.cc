#include "QGCVideoFrameContextStore.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>

#include <QtCore/QMutexLocker>

namespace {

qint64 nextTimestampUs()
{
    static const auto origin = std::chrono::steady_clock::now();
    static std::atomic<qint64> last{0};
    const qint64 elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - origin).count();
    qint64 previous = last.load(std::memory_order_relaxed);
    for (;;) {
        if (previous >= std::numeric_limits<qint64>::max() / 1000) {
            return -1;
        }
        const qint64 next = std::max(previous + 1, elapsed);
        if (last.compare_exchange_weak(previous, next, std::memory_order_relaxed)) {
            return next;
        }
    }
}

}  // namespace

QGCVideoFrameContextStore::QGCVideoFrameContextStore(qsizetype capacity)
    : _capacity(std::clamp<qsizetype>(capacity, 1, 1024))
{}

quint64 QGCVideoFrameContextStore::beginEpoch()
{
    QMutexLocker lock(&_mutex);
    _entries.clear();
    _epoch = ++_nextEpoch;
    _verifiedFrames = _presentedFrames = 0;
    _epochStartedMs = monotonicMs();
    _lastPresentedPts = _lastPresentedMs = -1;
    _lastPresentedAgeBoundMs = -1;
    return _epoch;
}

void QGCVideoFrameContextStore::endEpoch(quint64 epoch)
{
    QMutexLocker lock(&_mutex);
    if (_epoch == epoch) {
        _entries.clear();
        _epoch = 0;
    }
}

qint64 QGCVideoFrameContextStore::insert(quint64 epoch, const QJsonObject& metadata, qint64 receivedMonotonicMs)
{
    QMutexLocker lock(&_mutex);
    if (!epoch || _epoch != epoch) {
        return -1;
    }
    const qint64 ptsUs = nextTimestampUs();
    if (ptsUs < 0) {
        return -1;
    }
    const qint64 now = monotonicMs();
    const qint64 received = receivedMonotonicMs < 0 ? now : std::min(receivedMonotonicMs, now);
    _entries.insert(ptsUs, Entry{epoch, received, metadata});
    ++_verifiedFrames;
    while (_entries.size() > _capacity) {
        _entries.erase(_entries.begin());
    }
    return ptsUs;
}

std::optional<QGCVideoFrameContextStore::Entry> QGCVideoFrameContextStore::lookup(qint64 ptsUs) const
{
    QMutexLocker lock(&_mutex);
    const auto entry = _entries.constFind(ptsUs);
    if (!_epoch || entry == _entries.cend() || entry->epoch != _epoch) {
        return std::nullopt;
    }
    return *entry;
}

bool QGCVideoFrameContextStore::isCurrentEpoch(quint64 epoch) const
{
    QMutexLocker lock(&_mutex);
    return epoch && _epoch == epoch;
}

void QGCVideoFrameContextStore::markPresented(qint64 ptsUs, const Entry& receipt)
{
    QMutexLocker lock(&_mutex);
    if (!_epoch || ptsUs < 0 || ptsUs <= _lastPresentedPts || receipt.epoch != _epoch) {
        return;
    }
    _lastPresentedPts = ptsUs;
    _lastPresentedMs = monotonicMs();
    ++_presentedFrames;
    const auto age = receipt.metadata.value("provenance").toObject().value("capture_age_ms");
    _lastPresentedAgeBoundMs =
        age.isDouble() && std::isfinite(age.toDouble()) && age.toDouble() >= 0
            ? age.toDouble() + std::max<qint64>(0, _lastPresentedMs - receipt.receivedMonotonicMs)
            : -1;
}

QJsonObject QGCVideoFrameContextStore::presentationStatistics() const
{
    QMutexLocker lock(&_mutex);
    if (!_epoch) {
        return {};
    }
    const auto now = monotonicMs();
    return {{"version", 1},
            {"epoch", QString::number(_epoch)},
            {"elapsed_ms", now - _epochStartedMs},
            {"verified_frames", static_cast<qint64>(_verifiedFrames)},
            {"presented_frames", static_cast<qint64>(_presentedFrames)},
            {"last_presented_age_bound_ms",
             _lastPresentedAgeBoundMs < 0 ? QJsonValue() : QJsonValue(_lastPresentedAgeBoundMs)},
            {"since_last_presentation_ms", _lastPresentedMs < 0 ? QJsonValue() : QJsonValue(now - _lastPresentedMs)}};
}

qint64 QGCVideoFrameContextStore::monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
