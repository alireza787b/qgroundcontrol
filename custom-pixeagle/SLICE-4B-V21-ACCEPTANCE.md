# V21 operator review and next qualification slice — 2026-10-03

The operator reports the recorded-video workflow worked, including Chase.
Accept the completed-Stop/target-selection repair and recorded-SIH operator
workflow for this checkpoint. This does not complete all 4b.4, optimal tuning,
physical camera, or release qualification. No production/configuration changes
were made during this review.

## Final operator evidence

Review root:
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-03/recorded-sih-operator-v21`.
Preserve both historical backend logs, security audit and trace files; the
`pixeagle.log` link alone does not cover the earlier launch preflight.

- All 38 audited tracking-start actions succeeded; all six tracking-stop actions
  succeeded. Native follower selection succeeded. No recurrence of the stale
  completed-Stop flag was found.
- All 277 recorded Offboard publications succeeded: Position sessions contained
  17 and 77 records, and Chase contained 183. Maximum recorded inter-publication
  gaps were 52.15, 53.69 and 54.79 ms respectively. These are trace observations,
  not network or physical motor-stop measurements.
- Position produced yaw guidance with zero horizontal/vertical velocity. One
  Position run stopped after tracking uncertainty with confirmed Offboard stop;
  another completed native operator Stop successfully.
- The Chase session began at 12:53:10 Tehran time, ran about 9.25 seconds, and
  showed independently observed yaw from +2.44° to −97.72°. Recorded commands
  remained within 0.5 m/s forward and 22.25°/s yaw; vertical commands were zero.
  Chase retargeting advanced generation 40 → 41, passed through COASTING and
  REACQUIRING, and restored confirmed target authority. These observations
  establish retarget/publication/response, not optimal gains or visual convergence.
- An earlier Chase start at approximately 12:51:13 failed its post-Offboard
  readiness check because the tracker became prediction-only during startup;
  cleanup ran. Position and Chase later returned to confirmed Hold on
  `classic_tracker_measurement_uncertain`, consistent with this profile's
  explicit local immediate-handoff policy. Do not erase these guarded failures
  or report all Start attempts as successful.
- Several CSRT tracking-only recovery attempts timed out or remained ambiguous.
  They are target-quality/recovery limitations, not the repaired selection
  refusal. Normal follower loss policy differs from the bounded gimbal overrides.
- The available QGC log contains a second-instance refusal; it is not a complete
  UI trace of the accepted session. The backend audit/traces provide the action
  and publication evidence above. Do not claim new mouse/touch/suspension coverage.

Altitude guidance remained disabled for normal followers; altitude safety remained
enabled. No gain/default changes are justified by these uncoupled video/SIH
observations. Automated Visual Centering evidence remains in the separate
[v21 repair checkpoint](SLICE-4B-RECORDED-SIH.md); this operator trace contains
Position and Chase, not a Visual Centering publication session.

## Next slice: remaining 4b.4d network/load and recovery qualification

Use the existing isolated launcher and production API/command path. Preserve the
accepted UI, factory defaults and 350 ms manual movement lease. Reuse existing
64-case software link evidence where source and scenarios remain unchanged.

On 2026-10-04 the focused qualification harness passed **68 tests**. The
loopback-only `SharedLinkRelay` applies one aggregate byte budget across
concurrent streams, and one production-fixture case exercised ten authenticated
gimbal-control renewals alongside ten real JPEG WebSocket frames. It also verifies
deterministic latency/reset behavior and fresh-connection recovery after a reset.
Its manifest is
`~/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/link-v1-shared-relay/manifest.json`.
This is transport-fixture evidence; it does not qualify the QGC desktop UI,
radio, Pi, camera, or motor.
The existing Dashboard `VideoStream` suite passed **25/25**, and the focused
QGC PixEagle suite passed **4/4** after making hidden application state stop
the native video controller like suspension.
Single-vehicle sign-in now automatically performs the identity verification
request once matching command and telemetry identities are available; duplicate
or multi-vehicle associations still require the explicit guarded action.

1. Expand the production relay run to bounded jitter/reconnect cases and capture
   exact source/config/dependency
   hashes, workload and actual observations. Avoid changing the host's
   unrelated network routes.
2. Exercise QGC and Dashboard against constrained video/control links, blocked
   capture and local Smart load. Verify bounded backlog, captured frame selection,
   stale-state display, continuous publication while authority is retained, and
   no delayed obsolete target/movement after reconnect.
3. Exercise QGC focus loss, suspension, panel closure and endpoint/vehicle changes.
   Mock-camera manual Stop must retain the receipt-to-dispatch ≤100 ms and lease
   expiry ≤400 ms gates under blocked inference. Record transmission and physical
   response separately; a poor network must not extend the movement lease.
4. Qualify ordinary follower recovery using the existing TargetContinuity policy
   and per-follower overrides. Compare the current immediate-handoff baseline
   with an explicit bounded-recovery test overlay, including manual reacquisition,
   repeated retargets, exhausted budgets and safety/pilot precedence. Do not add
   another bypass or silently alter saved/default policies. Use a separate normal
   altitude-enabled SIH overlay to verify climb/descent signs and boundary guards;
   retain factory yaw-only Position.
5. The failed-teardown cleanup retry is now implemented with explicit
   ownership/identity and concurrency tests. A failed cached Stop outcome remains
   visible; retained publishers are never treated as stopped. The completed-clean
   Stop behavior is preserved.
6. Run focused gates during implementation, then the affected regression suites
   and a fresh logged checkpoint. State the tested operating envelope and
   separate operator acceptance, AI review, software timing and SIH response.

### 4b.4d software gate renewed

The authenticated camera-link qualification was rerun against the shutdown-
retry source. Its impairment is deterministic sender delay/drop/jitter, not
kernel radio shaping. It passed the unchanged limits: Stop dispatch under 1 ms,
lease expiry about 353–355 ms, 20 Hz heartbeat gaps about 50–55 ms,
reordered/late packets with zero obsolete motion, two-client arbitration, and
blocked capture/inference handling. The recovery-focused controller, continuity
and follower-default tests passed **245** cases. Evidence is under
`~/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-03/link-v6-final/manifest.json`
and `recorded-sih-v21-recovery-tests.log`. The subsequent full backend CI unit
sweep passed **3,463** tests, 41 optional skips and 14 subtests; see
`retry-unit-final-v2.log`.

This closes the renewed software timing/recovery gate only. Shared radio
throughput, full QGC transport under contention, application suspension,
process-death/device watchdog behavior, physical camera motor stopping, and Pi
CPU/thermal limits remain open. No lease, gain, default, or safety limit was
extended to pass the gate.

The failed-teardown repair is now explicit and tested: a failed Offboard stop keeps
admission blocked; a later explicit Stop retries the owned cleanup; only a
confirmed retry releases the stopping state. The retry preserves the original
handoff reason and lifecycle guard. No automatic retry or operation resume occurs.

No physical camera is needed for the remaining software steps. Final physical camera/control/tracker
acceptance and Pi-to-camera network/process-death motor behavior remain deferred
until the operator returns. Software/mocked failures cannot close those gates.

## Remaining release sequence

| Checkpoint | Remaining work |
| --- | --- |
| 4b.4 | Shared-link/load/recovery above; final physical camera/failure retest; retain horizontal physical-mount limitations |
| Normal followers | Sustained MC attitude-rate envelope and fixed-wing following/airspeed or opted-in ground-speed qualification remain blockers |
| 5 | Portable provisioning/harness, renewed Linux release, Windows x64 installer, Android qualification, dependency/checksum/compatibility/rollback evidence, coherent commits and reviewed PRs |
| 6 | Inventory Pi/OS/accelerator/router/PX4/camera, prepare trust/recovery/logging, onboard ground acceptance with CB active and no PixEagle aircraft dispatch |

Integration work remains uncommitted and unpublished. Neither PixEagle main nor
Windows/Android release artifacts are updated. Real-flight qualification remains
later scope. Keep [release readiness](RELEASE-READINESS.md) blockers visible.
