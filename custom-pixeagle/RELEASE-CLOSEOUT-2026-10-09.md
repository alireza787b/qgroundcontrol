# Operator feedback and release closeout — 2026-10-09

## Changes

The movable tracking and camera panels now resize using pointer coordinates in
an unchanged viewport. Growth is proportional, bounded to 100–200% and the
available viewport, and clamped smoothly at edges. The handle remains a small
visual mark with a reachable hit area. Panel sizes update their Options dialog
scale. Positions, independent sizes and camera-open preference are saved in the
application's isolated UI settings; there is no backend configuration duplicate.
Reset restores panel position and size.

Opening Settings, Options or PiP does not discard the camera-open preference.
Returning to Fly View restores the panel, but navigation, focus loss, movement
and resizing still cancel manual gestures and pending flight confirmations.
Hidden compact panels do not overwrite saved geometry to fit their temporary
viewport. Visible panels fit when their available viewport shrinks.

## Verification and artifacts

The debug build and changed-file locked pre-commit gate passed. The camera-client
suite passed actual pointer resizing, proportional scale, release stability,
viewport fit, navigation preference and existing Stop/ownership checks.
All 446 Unit/Integration CTest entries passed (226.03 seconds), using the
standard Flaky/Network exclusions. The three focused suites also passed.
Invalid-video-URL diagnostic expectations and a scoped host-dependent Bluetooth
fixture warning were corrected in tests; production behavior was unchanged.
The independent [AI panel review](reviews/2026-10-09-panel-closeout-raw.md) is
retained separately from physical operator acceptance.
Exact results and workflow/artifact hashes are recorded in the final Desktop
bundle manifest; packaging success is separate from physical acceptance.

PixEagle main includes measured JPEG pacing, detail-preserving adaptation and
resolution-aware OSD repairs. Patch 7.4.1 aligns backend, Dashboard and setup
metadata with the release. Factory and existing saved profiles remain unchanged.
The Pi normal updater preserved its custom configuration, GStreamer and Full AI
environment. Full-quality 720p/20 FPS is not established by the JPEG receipt probe.

## Future work and boundaries

Native WebRTC in QGC is a recorded TODO, not part of this release. Reuse existing
PixEagle signaling and GStreamer WebRTC, qualify RTP-to-presented-frame identity,
session recovery and bundled Windows/Linux/Android plugins, then advertise
interactive support. Keep qualified authenticated JPEG as a fallback. Firmware
camera passthrough and broad constrained-link/physical motor-failure qualification
retain their existing gates.

No QGC PR or public QGC release is created. The existing fork is GitHub-public;
custom installers remain qualification artifacts. PixEagle is released publicly.
Physical workflow, onboard ground and real-flight qualification are distinct from
software/packaging checks. The Pi's historical undervoltage/throttling observation
requires checking its supply/cable before further hardware qualification.
