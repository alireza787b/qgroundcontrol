#include "QGCVideoFrameContextStore.h"

#include <algorithm>
#include <atomic>
#include <chrono>
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

qint64 QGCVideoFrameContextStore::monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
