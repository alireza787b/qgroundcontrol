# Shared restart and recorded-video checkpoint — 2026-10-03

The shared restart implementation and the recorded-video/normal-follower software
checkpoint pass. The operator is away from the camera; final physical camera
acceptance is deferred. No physical camera or aircraft was commanded in these
recorded-video runs. Integration changes remain uncommitted and unpublished.

For the later actual recorded-tracker-to-PX4 allowance and fresh five-minute
handoff, see [the recorded SIH checkpoint](SLICE-4B-RECORDED-SIH.md). The synthetic
publication runs in this report are not the same evidence as recorded-image
tracking.

## What changed

Normal Linux and isolated SIH launchers use one backend supervisor. Exit code 42
replaces only the owned backend, preserves router/PX4 sidecars and historical
logs, and starts a new runtime without resuming operations. Unmanaged and
unqualified Windows deployments do not advertise restart support. SIH validates
the complete loopback command route before startup and every replacement and
restores the flight-command block.

QGC's expanded Backend settings now offer **Restart PixEagle…**, with confirmation
bound to the current instance/runtime/configuration. The shared authenticated
action retains permissions, backup, audit, idempotency and inactive-operation
guards. Pending changes come from canonical backend configuration. QGC observes
a new ready runtime for up to 60 seconds and never replays the restart request.
Saved sign-in uses the system credential store; transient retries are bounded.

The ordinary policy remains `local_only`. Remote installations can explicitly
use `authenticated_admin_https` with verified HTTPS and exact trusted proxy
addresses. Arbitrary forwarded headers are not trusted. Isolated SIH retains its
explicit lab policy. See the backend
[restart contract](/home/alireza/PixEagle-qgc-integration/docs/apis/native-configuration.md)
and [HTTPS deployment guide](/home/alireza/PixEagle-qgc-integration/docs/setup/production-remote-reverse-proxy.md).

The existing profile preparer now supports test4/test9 without any camera provider,
CSRT startup, installed local Smart models and compatible normal followers.
Video input, engine, manual camera ownership and installation geometry stay
independent. With RTSP hardware later, local tracking can retain optional camera
controls; manual camera movement remains excluded during aircraft following.
The [camera configuration guide](/home/alireza/PixEagle-qgc-integration/docs/trackers/06-integration/camera-workflows.md)
lists the exact configuration differences.

## Renewed evidence

| Gate | Result |
| --- | --- |
| Backend CI Unit | 3,419 passed; 41 optional-dependency skips; 14 subtests passed |
| Backend Integration | 195 passed |
| Focused API/security/parameter/launcher checks | 183 passed |
| Final supervisor/profile route checks | 23 passed |
| Schema and candidate-route inventory | Passed |
| Dashboard | 490 tests / 64 suites; lint and production build passed |
| QGC | Incremental build, locked applicable lint hooks and typed QML lint passed; 414/414 Unit/Integration tests passed with `Flaky\|Network` excluded |
| Real supervised restart in isolated recorded-video SIH | Ready in 7.161 seconds; stale confirmation refused with 409; pending changes cleared; sidecars unchanged; no operations resumed; commands blocked |
| Local recorded-video CSRT | Acquisition, retarget and Stop confirmed through authenticated native actions |
| Local recorded-video Smart | Actual test9 detections and accepted selection, followed by confirmed tracking and Stop; CPU fallback explicitly observed |

The Smart probe initially acknowledged its selected frame too early and reused a
backlogged stream across a mode change. Correcting the probe to retain the
displayed frame and use a fresh stream yielded an accepted selection at a
352 ms frame age. Production stale-frame protections were kept unchanged.
This establishes acquisition, not sustained detection accuracy or GPU performance.

Fresh production guidance/publication tests used independently observed PX4 SIH
pose, with synthetic image-target inputs separate from the acquisition tests:

| Follower | Successful publications | Result |
| --- | ---: | --- |
| `mc_velocity_position` | 199 | Direction, yaw/altitude response and confirmed Hold passed |
| `mc_velocity_chase` | 198 | Direction, yaw/altitude response and confirmed Hold passed |
| `mc_velocity_distance` | 199 | Direction, lateral/altitude response and confirmed Hold passed |

All 596 publications succeeded. Test overlays enable altitude guidance; factory
Position remains yaw-only. No gains or numeric limits changed. The unchanged
gimbal horizontal/vertical and software-link evidence is retained in
[4b.4 closeout](SLICE-4B-4.md) and [camera-free qualification](SLICE-4B-4D.md).
Neither synthetic targets nor recorded video establish physical closed-loop convergence.

## Provenance and log locations

QGC base commit: `2e6190bafc4f4796e29ae24cbd1d27f73b96d07d`.
Backend base commit: `989d9662173b364de03b307f4208e9d0ca96451f`.
Both include uncommitted integration changes; a base commit alone does not
identify the tested source. Each private SIH profile has a source/checksum
manifest, and the normal SIH evidence records its driver/configuration/dependencies.
Current Debug executable SHA-256:
`53582dcf4a73a35c0dd99f204bc519b65490401a8aced87f61872b3ce0830186`.

Suite logs are under `~/.cache/pixeagle-qgc-baseline/`, named
`restart-recorded-unit-release.log`, `restart-backend-integration-ci.log`,
`restart-contract-final.log`, `restart-route-profile-final.log`,
`restart-schema.log`, `restart-api-check-final.log`, `restart-dashboard-*.log`,
`restart-qgc-lint-sdk-final.log`, `restart-qml-lint-sdk.log` and
`restart-recorded-qgc-final.log`.

Runtime evidence is under
`~/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-03/`:
`normal-{position,chase,distance}-final/result.json`,
`recorded-sih-validation/logs/{recorded-tracking-probe,smart-fresh-stream-result}.json`
and `recorded-sih-final-validation/logs/restart-probe-result.json`.
Earlier failed probe attempts remain retained. Raw
[AI review feedback](reviews/restart-recorded-raw.md) is separate from operator acceptance.

## Remaining sequence

1. Continue 4b.4d shared-link bandwidth/contention, load and suspension checks.
   Existing blocked-inference Stop/lease-expiry software timing passes remain
   evidence, not physical motor-stop proof or Pi thermal qualification.
2. Retest camera controls, Camera Classic/Smart, retargeting and failure behavior
   when the operator returns. Horizontal physical installation remains untested.
3. Close sustained MC attitude-envelope and fixed-wing airspeed/ground-speed
   follower qualification before claiming those modes release-ready.
4. Slice 5: portable harness, renewed Linux release/package acceptance, Windows
   x64 installer, Android, compatibility/rollback docs and reviewed PRs. The older
   Linux package does not include this new restart implementation.
5. Slice 6: Pi/router/PX4/camera inventory and deployment, then onboard ground
   acceptance with the flight-command block active. Real-flight qualification
   remains later scope.

No hardware action is required now. An optional prepared recorded-video session
is documented in [recorded SIH operator steps](OPERATOR-RECORDED-SIH.md).
