# Camera-free qualification — 2026-10-04

The subsequent [October 3 restart/recorded-video checkpoint](SLICE-4B-RESTART.md)
renews the backend, Dashboard and QGC regression gates, confirms actual local
CSRT/Smart acquisition and adds three normal-follower SIH runs. The operator's
physical camera retest remains deferred until return; earlier failed evidence
below is retained rather than replaced with a broader qualification claim.

The camera is unavailable. No physical camera or aircraft was commanded and no
operator acceptance was inferred. Existing accepted UI stays unchanged. This
checkpoint advances software/SIH and package preparation while the fresh camera
retest remains pending.

## Evidence and scope

The four current gimbal SIH cases already passed; unchanged cases were not
repeated. Chase and Vector each exercised synthetic horizontal and vertical
mounts through production guidance/publication, with **2,210 successful sends**,
independent simulated yaw/altitude observations, retarget/loss restoration,
bounded submitted commands and confirmed Hold. See [v13 closeout](SLICE-4B-4.md).
Physical horizontal installation and image/aircraft convergence are not proven.

The new `validation/run-normal-sih-evidence.py` reuses the isolated gimbal
launcher, immutable source snapshots and pinned PX4 image. It supplies normalized
image targets to production dispatch and publication rather than camera angles.
Multicopter cases use `sihsim_quadx`; fixed-wing uses `sihsim_airplane`. Neither
exposes a host MAVLink port. Source, driver, dependencies, configuration, actual
publications and independently observed poses are retained per run.

| Normal follower | Actual SIH result | Qualification limits |
| --- | --- | --- |
| `mc_velocity_position` | Direction, yaw/climb/descent response, publication and confirmed Stop passed | Synthetic target input; no tracker acquisition or optics |
| `mc_velocity_chase` | Direction, yaw/climb/descent response, publication and confirmed Stop passed | Large turns under held image error do not establish tuned gains |
| `mc_velocity_ground` | Lateral/forward signs, lateral response, publication and confirmed Stop passed | Configured vertical guidance remains disabled |
| `mc_velocity_distance` | Lateral/vertical signs, lateral/climb/descent response, publication and confirmed Stop passed | No physical distance calibration |
| `mc_attitude_rate` | Body-rate signs, finite bounded commands, simulated pitch response, publication and Stop passed | Actual pitch envelope failed; do not treat dispatch success as flight qualification |
| `fw_attitude_rate` | Original startup failed; a separate corrected airplane fixture now reaches 50.15 m with CB active | No follower/Offboard qualification; typed airspeed acquisition and sensorless guidance remain pending |

All five multicopter cases recorded **843 successful publications, zero failed
publications**, and confirmed Hold after Stop. Attitude SIH reached absolute
pitch **43.05°** against the configured **35°** envelope; its limited dispatch
pass must not be reported as full flight-mode acceptance.

Private normal-case evidence is under
`~/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/normal-sih-*/`.
Failed harness/startup attempts remain retained. The normal test overlay enables
altitude guidance and removes the attitude follower's target-altitude offset;
it does not change production gains, limits or defaults. A canonical CB probe
after confirmed Stop is separate from active-follow CB-transition qualification.

## Boundary repairs and remaining limitations

The MC attitude-rate path now checks fresh measured attitude after filtering
and at every publication. BODY/Euler lookahead scales angular rates while
retaining thrust and existing limits. Cached REST samples cannot manufacture
freshness. Missing, stale or out-of-envelope readings trigger immediate handoff;
concurrent Stop callers share teardown. Gains and numeric limits are unchanged.

A final matching 1.2-second SIH stimulus recorded **48 successful sends**, zero
failures, maximum pitch **31.736°** and roll **15.089°**, with confirmed Stop.
The longer 3-second reversal reached **35.171°** pitch and then confirmed Hold:
**sustained attitude-rate use remains a release blocker**. No tolerance or gain
change masks this failed test. An unchanged Vector/Vertical regression added
**550 successful sends**, zero failures. The small subsequent cross-profile
failure-policy correction passed its focused test; SIH snapshot hashes remain
recorded separately. See [normal SIH evidence](validation/NORMAL-SIH-EVIDENCE.md).

Fixed-wing now refuses missing/stale/underspeed airspeed rather than silently
using ground speed or configured cruise speed. This does not require a physical
sensor on every aircraft: typed airspeed acquisition remains open. The operator-requested ground-speed
proxy now has an explicit off-by-default PixEagle configuration checkbox; it
does not remove the fixed-wing live-qualification gate. The current live-profile qualification
restriction stays in place. A reproducible startup-only probe found the pinned
SIH airframe configured `SIH_T_MAX`, while airplane dynamics consume
`SIH_F_T_MAX`. Setting only simulator `SIH_F_T_MAX` to 6 N reached **50.15 m**;
CB stayed active, with **zero follower commands and no Offboard request**.
This resolves that fixture failure without qualifying fixed-wing following.

Fresh defaults are test4 recorded video, CSRT/PixEagle, no gimbal provider or
controls, `mc_velocity_position` yaw-only following, altitude safety on and CB
on. Existing saved profiles remain explicit; adoption uses Config Sync preview.
The measured-attitude and airspeed repairs, schema changes and exact evidence
are recorded in the backend
[boundary checkpoint](/home/alireza/PixEagle-qgc-integration/docs/reporting/agent-ops/codex-modernization/checkpoints/2026-10-02-follower-boundary-repair.md).

## Network/load software gate

**68 focused tests passed**, including 60 authenticated control renewals and
60 JPEG WebSocket frames sharing one 128 KiB/s loopback relay budget, bounded
JPEG ownership/backlog, raw measurement freshness and independent flight-thread
publication. Capture/inference-blocked Stop dispatch measured **0.70/1.32 ms**
from route receipt; lease expiry measured **351.5/367.3 ms**, with late packets
unable to revive movement. Independent 20 Hz publications had maximum gaps
**50.39/50.48 ms**. The 350 ms movement lease was unchanged.

Evidence: `~/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/link-v3-sustained/`.
The backend [checkpoint](/home/alireza/PixEagle-qgc-integration/docs/reporting/agent-ops/codex-modernization/checkpoints/2026-10-03-qgc-4b4d-shutdown-retry.md)
records exact commands, source hashes and limits. This is a user-space loopback
relay, not radio or kernel shaping; it does not qualify Pi CPU/thermal stress,
camera-device stopping, process-death watchdog behavior, or physical motor
response. Full QGC transport and actual constrained-link qualification remain
open.

## Use this gimbal camera as an ordinary camera

Keep its RTSP stream, select **Tracking runs on: PixEagle** in Dashboard Advanced
settings, select a local Classic tracker or installed local Smart model, and
explicitly select a compatible ordinary `mc_*` follower. For an RTSP-only
installation set `GimbalTracker.ENABLED: false` and `CONTROL_ENABLED: false`;
apply/restart while following is inactive. This disables the camera provider,
not video. Optional camera controls can instead remain enabled under PixEagle
ownership while tracking stays local. Stop any camera-firmware tracker or
external control application first.

See [camera scenarios](CAMERA-SCENARIOS.md) and the backend
[configuration guide](/home/alireza/PixEagle-qgc-integration/docs/trackers/06-integration/camera-workflows.md).
One-time installation geometry remains in canonical Dashboard configuration;
no QGC duplicate setting or universal fixed-camera transform was introduced.

## Remaining sequence

Complete sustained MC attitude-envelope and actual fixed-wing/sensorless
qualification, remaining radio/Pi/process-loss evidence, and portable harness
provisioning. When the camera returns, run the fresh
[camera retest](OPERATOR-RETEST-4B-4.md) and physical loss/Stop checks. Slice 5 still
requires Linux package acceptance, Windows/Android artifacts and reviewed source
publication. Slice 6 requires actual Pi/router/PX4/camera inventory and a
command-blocked onboard ground test. See [release readiness](RELEASE-READINESS.md).

## Final configuration-checkbox regression

After the operator-requested ground-speed fallback, full backend Unit passes
**3,391** with 41 optional dependency skips; Integration passes 195. Dashboard
passes 490 tests/64 suites, build and lint. The renewed software link 68-test
gate records a stable source manifest. The rebuilt QGC run passes 413/414 plus an
isolated passing retry of its unchanged GPS visibility test; the failed full
run remains retained. No current SIH snapshot is relabelled to conceal the
subsequent ground-speed receipt/metadata additions. Their focused/full gates
are separate from actual sustained attitude/fixed-wing qualification.
