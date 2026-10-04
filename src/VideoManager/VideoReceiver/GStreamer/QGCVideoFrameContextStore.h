#pragma once

#include <optional>

#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QMutex>

/// Bounded frame metadata keyed by the microsecond timestamp carried by QVideoFrame.
class QGCVideoFrameContextStore final
{
public:
    struct Entry
    {
        quint64 epoch = 0;
        /// Earliest local receipt bound; native ACK streams use the echoed token's send time.
        qint64 receivedMonotonicMs = 0;
        QJsonObject metadata;
    };

    explicit QGCVideoFrameContextStore(qsizetype capacity = 128);

    quint64 beginEpoch();
    void endEpoch(quint64 epoch);
    /// Returns a process-unique timestamp, or -1 if this source epoch has expired.
    qint64 insert(quint64 epoch, const QJsonObject& metadata, qint64 receivedMonotonicMs = -1);
    std::optional<Entry> lookup(qint64 ptsUs) const;
    bool isCurrentEpoch(quint64 epoch) const;
    static qint64 monotonicMs();

private:
    const qsizetype _capacity;
    mutable QMutex _mutex;
    QMap<qint64, Entry> _entries;
    quint64 _nextEpoch = 0;
    quint64 _epoch = 0;
};
