# Next gimbal checkpoint — existing provider and hardware provenance

Read-only local research, 2026-09-26. No services, camera packets, connectivity
probes, configuration changes or hardware tests performed. Network credentials
were not printed. Repository checkpoints supplied sufficient prior-session context;
no broad Codex-session search was needed.

## Provider identified

The user is correct that this backend currently defines **one supported provider**:
`topotek_sip_udp`, displayed as **Topotek SIP UDP**. It uses Ethernet/IP with
SIP-over-UDP control/telemetry and matching RTSP video, not MAVLink Gimbal Protocol
v2. `src/classes/gimbal_provider.py::list_supported_gimbal_providers()` returns
only that provider. The original `/home/alireza/PixEagle/configs/config.yaml`
also selects it; integration defaults select it and keep CONTROL_ENABLED false.
The inspected configuration is evidence of intended setup, not live connectivity.

Paths in the remainder are relative to `/home/alireza/PixEagle-qgc-integration`.
Do not change the original main checkout to prepare QGC hardware testing.

## Prior implementation and actual qualification

- `docs/reporting/agent-ops/codex-modernization/checkpoints/2026-09-20-gimbal-feature-handoff.md`
  records completed dashboard software: Classic point/rectangle, camera Smart/fuzzy
  selection, bounded pan/tilt/roll/zoom/Home/Stop, movement presets and serialized
  mouse/touch/keyboard holds. Final combined validation was 659 backend tests,
  62 dashboard suites/471 tests and schema/build checks. Browser action tests were
  intercepted and do not count as physical camera movement.
- `.../2026-09-20-gimbal-horizontal-acceptance.md` records **partial** standalone
  camera acceptance. Pan left/right holds and release/Stop had observable motion;
  individual axis probes succeeded at the command path. Base/pose changes prevent
  complete axis calibration claims. The rectangle test failed in the driver before
  sending a selection; no rectangle success was claimed. Following stayed off.
- `.../2026-09-19-gimbal-drag-selection.md` and its evidence README retain short
  Classic retention and cancellation, with loss/reacquisition. Camera Smart candidate
  identity, stable rectangle/retarget retention and vertical installation remain open.
- `.../2026-09-20-gimbal-movement-controls.md` specifies the camera operator workflow,
  release/cancel semantics and planned hardware sequence. Fine/Normal/Fast are
  provider-owned presets, with advertised custom bounds; QGC should consume those
  contracts rather than copy vendor constants or packets.
- `.../2026-09-20-gimbal-feature-handoff.md` explicitly says the proposed base-pitched-up
  90-degree installation is **unqualified**. Its passive video/telemetry did not
  establish physical axis mapping, native tracking or follower direction. Do not
  infer a yaw/roll swap. No aircraft, SITL or HIL qualification was obtained.

The prior docs identify firmware `1.6.26.R.D` in a manufacturer-workflow follow-up,
but a current physical-device identity/firmware confirmation is still appropriate
at hardware setup; do not treat that old identifier as live discovery.

## References for the next prepared session

- `docs/trackers/02-reference/gimbal-tracker.md`, especially Optional Dashboard Camera
  Controls and Select, Replace, And Cancel A Target.
- `docs/trackers/05-development/external-gimbal-providers.md` for capability/provider
  separation and qualification obligations.
- `docs/reporting/agent-ops/codex-modernization/checkpoints/2026-09-19-gimbal-mounting-audit.md`.

Backend target selection expects matching RTSP_OPENCV/RTSP_STREAM input with the
same literal host as the provider's UDP_HOST, fresh full-frame video, and known
rotation/flip. A different source, replay or camera-internal crop/PiP cannot silently
qualify targeting. QGC should use the paired PixEagle output and retained-frame
contract, while PixEagle ingests the matching RTSP camera.

Next implementation can proceed with offline capability/motion guards and fixtures.
When the prepared native test is ready, ask the user to connect this Topotek camera
by Ethernet and confirm current mount/scene and its existing private connection
configuration. First verify read-only video/state, then explicit bounded movement
and target checks. Following/aircraft tests remain a separate gate. No new broad
provider questionnaire or second camera integration is presently necessary.
