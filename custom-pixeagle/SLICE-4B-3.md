# Slice 4b.3 — responsive camera control

Status: camera-v5 controls and tracking accepted by the operator on 2026-10-01.
Mount direction and follower command geometry remain unqualified. The first Ethernet bench established
working RTSP and camera telemetry, but the operator reported delayed movement,
misleading cancellation errors and an unwanted five-second hold limit.

## Control behavior

The camera runtime keeps one provider and transport. Manual gestures use a
separate camera executor, independent of frame capture and local AI inference.
The native client retains current input only, renews it every 100 ms, and uses a
captured gesture identity and increasing sequence. The backend expires input
after 350 ms without renewal. Begin prepares ownership; the first acknowledged
update permits movement. Delayed input from a released gesture cannot restart it.

The joystick changes speed with displacement. This Topotek adapter uses one
dominant axis; simultaneous pan/tilt is not qualified. Manual takeover cancels
camera tracking once, before movement, and does not automatically resume it.
Release, neutral, focus loss, panel closure, owner changes and input expiry stop
the gesture. Aircraft following and manual movement exclude each other.

Roll controls remain visible below the joystick when supported. The disclosure
shows measured camera angles and their coordinate frame. Stale readings show a
dash. Zoom buttons do not imply measured zoom telemetry; unsupported zoom
readings are omitted. No guessed vertical-mount axis transform is applied.

The old bounded action interface remains compatible. QGC requires the advertised
manual gesture capability for continuous control. Stop retains the captured
camera identity and does not require fresh ordinary video telemetry.
The legacy finite-pulse interface retains its defensive hold limit; negotiated
manual gestures have no duration cutoff while their lease is renewed. Dashboard
and QGC both use that renewable interface when advertised.

## Tracking and follower choices

The physical camera test profile starts with `Gimbal` and `gm_velocity_chase`.
Camera Classic/Smart are provider modes; local Classic/Smart remains available
through the Advanced tracking setting. Returning to an engine restores its session
selection. A change in Advanced tracking applies and saves the startup engine;
ordinary tracker and mode choices remain runtime-only. A failed save is shown
separately from the runtime choice and can be retried without replacing a target.

Follower compatibility comes from tracker output and follower requirements.
Camera-angle tracking uses compatible angle followers. The UI shows follower
identity and readiness without an aircraft. Selecting a compatible profile is
allowed in companion-only mode; Start retains aircraft and association gates.
This bench reports
“Bench: following disabled.”

Selections are remembered per engine during the backend session. Explicit
tracker/model choices remain authoritative; a missing remembered tracker is
reported instead of silently substituted. Switching tracking engines does not
automatically select or start a different follower. Camera settings remain in
the existing `GimbalTracker` configuration group.

## Verification and limits

The camera-v2 operator test found that Camera Classic and Smart selections
returned a generic target/connection conflict. An authenticated dry-run on that
bench reproduced `frame_evicted` by 0.4 seconds: every camera frame unnecessarily
retained full analysis pixels and exhausted the 128 MiB selection cache before
the advertised 1.5-second deadline. Camera-owned tracking now retains the exact
streamed frame and verified geometry without a redundant analysis copy. The
unused video variant cannot consume selection-cache space. Local tracking still
retains its clean analysis pixels; its high-resolution memory-pressure limit is
documented separately. QGC identifies an expired frame and asks for a new tap.
The initial revised backend video/target regression passed 256 tests. A paced
1920×1080-analysis / 1280×720-display, 30 fps check retained a camera frame
through 1.45 seconds and expired it by 1.60 seconds. It verifies software
retention under delayed selection, but live camera-v3 continuous delivery still
evicted a 1.2-second frame. Camera-v4 pins frames sampled for WebSocket delivery,
bounded to three frames per client and 96 MiB total. The existing QGC
latest-frame acknowledgment prevents new delivery from displacing the image while
the operator acts. A live camera-v4 dry-run using that acknowledgment accepted
clicks at 0, 0.4, 0.8 and 1.2 seconds without executing them; a 1.6-second
click was rejected. Physical Camera Classic/Smart acceptance followed in the
camera-v5 operator session.
Frames older than the age limit fail closed.

The camera-v4 operator session exposed a separate QGC input gate: opening the
camera-control panel disabled target selection. The panel now leaves selection
active on the unobscured video. Opening the panel alone no longer blocks a
future aircraft-follow Start; active manual movement remains blocked by backend
readiness and ownership. The panel also offers compatible follower profiles in
the camera-only bench without enabling Start. The joystick tilt sign was
corrected against the operator's observation and prior camera image-motion
evidence; physical direction remains an operator acceptance check.

The operator then accepted camera-v5 manual controls, Camera Classic/Smart
tracking, local tracking selection and compatible follower choices. The brief
red flash during manual takeover has no matching failed camera action in the
saved backend audit; the exact UI text was not recorded. The routine
“Taking manual control…” state now uses ordinary text color. A new operator
review will determine whether any other warning still flashes.

QGC Options now reads the same backend circuit breaker as the Dashboard.
Its compact control requires an explicit confirmation before permitting
PixEagle aircraft commands. The collapsed follower area shows a short
“Commands blocked” or “Follower test” indication only for confirmed fresh
state. Follower Test is never presented as aircraft following. Older backends
show an unavailable, read-only control. Neither QGC Settings nor the app stores
a duplicate breaker value; camera Stop and ordinary QGC flight controls are
outside this breaker. Changing tracking engines moved to Advanced tracking in
both QGC and Dashboard.

Evidence is retained under
`~/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/`.

| Gate | Result | Evidence in that directory |
| --- | --- | --- |
| Debug and Release builds | Passed | `control-repair-{debug,release}-build.log` |
| Scoped QGC pre-commit checks | Passed | `control-repair-lint.log` |
| Focused client, target and camera tests | 3 suites passed | `control-repair-qgc-focused.log` |
| QGC Unit/Integration, excluding Flaky/Network | 413/413 passed, 436.08 s | `control-repair-all-tests.{log,xml}` |
| Combined backend gate | 632 tests + 14 subtests passed | `response-fix/backend-tests.{log,xml}` |
| Final camera regression gate | 115 passed | `response-fix/camera-final.log` |
| Explicit Stop/expiry timing regressions | 4 passed | `response-fix/camera-timing.{log,xml,json}` |
| Tracker/config schema gate | 606 passed | `response-fix/schema.log` |
| Engine restoration and compatibility | 357 passed; final hook gate 263 passed | Backend engine-restoration checkpoint |
| Dashboard | 63 suites, 488 tests; ESLint and production build passed | `dashboard-gesture-{tests,lint,build}.log` |
| Camera-v4 dashboard build | Reuses validated camera-v2 build, backend port 8095 | `dashboard-camera-v2-build.log` |
| Camera selection regression | 256 backend tests and full-resolution paced retention passed | `frame-retention-backend-broad.log`, `frame-retention-camera-fullres.log` |
| Camera action contract | 134 backend tests passed, including camera selection and mode validation | `frame-retention-camera-actions.log` |
| Delivered-frame retention | 341 backend tests passed on final regression rerun | `frame-retention-delivery-broad-rerun.log` |
| Live camera-v4 delayed dry-run | 0–1.2 s accepted without execution; 1.6 s rejected | `camera-v4/logs/frame-selection-ack-dryrun.jsonl` |
| Idle-bench launcher | Verified and retired camera-v3, started camera-v4 with a harmless test binary | `camera-v4-launcher-smoke.log` |
| Revised QGC Debug/Release builds and scoped lint | Passed | `frame-retention-qgc-{debug,release}-build.log`, `frame-retention-qgc-lint-final.log` |
| Revised QGC target/client/video tests | 3/3 passed | `frame-retention-qgc-focused.log` |
| Revised QGC full Unit/Integration | 410/413 on first host run; all three failed cases passed when rerun with corrected host test environment | `frame-retention-qgc-full.log`, `frame-retention-qgc-env-rerun.log`, `frame-retention-qgc-bluetooth-host-isolated.log` |
| Camera-v5 backend target/follower/runtime regression | 207 passed | `camera-v5/logs/panel-fix-backend-tests.log` |
| Camera-v5 QGC Debug/Release and scoped lint | Passed after local missing-library/cache-tool build repair | `camera-v4/logs/panel-fix-{debug,release}-build.log`, `camera-v5/logs/panel-fix-{debug,release}-final-build.log`, `camera-v5/logs/panel-fix-qgc-lint-final.log` |
| Camera-v5 focused QGC client/target/camera tests | 3/3 passed | `camera-v5/logs/panel-fix-qgc-focused.log` |
| Camera-v5 full QGC Unit/Integration | 412/413 host run; sole Bluetooth warning failure passed in isolated rerun | `camera-v5/logs/panel-fix-qgc-full.log`, `camera-v5/logs/panel-fix-qgc-bluetooth-direct.log` |

Counts overlap between focused and combined runs; they are not an additive
unique-test total. The full-suite container exited 0, verified in Docker events;
the outer tool session reported a termination signal after the completed run.
The complete CTest log and JUnit both record 413 passes and zero failures.
For the revised repair, the first host-only full run lacked Ninja on `PATH`,
missed one unrelated GPS UI timeout by about 50 ms, and saw a host BlueZ warning
that the strict Bluetooth test did not expect. GPS and the CMake fixture passed
on targeted rerun with the pinned Ninja available. Bluetooth passed all 14 cases
when rerun with only `qt.bluetooth.bluez.warning` suppressed in that test process.
No application source or production logging rule was changed for these reruns.
The camera-v5 run reproduced the same host BlueZ warning in the strict Bluetooth
CTest process. Its 14 cases passed with only that warning filtered in a direct
test invocation. All PixEagle test targets passed in the full run.

Under simulated blocked capture/inference, authenticated in-process Stop
dispatch took 3.46/3.04 ms against the 100 ms limit. Expiry Stop occurred
368.13/362.63 ms after the last accepted renewal against the 400 ms limit.
These measure fake-provider dispatch, not network or physical motor stopping.
The mock QGC application held a gesture for six seconds, reversed direction,
and reported stopped after release (`control-ui/held-six-seconds.json` and
`released.json`). Tests also cover delayed/reordered requests, release during
takeover, expired input, stale responses, two-client ownership, transmission
failure and following/lifecycle exclusion.

The fresh physical backend started with Gimbal/Camera Classic, fresh raw body
angles and live 1920×1080 RTSP. Read-only QGC inspection verified both camera
mode choices, visible disabled follower identity, roll buttons and angle
disclosure. No target was selected and no movement, Home or tracking-mode
command was issued. Startup did not load a local inference model. Local Smart
remains an installed option; its physical load/latency acceptance is pending.
The bench records one existing Starlette deprecation; the private Xvfb run
also reports locale/speech-service warnings, with no QML binding errors.

[Raw AI UI feedback](reviews/slice-4b3-control-repair-raw.md) is separate from
operator acceptance. Private screenshots remain in `control-ui/` and
`desktop-v2/`. Camera-v4 source checksums and its changed configuration are in
`camera-v4/demo-manifest.json` and `camera-v4/bench-manifest.json`; the latter
explicitly supersedes recorded-video configuration validation for this bench.
The new camera-v5 snapshot and bench configuration hashes are in its matching
manifests. It uses a fresh login and keeps aircraft commands inhibited.

Use [the operator steps](OPERATOR-TEST-4B-3.md) for the original camera-v5
credentials and launcher. That launcher runs the older camera-v5 snapshot and
does not demonstrate the new safety or advanced-setting contract. The accepted
camera movement and target acquisition do not qualify aircraft following or
mount geometry.

After software checks, the operator repeats short axis/release tests, then local
tracking and camera-owned Classic/Smart acquisition. Angle/mount and compatible
follower qualification with SIH remains 4b.4; Windows/Android and release
qualification remains slice 5.
