#pragma once

#include <QtCore/QtGlobal>

/// Selection provenance requires an intact sync frame and uninterrupted dependent access units.
/// This gate neither retransmits packets nor changes GStreamer's congestion/decoder recovery.
class QGCRtpReferenceState final
{
public:
    struct Frame
    {
        quint64 generation = 0;
        bool complete = false;
    };

    Frame observe(quint32 ssrc, quint16 sequence, quint32 timestamp, bool marker, bool discontinuity)
    {
        const bool sameSource = _havePacket && ssrc == _ssrc;
        const bool contiguous = sameSource && sequence == _expectedSequence;
        const bool newFrame = !_havePacket || !sameSource || timestamp != _timestamp;
        const bool interrupted =
            _havePacket && (!contiguous || discontinuity || (newFrame && !_lastMarker) || (!newFrame && _lastMarker));
        if (interrupted) {
            invalidate();
        }
        if (newFrame) {
            _frameGeneration = _generation;
            // Joining mid-access-unit cannot establish that its first fragment arrived.
            _intact = contiguous && _lastMarker && !interrupted;
        } else if (interrupted || _frameGeneration != _generation) {
            _intact = false;
        }
        _havePacket = true;
        _ssrc = ssrc;
        _timestamp = timestamp;
        _expectedSequence = static_cast<quint16>(sequence + 1);
        _lastMarker = marker;
        return {_frameGeneration, _intact && marker};
    }

    bool observeAccessUnit(const Frame& frame, bool syncFrame, bool corrupted)
    {
        if (!frame.complete || frame.generation != _generation) {
            return false;
        }
        if (corrupted) {
            invalidate();
            return false;
        }
        if (syncFrame) {
            _referenceGeneration = _generation;
        }
        return verified(frame);
    }

    bool verified(const Frame& frame) const
    {
        return frame.complete && frame.generation == _generation && _referenceGeneration == _generation;
    }

    void invalidate()
    {
        ++_generation;
        _referenceGeneration = 0;
    }

    quint64 generation() const { return _generation; }

private:
    quint64 _generation = 1;
    quint64 _referenceGeneration = 0;
    quint64 _frameGeneration = 0;
    quint32 _ssrc = 0;
    quint32 _timestamp = 0;
    quint16 _expectedSequence = 0;
    bool _havePacket = false;
    bool _lastMarker = false;
    bool _intact = false;
};
