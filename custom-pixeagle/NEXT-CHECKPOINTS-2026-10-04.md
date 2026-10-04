# Accepted camera checkpoint and remaining release/deployment plan

## Acceptance and evidence

The operator accepted camera-final-v28 on 2026-10-04. Its configuration uses the
real Ethernet camera, camera-owned tracking, vertical mounting and isolated PX4
SIH. This is bench workflow acceptance, not real-flight approval.

The saved evidence contains 796 successful publication records and no failed
publication records, 10 successful native target actions, 81 successful camera
actions, and successful native follow Start/Stop actions. Publication records
identify Vector (794 intents; two startup records have no intent). Chase was
available, but these logs do not demonstrate an active Chase run. Continuity
records include ACTIVE, COASTING and REACQUIRING. The authenticated observational
connection request is recorded. Final physical failure behavior and optimal
gains remain unqualified; no gains change follows from this acceptance alone.

Evidence remains private under
`~/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/camera-final-v28/`.
The tested binary checksum is in [the handoff](FINAL-CAMERA-SIH-HANDOFF.md).

## Work order and completion gates

| Checkpoint | Work | Completion evidence / operator dependency |
| --- | --- | --- |
| 4b.4d physical closeout | QGC-to-backend loss, Pi-to-camera loss, camera reboot, backend failure/restart, focus/suspension and Smart/capture load; record actual motor stop behavior | Software dispatch/lease gates plus separately measured network/motor response. Brief camera/bench operator assistance is needed for physical observations; do not disconnect or kill an active following session without a coordinated test |
| 5a source and reproducibility | Replace private-cache provisioning with parameterized setup, identify model/tool/dependency revisions, classify unrelated QGC edits, consolidate related repair commits, capture effective configuration and rollback | A clean fresh checkout reproduces tests and the approved profile. Preserve the original PixEagle main checkout and unrelated edits |
| 5b PixEagle publication candidate | Review backend/Dashboard/default/schema changes; renew required gates; compatibility table, restart/HTTPS guide, Config Sync migrations and scenario cards; compare against current remote main in an isolated checkout | Reviewed PixEagle PR targeting main, exact tested revision and passing CI. Merge/push status reported separately; QGC platform builds do not block independent backend changes that have passed their own gates |
| 5c QGC fork candidates | Keep the optional overlay and stock behavior regression; resolve QML/type and package debt; prepare private fork branch/PR; separate reusable generic QGC fixes from integration-specific changes | Debug/Release and Unit/Integration gates on exact combined revision; dependency/transport provenance; no public customized-QGC binary release |
| 5d platform qualification | Linux install/upgrade/uninstall alongside stock QGC; Windows x64 build and installer on a native runner; Android custom build and device tests | Artifact checksums, bundled dependencies, secure credential store, default-off/stock UI, touch/PiP/fullscreen, suspension and multi-vehicle tests. Windows/Android infrastructure or operator access is needed if unavailable |
| 6a Pi inventory and provisioning | Capture Pi/OS/architecture/accelerator, firmware and router topology; back up existing services/configs; provision reviewed source, dependencies and supervisor with rollback | SSH access and explicit board deployment approval; installation card with versions/checksums, secrets handed off privately |
| 6b onboard ground acceptance | Apply the explicit platform profile, verify routes/trust/identity, camera controls/tracking, Dashboard Follower Test, restart/recovery and sustained load | Physical Pi/router/PX4/camera ground report with circuit breaker on and no PixEagle aircraft-control dispatch. Synthetic preview state remains distinct from observed aircraft telemetry |

Platform build preparation and backend PR review can run alongside physical
qualification. Real aircraft command testing and real flight remain separate
later approvals, with flight-envelope and pilot recovery evidence.

## Public PixEagle defaults and supported combinations

Keep one canonical PixEagle configuration, editable through Dashboard/Config
Sync. QGC stores its connection/UI preferences, not duplicate installation
geometry, camera-provider, follower tuning or circuit-breaker state.

- New users: bundled video, real-time pacing, CSRT, PixEagle engine, no gimbal,
  GStreamer off, `mc_velocity_position` yaw-only behavior, altitude safety on,
  circuit breaker on. Live USB/CSI/RTSP is an explicit source change.
- RTSP plus local tracking: PixEagle Classic/Smart and image-compatible
  followers. Optional gimbal control may remain enabled independently; camera
  angles do not substitute for image tracker outputs.
- Camera-owned tracking: qualified provider, Camera Classic/Smart and
  angle-compatible gimbal followers; mounting geometry is backend configuration.
- External camera-control application: disable PixEagle manual ownership while
  retaining only supported video/tracking capabilities; never introduce another
  competing provider/socket owner.

Public docs describe optional native QGC integration as future/customized
availability with maintainer contact. Preserve Apache licensing and the existing
project-use notice; license modifications remain separate scope.

## Explicit Raspberry Pi robot profile

This profile is commissioned once on the user's board, not shipped as ordinary
defaults. The approved intent is:

- Pi Ethernet `192.168.0.226/24`, camera `192.168.0.108/24`; confirm router,
  air/ground datalink and GCS addresses against the actual devices.
- Wi-Fi retains Internet/hotspot access. Ethernet carries robot camera/MAVLink
  traffic; verify route metrics, subnet overlap, firewall and recovery. Do not
  assume the local LAN or a private overlay provides HTTPS trust automatically.
- RTSP with a qualified GStreamer input pipeline; enable acceleration only after
  inventorying the board and measuring CPU/memory/temperature/frame age.
- Camera engine, Camera Classic startup with firmware Smart selectable,
  Topotek provider/manual control enabled, **VERTICAL** shared geometry preset.
- `gm_velocity_vector`, coordinated-turn guidance. Keep Chase selectable. Local
  tracking remains possible without rewriting provider or video ownership.
- Altitude command generation disabled for the requested platform profile;
  global safety/freshness/abort protections and circuit breaker remain enabled
  during commissioning. This differs from the altitude-enabled SIH retest.
- The requested 2 m/s starting speed is an explicit profile tuning objective,
  requiring compatible global/follower limits and SIH/ground checks; it is not
  automatically inherited from the conservative factory envelope or treated as
  authorization for flight. Do not raise gains from bench visual impressions.
- Preserve service ports: Dashboard 3040, backend 5077 and telemetry bridge 8088
  internally. Remote QGC uses the verified authenticated deployment URL; its
  native connection does not point at the Dashboard port.
- Use the shared supervisor. Advertise remote restart only under the supported
  authenticated HTTPS administrator policy. Do not assume admin/admin on a
  field board; use its actual private credential handoff.

Apply through existing configuration groups, snapshot the effective config and
restart only when required. Rollback restores a known source/configuration and
does not resume targets, manual movement or following automatically.

## Open issues and reporting

Retain the sustained multicopter attitude-envelope issue, fixed-wing airspeed
and opt-in ground-speed qualification, SDK-only telemetry compatibility, custom
QML type lint and platform credential-store behavior as explicit release items.
Fix or clearly delimit unsupported capabilities; do not mask them by disabling
checks or changing unrelated gains. Retain stock-QGC coverage.

At each gate report exact revisions, tests, artifacts, defaults/profile changes,
remaining blockers and the next operator action. Separate automated evidence,
AI review, physical acceptance, repository publication and release installation
claims. The next physical action is the coordinated failure test; Pi commissioning
requires the board and access details later.
