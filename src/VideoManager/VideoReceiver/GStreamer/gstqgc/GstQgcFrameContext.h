#pragma once

#include <gst/gst.h>

namespace GStreamer {

inline constexpr char PRESENTED_FRAME_CONTEXT_META[] = "QgcPresentedFrameContext";

/// Attach a Qt presentation identity without changing RTP/pipeline scheduling timestamps.
inline bool attachPresentedFrameContext(GstBuffer* buffer, gint64 timestampUs)
{
    static gsize registered = 0;
    if (g_once_init_enter(&registered)) {
        static const gchar* tags[] = {nullptr};
        const auto* info = gst_meta_register_custom(PRESENTED_FRAME_CONTEXT_META, tags, nullptr, nullptr, nullptr);
        g_once_init_leave(&registered, info ? 1 : 2);
    }
    if (registered != 1 || !buffer) {
        return false;
    }
    auto* meta = gst_buffer_get_custom_meta(buffer, PRESENTED_FRAME_CONTEXT_META);
    if (!meta) {
        meta = gst_buffer_add_custom_meta(buffer, PRESENTED_FRAME_CONTEXT_META);
    }
    if (!meta) {
        return false;
    }
    gst_structure_set(gst_custom_meta_get_structure(meta), "timestamp-us", G_TYPE_INT64, timestampUs, nullptr);
    return true;
}

}  // namespace GStreamer
