# Native WebRTC upstream port — 2026-10-19

The integration worktree is `/home/alireza/qgroundcontrol-pixeagle-webrtc-port`,
branch `integration/native-webrtc-upstream-2026-10-19`. Its merge uses QGC upstream
`5540c98a05365c448d9c8f066c009a41b64deccb` and integration parent
`a6a1bb8a7814985436a734fb070cd89e0ce0fbbd`. The previously tested
branch can advance by fast-forward to this reviewed merge.

## Changes

- Use upstream's `DecodingBranch`, `RecordingBranch`, pipeline generations and
  worker-thread teardown. Do not restore the older monolithic receiver.
- Snapshot authenticated transport options into `StartConfig`; apply changed
  credentials only to a new receiver attempt. Stop the source client before
  tearing down its pipeline.
- Associate native RTP metadata in the decoded sink probe. Preserve the
  presented-frame context across the upstream `GstQgcFrameMap` rename without
  changing GStreamer clock timestamps.
- Preserve HTTP MJPEG, authenticated WebSocket JPEG and the external-source
  integration while retaining upstream's stock receiver lifecycle.
- Extend upstream's component and plugin policy for optional WebRTC. Native
  bundles and runtime initialization require ICE, DTLS, SRTP and SCTP plugins;
  native-disabled builds retain their existing plugin requirements.
- Follow upstream's Qt 6.12.0 requirement. Include its TaskTree dependency in
  managed Qt installation, so QML plugin discovery works on fresh machines.

## Development verification

Evidence is under
`~/.cache/pixeagle-qgc-baseline/native-webrtc-2026-10-19/port-*`.

- 55 GStreamer CMake/policy tests passed, including native and stock policies.
- 42 Qt setup tests passed.
- The fresh native-enabled build passed after resolving the initial link errors.
- Native-disabled configure and syntax checks for the receiver, source factory
  and decoding branch passed without WebRTC development components.
- Three consecutive recorded test9 presentation runs passed for both H.264 and
  VP8, including exact barcode/context and retained selection geometry checks.
- Generic GStreamer, receiver and receiver-component suites passed.
- All 471 Unit/Integration CTest suites passed with `Flaky|Network` exclusions
  through the standard CI helper (`port-qgc-final.log`, `port-qgc-final.xml`).
- Two client-test synchronization assumptions were repaired: TLS rejection now
  checks disconnection and absence of application data; context recovery waits
  for the authoritative runtime rather than retained video availability. Client
  authentication and recovery behavior were unchanged.
- The first full run also reported invalid-context warnings in the unchanged
  GeoMap UI suite. That suite passed in the renewed full run. No warnings were
  suppressed; retain this intermittent observation for platform qualification.
- Changed-file lint passed after the final formatting corrections. The native
  loopback fixture was shut down cleanly; it opened no hardware connection.

The local toolchain uses Qt 6.12.0, GStreamer 1.24.2 and the previously pinned
optional development plugins. Host XKB headers come from the existing local
cache. `OPENGL_opengl_LIBRARY` resolves to the installed `libOpenGL.so.0` because
this host lacks its development symlink. These are development environment
inputs, not product defaults or installation instructions.

## Reproduce and continue

Use `tools/.venv/bin` on PATH and the development GStreamer plugin path recorded
in the runtime manifest. The native build is `build/native-webrtc-port`; the
native-disabled configure is `build/native-webrtc-off-check`.

The standard CI helper runs the final gate from the native build directory:

```bash
../../tools/.venv/bin/python ../../.github/scripts/cmake_helper.py ctest \
  --junit-output results.xml --ctest-output results.log --jobs 6 \
  --build-type Debug --include-labels 'Unit|Integration' \
  --exclude-labels 'Flaky|Network'
```

Next qualify automatic controller-level fallback, production selection and
revocation/restart behavior, then measured constrained-link adaptation. Package
and probe complete platform runtimes before requesting the Pi/camera checkpoint.

Native transport remains developer-gated. Constrained-link adaptation,
production selection/recovery, clean Windows/Linux/Android packages and Pi
qualification remain required. No hardware or Pi configuration was changed.
