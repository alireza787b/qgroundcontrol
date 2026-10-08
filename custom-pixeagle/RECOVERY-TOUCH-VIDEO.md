# PixEagle video recovery and touch controls

PixEagle owns its external video source when the integration and PixEagle video
settings are enabled. QGC's separate stock video-stream switch does not need to
be enabled. Stock QGC video remains controlled by its normal settings when the
PixEagle source is disabled.

The custom controller retries connection and decode failures with bounded
backoff. A presented frame resets the backoff. A connection that receives no
presented frame is restarted after the presentation watchdog; reconnecting
restores video and status only and never resumes tracking, camera movement or
aircraft following.

The video identity follows the authenticated endpoint, session, backend
instance/runtime and camera stream epochs. Aircraft telemetry connection
generations do not restart an unchanged video source.

Floating PixEagle panels can be moved and resized from the lower-right handle.
Each panel keeps its own scale between 100% and 200%. The handle and frequent
controls use touch-sized targets, while the panel contents scale together.
Moving or resizing cancels an active camera gesture or pending follow
confirmation. Double-click the panel grip to restore its position; use the
Options dialog reset action to restore panel placement.

The new factory streaming profile uses Automatic delivery with a 1280×720
ceiling, 20 FPS ceiling and JPEG quality 70. Automatic starts at 75% delivery
dimensions and 12 FPS, then adapts per client from measured write/ACK/render
feedback. Capture and 640×480 analysis remain separate. Existing saved
profiles retain their values until Config Sync preview/apply accepts migration.

The 8,000 kbps aggregate JPEG budget is a configured payload limit, not a
guaranteed link capacity. Use Dashboard's Low bandwidth profile or lower the
advanced limit for constrained links. WebRTC remains gated until its complete
authenticated RTP-to-presented-frame identity and congestion-control chain is
qualified; JPEG remains the compatible interactive path.
