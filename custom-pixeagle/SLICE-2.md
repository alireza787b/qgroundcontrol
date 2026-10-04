# Slice 2: authenticated video and read-only status

Status: **Linux read-only video technical checkpoint complete. Paused awaiting
the user's explicit confirmation to start slice 3.** The user deferred hands-on
tracker testing until the end of slice 3. Final fullscreen click and double-click
checks passed.
Slices 0 and 1 retain their completed local checkpoints. This is a local branch
checkpoint, not Windows/Android, full remote CI or aircraft qualification.

## Scope

The custom build adds authenticated WebSocket JPEG video to QGC's existing Fly
View, map/video, PiP and fullscreen layout. A separate per-aircraft client keeps
its session through selection changes. A companion-only client permits video
and diagnostics without PX4; it cannot acquire an aircraft identity and never
transfers its session to a newly connected QGC vehicle. Integration remains off
by default, with no companion traffic or reserved Fly View space while disabled.

The generic source takes an explicit in-memory cookie/Origin snapshot only for
native mode. It validates JSON/JPEG pairs, encoded size, ACK delivery challenges
and TLS/redirect policy. Ordinary generic HTTP/WebSocket transports retain their
existing unauthenticated behavior; the original PR branches remain unchanged.

Each JPEG receives a unique microsecond PTS linked to immutable backend
provenance in a bounded store. Decoded frames retain that PTS. The custom video
item owns pixels and context together through texture allocation and Qt's
`frameSwapped` boundary. Unknown context blanks the view; stale frames keep their
identity with freshness false. This proves application presentation, not physical
scanout or sensor exposure time. Qt 6.11.1's private CPU frame converter avoids
mandatory GPU upload/readback; the dependency must be rechecked on Qt upgrades.

Backend metadata includes instance/runtime, stream/source epochs, capture and
publication IDs/ages, exact encoded dimensions and variant. ACK challenge timing
provides a conservative network-delay bound without clock synchronization.
Geometry stays unverified. Tracker/following status is separately polled and
expires independently. No selection, following, gimbal, recording or provisioning
action is added in this slice.

## Validation

Local evidence is under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-2-2026-09-21/`.

- Backend: 948 focused tests, 26 existing no-PX4 contracts and 8 replay
  preparation tests passed. Schema remains 43 sections / 606 parameters.
  Route/candidate, compile and fatal lint gates passed. The backend checkpoint
  retains exact commands and previous repository CI qualifiers.
- Generic GStreamer transport: 118 Qt cases passed, 18 existing platform or
  headless skips. Real JPEG decoding, forced drops, large IDs, authentication
  isolation, token framing, redirects and TLS were exercised. See
  `transport/review.md` and its complete raw logs.
- Native displayed-frame tests: 40 Qt cases passed with both software rendering
  and threaded OpenGL under Xvfb/Mesa, no skips. A composed native WS → JPEG
  decode → QVideoSink → Qt presentation test decodes numbered pixels from the
  displayed image and compares their exact frame identity. Rotation/mirroring,
  clipping, visibility, stale state and scene/sink replacement are included.
  `surface/REVIEW.md` retains the independent technical review and commands.
- Stock and custom Debug and Release builds passed. Custom Unit/Integration
  passed 407/407; stock passed 405/405, both using standard `Flaky|Network`
  exclusions and retaining the three StockUI targets. Logs: `custom-tests.*`,
  `custom-confirm-tests.*`, `stock-tests.*` and their runner logs.
- An intervening full custom run timed out in `ObjectListModelBaseTest` after
  all 11 assertions/cases, including cleanup, had passed. Immediate isolated
  recheck and ten consecutive repetitions passed; a new complete run passed
  407/407 in 221.75 seconds. Preserve `custom-final-tests.*` as the failed run,
  plus `object-model-recheck*` and `object-model-repeat*`. The shutdown timeout
  was not reproduced or diagnosed; it is not silently relabelled as a pass or
  proven to be an upstream defect.
- The full-suite confirmation preceded the final QML-only Exit fullscreen
  button and its external-fullscreen stacking correction. `full-suite-qml.sha256`
  records the custom view revision. The subsequent
  builds, focused native/VideoManager/StockUI checks and real Release click
  smoke cover that last UI change; no C++ logic changed after the full run.
  Final focused results: custom 8/8 and stock 5/5, including FlyViewGeoUITest
  and all three StockUI targets (`final-click-focused*`, `stock-final-focused*`).
- Scoped locked hooks pass for the custom/native files and generic Fly View /
  VideoManager edits (`checkpoint-lint.log`, `operator-final-lint.log`,
  `stock-hook-lint-final.log`). Transport review records existing GStreamer
  CMake formatting and `Q_ASSERT(user_data)` hook failures present at HEAD.
  Whole-tree baseline lint remains qualified by slice 0; no green whole-CI
  claim is made.

First development checks exposed a destructor callback crash, offscreen-context
retention, Qt GPU conversion, missing receiver widget attachment and an incorrect
status field. These were corrected and the relevant tests/UI rerun. Failed
development logs remain available rather than being overwritten with success.

## Release interaction evidence

The isolated UI containers use network mode `none`, private settings and no host
devices. QGC runs the real Release executable. The synthetic companions wrap
production auth/context/publisher/JPEG/WebSocket routes around mocked observations.
No command, upload, arming, target or following action was performed.

| Scenario | Evidence and observation |
| --- | --- |
| Companion video without an aircraft | `synthetic-companion/ui/01-fly-video.png`; authenticated moving numbered frames. |
| Frozen and dropped stream | `02-frozen.png` and `07-dropped-stream.png`; retained pixels visibly delayed, not live. The early file named `03-fullscreen-frozen.png` did **not** actually enter fullscreen; that failure led to the no-aircraft fullscreen correction. |
| Source, shape and variant replacement | `04-source-resolution-swap.png`; camera-a → camera-b, 640×360 → 360×640, processed OSD → raw, with a new source/stream generation and correct aspect. |
| Vehicle ownership and session isolation | `multi-ui/ui/04-vehicle1-video.png`, `06-vehicle2-settings.png`, `07-vehicle2-video.png`, `08-vehicle1-return.png`; Vehicle 2 initially has no inherited session. After its own sign-in/verification its pixels say MOCK SYS 2; returning to Vehicle 1 retains its session and shows MOCK SYS 1. |
| Fullscreen, retained stale identity, compact status | `multi-ui/ui/09-fullscreen.png`, `10-fullscreen-frozen.png`, `12-portrait-pip-frozen-unhovered.png`; Vehicle 1 ownership and amber Delayed remain legible. |
| Actual PixEagle Core replay | `real-replay-ui/ui/05-recorded-video-main.png`; then final wording/layout in `final-replay-ui/ui/04-connected.png` through `08-portrait-video.png`. Real OpenCV file capture, CSRT construction, OSD, publisher, authentication and API run without PX4. |
| Restore stock source then native video | `final-replay-ui/ui/10-stock-source-restored.png`, `11-video-restored.png`; turning the video preference off removes the native source/banner, turning it on restores authenticated moving video. VideoManager tests verify that stock preferences survive. |
| Stock Release UI | `stock-release-ui/ui/05-fly.png`, `07-plan.png`; both disarmed mock vehicles, standard instruments/flight controls, and Plan View remain available. Minimal mock parameter/mission providers produce expected startup warnings; these are fixture limitations, not successful mission-transfer evidence. |
| Final fullscreen affordance and return from Plan | `click-final-ui/ui/01-fullscreen.png`, `02-exit-returned.png`, `03-double-click-returned.png`, `04-plan.png`, `05-video-after-plan.png`; both exit actions work, and returning from Plan restores the authenticated stream. |

The combined branch has no PixEagle flight controls. QGC's existing stock flight
tools remain available. Gray maps reflect unavailable map-network access;
native local video is independent. Still images document appearance; automated
presentation/transport tests and the recorded interaction sequence establish
the corresponding behavior. Physical GPU, mobile lifecycle and scanout remain
unqualified.

## Real replay and desktop handoff

The user selected the bundled recorded source. Backend preparation creates a
new private snapshot, inhibited configuration and generated viewer account;
neither the original PixEagle main checkout nor its configuration is modified.
The corrected snapshot is
`~/.cache/pixeagle-qgc-baseline/slice-2-2026-09-21/real-replay-v2`.
Its manifest pins source, video/config hashes and interpreter. Credentials are
stored separately in its mode-0600 `credentials.json`, never in this checkpoint.

The first real startup failed because its Host allowlist included a port.
Preparation now calls the actual runtime exposure-policy resolver and uses
`127.0.0.1` as the Host. The failed snapshot/log is retained. The corrected
runtime passed anonymous-denial, authenticated context/status and two distinct
decoded 640×480 frame checks with matching provenance; command/telemetry were
disconnected and following inactive. See the backend checkpoint and
`reports/qgc-slice2/real-replay-v2-runtime-proof.json`.

`validation/run-replay-desktop.sh` starts the actual prepared Core on loopback,
checks instance identity and no-aircraft state, then opens custom Release with
private settings and automatic aircraft connections disabled. It stops its own
backend when its QGC process exits. The first host launch identified a missing
Qt xcb-cursor library; the pinned local extraction and checksum are documented
in README. The successful desktop process was left open for the user, with
its binary hash in `desktop-launched-binary.sha256`. That launched build predates
the final Exit fullscreen button; double-click exits fullscreen in that process.
The desktop process subsequently exited; the launcher stopped its backend.
No shared-desktop automation is needed for the user's review or restart.

The scoped log/settings audit scanned 43 captured files and found no generated
demo password or emitted credential headers. Final isolated QGC replay logs
showed no QML/runtime errors. The host log reported geoclue location access
denied and unavailable map tiles; neither supplies the local video path.
Backend shutdown completed, then the installed MAVSDK destructor emitted a
Python-interpreter-teardown ImportError. Preserve `log-audit.json` and
`desktop-completed-*.log`; this is a shutdown qualifier, not a clean-log claim.
The backend reviewer reproduced the same warning using only the installed
MAVSDK 3.15.3 (matching slice 0) with an unconnected System retained until Python
exit; both dependency-only probes exited zero. The PixEagle constructor/stop
paths are unchanged from the baseline. No workaround was added to production.
All validation containers and desktop-owned processes were stopped after the
checkpoint; use the launcher to start a new user session.

Follow the [desktop demo instructions](README.md#desktop-recorded-video-demo-without-px4).
The main aircraft toolbar can say Disconnected while the separately authenticated
PixEagle stream works. “Live” means fresh delivery from the selected source; the
source in this demo is a recording with a target box already in its pixels.
Current tracking/following remain inactive. No backend connection request,
MAVSDK server, MAVLink2REST, dashboard, camera, service or deployment is needed.

## Operator feedback and next slices

Raw [first](reviews/slice-2-operator-raw.md) and
[follow-up](reviews/slice-2-operator-followup-raw.md) screenshot feedback are kept
unchanged, with a [final fullscreen addendum](reviews/slice-2-operator-fullscreen-raw.md).
The [disposition](reviews/slice-2-disposition.md) records changes and
deferrals. These are independent simulated operator reviews, not a human user
study or a guarantee of unbiased findings. The user's
[checkpoint decision](reviews/slice-2-user-checkpoint.md) defers hands-on tracker
testing to the end of slice 3 and requires a recap and explicit confirmation
before starting that slice. No human usability test has been completed.

Slice 3 adds target operations only after the displayed-frame geometry/action
contract is established; this slice intentionally reports geometry unverified.
Slice 4 owns following/gimbal controls and their separate SITL/hardware gates.
Slice 5 owns Linux/Windows/Android release qualification, installation and device
UX. No target/following control is enabled by passing this read-only checkpoint.

No dashboard files changed from PixEagle base
`989d9662173b364de03b307f4208e9d0ca96451f`: slice 0 retains 471 local dashboard
passes/build success, four existing lint errors, exact-base remote CI lint
failure/build skip, and the backend test-hygiene guardrail finding. This local
uncommitted combined branch has no new remote CI result.

## Reproduction

Use the same pinned environment, CMake arguments and dependency commits as
[BASELINE.md](BASELINE.md), with `QGC_UPDATE_TRACKED_DEPS=OFF`. Example custom
regression command (substitute the stock directory for stock coverage):

```bash
custom-pixeagle/validation/run-container.sh \
  cmake --build build/pixeagle-custom-debug --parallel 6
QGC_BASELINE_NETWORK=none custom-pixeagle/validation/run-container.sh bash -c '
  cd build/pixeagle-custom-debug
  python ../../.github/scripts/cmake_helper.py ctest \
    --build-type Debug --include-labels "Unit|Integration" \
    --exclude-labels "Flaky|Network" --jobs 4 \
    --junit-output /absolute/evidence/tests.xml \
    --ctest-output /absolute/evidence/tests.log'
```

Build Release in its separate `Linux` preset directory. The synthetic and actual
replay commands are documented in README and the backend checkpoint. Record
source/binary/evidence hashes in `slice-2-manifest.json`; historical source
archives for earlier slices remain separate from the current uncommitted state.
