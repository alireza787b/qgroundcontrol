# Slice 4a checkpoint — normal following implementation

Date: 2026-09-27. Status: implementation and isolated PX4 SIH checks passed;
revised operator acceptance remains open. The latest compact-UI regression
results and stock GPS timing blocker are recorded below. This checkpoint does not
qualify real flight or the external camera.

Classic and local Smart tracking retain their slice-3 target workflow. PixEagle
now advertises the follower profiles compatible with the selected tracker output
and its effective/persisted profile state. The QGC Tracking options dialog offers that choice. The compact operating
panel offers a hold-to-confirm Start following action only after backend readiness. A
distinct Stop following action is available for the captured active session or
pending Start, including from the per-aircraft list in PixEagle settings. Stop
does not require a fresh video frame. The normal QGC flight, mission and
multi-vehicle controls remain present. Camera-gimbal profile compatibility is
advertised by the backend, but camera movement remains slice 4b.

The backend uses dedicated native routes documented in the
[following contract](../../PixEagle-qgc-integration/docs/apis/native-following-operations.md).
It checks the observed aircraft/target/source guard, fresh armed and in-air
state, selected profile and PX4 preflight before startup and again on the
flight-owner loop around Offboard entry. Target selection remains available on
the ground. Each pending Start has a captured attempt ID. Stop can cancel it; an
active Stop binds to its follow-session ID and aircraft UID. Cleanup with a
changed command identity or connection generation withholds a final setpoint
and Offboard stop from the rebound connection. Recorded video is deliberately
not authorized for PX4 following.

## Evidence

- QGC pinned Qt 6.11.1 custom Debug and Release builds succeeded in the
  baseline container. Release `--simple-boot-test` passed with isolated
  settings and network disabled. The container reports GStreamer 1.28.4;
  the baseline manifest separately records host GStreamer 1.24.2.
- Focused `PixEagleClientTest` and `PixEagleManagerTest` passed 2/2, including
  captured active Stop after video loss and pending Start cancellation.
  The final standard `Unit|Integration` suite with `Flaky|Network` excluded
  passed **411/411** (255.26 seconds); StockUI 3/3 also passed earlier.
- Backend focused auth, context, model, target, following, Offboard-safety,
  streaming, MAVLink identity, route and parameter suite passed **641/641**
  after the SIH fixes and airborne guard.
  The route/config gate passed 174/174; schema check passed 43 sections,
  606 parameters. Fatal Python lint and both worktrees' `git diff --check`
  passed. The generated API candidate inventory was refreshed.
- Scoped QGC pre-commit lint on the touched custom and test files passed.
  Repository-wide `just lint` was stopped after it reported existing unrelated
  shebang and clang-format failures across stock files. The pinned 3.12 tool
  environment was restored before the successful final builds and scoped lint.
- The pinned official PX4 SIH image
  `px4io/px4-sitl@sha256:fd6d93dc2705482aeb64ea26fdf16185d8a511010fdc53e26305f10d91855865`
  ran with `sihsim_quadx`. Pinned Mavlink Router commit
  `2362c620f483cef1edd574fb962a373a288e4b9e` routed PX4 UDP 14550
  to Mavlink2REST 0.11.25 (14569/8088), PixEagle (12550), and QGC (14560).
  PX4 supplied MAVSDK Server 3.12.0 directly on UDP 14540/gRPC 50051.
  PixEagle API was isolated on 127.0.0.1:8094 in the network namespace.
  The first full stack run exposed a symbolic MAVLink `base_mode` parsing bug
  and a stale MAVSDK Offboard setpoint; both were fixed with focused tests.
  The final run entered PX4 Offboard, published 20 Hz follower commands with
  zero failures for four seconds, stopped into Hold, canceled a pending Start,
  and stopped on synthetic target loss. An armed/takeoff/follow/land cycle
  proved the final ground Start guard: tracked/disarmed was refused, airborne
  was allowed, and landed was refused again. Logs live under
  `/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/sih-live-v1/logs/`:
  `armed-follow-publication.log`, `target-loss-probe.log`,
  `pending-cancel-probe.log`, and `airborne-gate-probe.log`.
- Private QGC screenshots showed the normal flight controls, profile selector,
  and captured Stop. They also revealed that the earlier UI offered Start on
  the ground, leading to the airborne gate. The raw simulated operator review
  is in [slice-4a-operator-raw.md](reviews/slice-4a-operator-raw.md).
- The [operator launcher](validation/run-sih-desktop.sh) passed `--check` for
  both the fixed image and a 282-frame synthetic loop of bundled `test9.mp4`.
  It verified backend login, the local dashboard and QGC UDP MAVLink delivery,
  and the test9 run advertised the copied, checksum-checked VisDrone26m model
  from the previously accepted Full AI fixture.
  Both checks removed their private Docker network and containers. The browser build
  uses API port 8094; the manifest is in the private slice-4 cache. Only the
  demo snapshot uses PixEagle's `trusted_lan_legacy` bind inside that Docker
  bridge, with authenticated API port 8094 published on host loopback and an
  explicit dashboard origin. This does not alter the backend worktree defaults.
  The [operator steps](OPERATOR-TEST-4A.md) remain to be performed on the desktop.

## Remaining gate

The user needs to run the repeatable normal-mode operator test and report
feedback; no human acceptance is inferred from the private screenshots. The
launcher uses isolated QGC settings; its GUI path relies on the user's desktop
session. A native video-input selector still needs its own typed catalog/apply
contract; users can select the PixEagle input in the dashboard and QGC observes
source epochs. The Ethernet Topotek RTSP/SIP camera is the following 4b
checkpoint and will require the user's physical connection.

The first desktop attempt reported low-contrast backend address text and a
restart failure after Ctrl-C twice. Its saved QGC/backend logs showed no
application crash; the named SIH containers and dashboard server had survived
the interrupted launcher and held ports 8094/3040. The backend address now
remains an enabled, read-only QGC text field while signed in. Private captures
`13-user-fix-settings.png` and `14-user-fix-signed-in.png` show dark address text
on white before and after sign-in. The launcher now serializes runs, reclaims
only its labeled stale containers/network and recorded child processes, ignores
further interrupts during cleanup, and explains unrelated port conflicts.
Repeated Ctrl-C, forced parent termination followed by restart, concurrent-run
rejection, and normal `--check` all passed with a fake QGC process. The user
should retry the desktop workflow before slice 4b starts. The unedited feedback
is in [slice-4a-user-feedback-raw.md](reviews/slice-4a-user-feedback-raw.md).

## Second desktop feedback — address, video and compact controls

On September 27 the user reported an empty aircraft backend address, unreadable
dark-theme placeholder, black video after connecting, accelerated test9 replay,
and excessive configuration controls in the collapsed panel. Investigation
reproduced the black video in a private Release UI connected to the real SIH
Docker bridge: authentication and aircraft verification passed, but WebSocket
requests returned 403 (`websocket_origin_not_allowed`). A localhost URL had
caused QGC to omit Origin even though the backend saw a Docker bridge peer.

Changes:

- Every new client and blank address now resolves to `http://127.0.0.1:5077`.
  Saved aircraft endpoints are restored when their UID arrives, without copying
  companion credentials or associations. Address text also observes later
  endpoint changes. The demo records the actual simulated UID and fills its
  separate `8094` endpoint before QGC starts.
- Backend/dashboard placeholders use QGC's text-field foreground palette. The
  address remains readable and selectable when signed in.
- QGC always supplies the endpoint's origin (without path) on media WebSocket
  requests. Host, Origin, authentication and vehicle-association checks remain
  enforced. The launcher now checks authenticated JPEG delivery over the same
  host-to-container route as QGC, beyond its earlier HTTP startup check.
- The private image-sequence fixture now uses `videorate` for timestamps and
  `appsink max-buffers=1 drop=true sync=true` for clock pacing and bounded
  consumer backlog. This follows the [GStreamer multifilesrc documentation](https://gstreamer.freedesktop.org/documentation/multifile/multifilesrc.html).
  The existing backend VIDEO_FILE timing implementation was not changed.
- Collapsed controls show mode, tracker/model and follower status. Configuration
  is under Options; Cancel and Start/Stop remain primary actions. An inline
  Smart model selector is also available when no model has been configured,
  so Smart setup does not require a working Smart mode first.
- Demo cleanup bounds the wait for an unresponsive QGC process, and audit logs
  now follow the selected runtime directory.

Validation artifacts are under the private slice-4 cache:

- `feedback2-final-full-tests.log`: **411/411** Unit/Integration tests passed
  with Flaky/Network excluded, including StockUI coverage. Endpoint regressions
  cover new/cleared defaults and delayed aircraft UID restoration; media tests
  assert the explicit origin excludes the API prefix.
- Debug and Release builds passed. Scoped pre-commit and `git diff --check`
  passed; the final settings notification change received an incremental build
  and focused QML/manager checks.
- `feedback2-replay-timing.json`: 310 reads spanned **10.30 seconds**, including
  the 282-frame loop boundary. A 1.2-second consumer pause advanced source time
  by **1200 ms**, demonstrating latest-frame delivery without accumulated lag.
- `feedback2-ui/`: private dark-theme captures of the populated demo endpoint,
  readable blank placeholder and restored default, verified aircraft, live
  replay, collapsed/expanded controls, and inline model selection. Selecting
  VisDrone9m and entering Smart succeeded; it ran on CPU with an explicitly
  reported CUDA fallback in this Docker fixture. A Classic selection reached
  the backend, then lost its target and exposed the loss/Cancel state. This is
  not sustained tracker-accuracy or Smart-follow qualification.

Repeat the [operator checkpoint](OPERATOR-TEST-4A.md). Human acceptance of 4a
remains open; hardware/gimbal 4b has not started.

## Third desktop feedback — verification action in Fly View

The user's screenshot showed authentication complete but aircraft verification
pending. The saved runtime audit had successful sign-ins and no request to
`/api/v1/integration/connection`. Fly View gave a verification instruction but
provided no action; it also displayed unavailable tracking/following controls.

Fly View now offers **Verify vehicle** and **Settings** on the connection card.
The verification action retains the pressed client and rechecks it at click,
then uses the existing guarded connection method. Before readiness, the status
reads **Connection required** rather than **No video**. The target panel appears
when connection and video are ready; an available Stop remains visible even
if either becomes unavailable. No authentication or association guard changed.

Private Release screenshots in `feedback3-ui/` reproduce the exact sequence:
`02-unverified.png` shows the action with no unavailable target panel; clicking
that action produced `03-verified-video.png`, with live test9 and compact
controls. These are coding-agent checks, not human operator acceptance. The
Debug/Release builds and scoped lint passed; regression results are recorded
in `feedback3-tests.log` under the private slice-4 cache.

## Fourth desktop feedback — compact operating panel

The user's latest session reported that the workflow worked but rejected the
widget's organization. Its saved audit contains four successful target starts
and one successful native follow Start. PixEagle stopped following after
`classic_tracker_measurement_uncertain`; PX4 returned from Offboard to Hold.
The audit does not show a user Stop request in that session.

The revised panel has a small PixEagle title and keeps Classic/Smart visible and uses separate target and
aircraft icons, active names, concise state words and palette indicators.
Green requires fresh confirmed Tracking/Following; external camera `lost`
never becomes green merely because its tracking session remains active.
Unknown state is amber Checking. Captured-session Stop remains available when
status or video freshness is lost. Following command errors stay visible across
readiness polling and clear on the next action or session reset.

Start uses stock `QGCDelayButton` and captured aircraft/target/profile intent.
Stop remains a single action. The gear opens a standard `QGCPopupDialog` with
the relevant tracker or model and compatible follower selectors, with aligned
Settings and Dashboard shortcuts. Catalog
changes invalidate captured selections; keyboard follower activation follows
the same guard. Tap-to-target stays in Settings. Dedicated drag grips move both
the operation and connection panels without initiating target gestures;
viewport changes clamp placement, and the dialog can reset it. Normal
connection status moves to the standard toolbar while preserving stock
indicators; its drawer provides Verify, Dashboard and Settings.

The [AI operator review](reviews/slice-4a-compact-operator-review.md) and
[gimbal architecture review](reviews/slice-4a-gimbal-mode-review.md) are labeled
simulated expert feedback, not human field acceptance. External Camera
Classic/Smart remains distinct from local Classic/AI models. No backend
provider rewrite was needed in this correction; actual camera controls and
provider/source selection remain the 4b checkpoint.

Private Xvfb checks used real authenticated SIH services, without desktop input:

- Test9: sign-in, explicit verification, paced video, compact idle states,
  modal tracker/follower options, palette/icon rendering and drag grips passed.
  A short Start press sent no native Start request and displayed Hold to Confirm.
  Target loss changed to amber Lost; sustained follow on this moving clip is
  still a human acceptance task.
- Fixed target: QGC takeoff, hold Start, Offboard, green Following, single-click
  Stop, Hold and Land passed. The audit recorded one accepted native Start at
  03:36:14 UTC and one accepted native Stop at 03:36:28 UTC on September 27.
  These prove UI/command behavior in simulation, not tracker accuracy in flight.
- The moved panel stayed within a resized 900 × 600 viewport. The toolbar
  drawer opened correctly. Touch-device acceptance remains untested locally.
- Focused PixEagle suites initially passed 6/6. The added persistent-error test
  then exposed a test assertion before asynchronous logout completed; waiting
  for logout fixed that test without a production change. The corrected
  PixEagleClientTest passed.
- The first full run passed 409/411. The remaining GPSReceiverSettingsTest
  mobile check exceeded its 1000 ms timeout; it reproduced on both stock and
  custom builds with the simulation stopped. This is a recorded stock baseline
  timing issue. The subsequent corrected full run passed **411/411** in
  259.39 seconds. The earlier stock/custom failures remain archived rather
  than being erased by the passing rerun. The final title/shortcut build also passed **411/411** in 258.86 seconds.
  Custom Debug/Release builds and scoped lint passed.

Evidence is under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/feedback4-ui/`
and the `feedback4-` build/test files in its parent. Earlier failed screenshots
are retained separately from the final captures. User test logs are archived
by the launcher under the operator desktop history directory on restart.

### Scripted moving target follow-up

The user preferred the compact UI and requested the small PixEagle title,
Settings/Dashboard shortcuts and a known-motion target. The title and aligned
shortcuts are implemented in the panel/dialog. The new
`validation/generate-target-path.py` generates a 640 × 480, 30 fps, 30-second
sequence with a 120-pixel square marker and smooth, timed center/right/left/up/
down waypoints. Its bottom-center timecode identifies the displayed frame.
The full-frame source-pixel and normalized bounding boxes, rational timestamps,
waypoints, tool version and per-PNG checksums are in `ground-truth.json`.

`run-sih-desktop.sh --video target-path` prepares a separate private runtime
under the launcher's lock; existing fixed-image/test9 config is preserved.
The pipeline remains clock-paced with old-frame dropping. Two independently
generated 900-frame sequences matched; bounds, dwells and loop position were
checked. The launcher `--check` passed login, simulated telemetry/UID and
actual authenticated JPEG delivery. Evidence is `target-path-check.log` and
`target-path-generator-validation/validation-final.json` in the private cache.
This synthetic pattern targets Classic tracker and SIH command-path testing;
test9 remains the Smart detector scene. Scripted image motion is independent
of aircraft motion, so it cannot qualify closed-loop follower flight behavior.
