# Native WebRTC development checkpoint — 2026-10-19

Native WebRTC is under qualification. The normal application continues to
use authenticated WebSocket JPEG. No Pi configuration, operator setting or
public QGC release changes in this checkpoint.

## Implemented and checked

- Generic `QGCWebRtcVideoSource` with a dedicated Qt signaling thread,
  memory-only session credentials, verified WSS trust, asynchronous SDP/ICE,
  bounded metadata and explicit shutdown ownership.
- Receiver output joins exact RTP SSRC/timestamp to decoded segment running
  time. A custom buffer meta carries the existing context-store identity into
  `QVideoFrame`; it does not overwrite GStreamer scheduling timestamps.
- Delayed, absent, duplicate, ambiguous, expired and retired-epoch contexts
  cannot borrow another image's selection identity. A prior client challenge
  provides a conservative age bound without synchronizing machine clocks.
- ICE and frame challenges wait until the offer is sent. The backend rejects
  changing providers inside an existing signaling session.
- Five consecutive Linux start/present/stop runs passed for both H.264 and VP8.
  Each decoded at least 12 presented 16-bit barcodes and matched capture IDs
  to the exact frame context.
- The same receiver now passes with recorded test9 at 1280×720. Both codecs
  render through `PixEagleVideoItem`; a selection references the displayed
  capture ID and its exact retained backend geometry. The fixture does not
  execute a production tracking/retarget action or establish camera quality.
- Independent read-only client-config discovery preserves sign-in and aircraft
  verification sequencing. It uses the authorized endpoint/cookie/Origin,
  bounded responses, no redirects/cookie persistence, and rejects obsolete
  endpoint replies. Eligible receivers prefer native WebRTC, then retain JPEG
  after a named native failure until media identity changes. No operator switch.
- The QGC build passed. Of 446 Unit/Integration CTest suites with the standard
  Flaky/Network exclusions, 445 passed on the first run; the CMake multi-config
  fixture could not find Ninja on PATH. It passed when rerun with the project's
  locked tools on PATH. Focused media/selection tests passed after later edits.
- Changed-file pre-commit checks passed except pre-existing CMake formatting
  and lint failures, reproduced on pristine HEAD copies. No whole-file CMake
  reformatting or public argument renaming is mixed into this media change.

## Local verification

The developer-only `QGC_ENABLE_NATIVE_WEBRTC=ON` build option requires the
GStreamer WebRTC, SDP and RTP development components. It is off by default
until deployment packaging is qualified. This is not an operator setting.

With `QT_ROOT_DIR` pointing to the Qt installation required by build-config:

```bash
cmake --preset Linux-debug -B build/native-webrtc-port \
  -DQGC_CUSTOM_DIR=custom-pixeagle -DQGC_ENABLE_NATIVE_WEBRTC=ON
cmake --build build/native-webrtc-port -j 6
```

In the PixEagle checkout, with the optional GStreamer Python runtime and GCC
plugin available, start the loopback-only fixture:

```bash
PYTHONPATH=src .venv/bin/python tools/qualify_gstreamer_native_server.py --port 8197
```

Then in this checkout:

```bash
QGC_WEBRTC_TEST_SIGNALING_URL=ws://127.0.0.1:8197/ws/webrtc_signaling \
  ctest --test-dir build/native-webrtc-port \
    -R '^QGCWebRtcVideoSourceTest$' --repeat until-fail:5 --output-on-failure
ctest --test-dir build/native-webrtc-port \
  -R '^(GStreamerTest|PixEagleVideoItemTest|PixEagleVideoControllerTest|PixEagleClientTest)$' \
  -LE 'Flaky|Network' --output-on-failure
```

The fixture has no aircraft or tracking routes. It reuses local-compat
authorization and explicitly admits the experimental provider for this test;
it does not establish remote browser-session security or production readiness.

Local evidence: `~/.cache/pixeagle-qgc-baseline/native-webrtc-2026-10-19/`.
The development runtime is Linux GStreamer 1.24.2 with gst-plugins-rs 0.13.7
(`cdec77cad2d9b7348d800507bc6eeec5039717eb`). QGC's release SDK remains pinned
at 1.28.4. The extra development dependencies were installed in a user cache;
no Pi service or system installation was changed.

The current Linux evidence also contains the 30-case GStreamer policy and
orchestrator test run, the repeated recorded-video native receiver run, and
the pinned runtime manifest. The matching QGC revision is
`919bfc82fb1e2fbc24583f5ea56939ca8eefbdca` and the matching PixEagle revision
is `7d9fed9987fb7cd74019050c31a1ffefca2f873d`.

The dedicated [upstream receiver port](NATIVE-WEBRTC-UPSTREAM-PORT.md) uses
upstream `5540c98a05365c448d9c8f066c009a41b64deccb` and Qt 6.12.0. It retains the
new worker/pipeline lifecycle and ports transport/provenance into the new
components. Its fresh build, repeated native presentation tests and all 471
Unit/Integration suites pass with the standard exclusions. The checkpoint records
the initial failures and their resolutions separately.

The next [constrained-network checkpoint](NATIVE-WEBRTC-NETWORK.md) records
real namespace-shaped traffic and the outstanding low-bandwidth repairs.

## Remaining gates

1. End-to-end automatic transport/fallback tests through the custom controller,
   beyond the independent capability/endpoint/authentication regression checks.
2. Production selection/retarget actions, missing metadata, restart/revocation
   and failure/recovery tests; the retained-fixture selection is a prerequisite.
3. Measured congestion feedback, shared automatic per-client/aggregate policy,
   0.5/1/2/5/10 Mbps and load tests. A connected estimator hook is not adaptation
   evidence; loopback feedback does not establish a constrained-link operating range.
4. Bundle and probe complete runtimes on Windows, Linux and Android, followed
   by full release regression. Operators must not install development GStreamer.
5. Request the cooled Pi/camera for deployment and physical acceptance only
   after the recorded-video software gates pass. Pixhawk is a later regression.

PixEagle-device GStreamer remains optional. Browser WebRTC and qualified JPEG
must continue to work without it. The advanced provider is selected by runtime
capability, never by requiring beginners to tune codecs, ICE or estimators.

## Optional backend installation

PixEagle's existing optional GStreamer setup installs media dependencies and
repairs them during updates of GStreamer-enabled installations. It does not add
a codec/ICE/estimator prompt. Core installations retain browser aiortc WebRTC
and JPEG without GI/GStreamer. Managed plugins are discovered in the active
backend venv, including services. The QGC receiver runtime is a separate bundled
application dependency; operators must not install development GStreamer on
Windows. Native automatic adoption remains gated until these platform packages
and congestion/failure tests pass.

## Platform bundle policy

QGC's existing GStreamer Orchestrator remains the only package owner. When the
native WebRTC build option is off, no WebRTC/ICE/DTLS/SRTP/SCTP plugin is added
to the QGC bundle. When it is on, the same platform SDK allowlist adds
`webrtc`, `nice`, `srtp`, `dtls` and `sctp`. Component discovery, package verification
and runtime initialization check the required pieces. Missing components fail clearly;
normal JPEG/QGC builds remain available. The option is still developer-gated
until Windows, Linux and Android clean-runtime probes pass.
