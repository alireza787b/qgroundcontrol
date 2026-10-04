# Slice 4 plan — ordinary following, then external camera

Revised 2026-09-26 after the operator clarified that the normal workflow is
Classic or local Smart tracking on a chosen PixEagle video input, followed by a
compatible aircraft follower. The earlier plan put camera movement first. The
backend contract audit found native following deliberately disabled, so normal
following is now the first implementation and test checkpoint. This changes the
order within slice 4, not the original scope or its acceptance gates.

## Starting point and ownership

Slices 0–2 and the local Classic/Smart target workflow are implemented and
accepted on Linux. PixEagle owns input acquisition, target state, follower
computation, Offboard publication and camera commands. QGC sends authenticated
intent and displays authoritative state. It never creates its own PixEagle
follower or camera UDP driver. Its existing flight controls remain accessible.

`GET /api/v1/integration/context` advertises following operations but leaves
its summary readiness false until the separate following resource is read.
The legacy dashboard profile switch persists a choice for the next follow
session; a saved choice is not automatically the running choice. Slice 4a adds
dedicated native Start, Stop and profile-selection routes while retaining the
dashboard routes.

The backend's profile schema and registered implementations, tracker output
type, configured airframe and execution mode determine compatibility. Ordinary
image trackers use position output; the external camera uses gimbal angles and
only the compatible gimbal follower profiles. No fixed, invented profile list
belongs in QGC. Recorded video is for local tracking and command preview; it
cannot authorize live PX4 following. The selected PixEagle input is the source
of the authenticated QGC video. Changing input through the dashboard must
invalidate target and frame context in QGC. A native input selector is useful
only after a typed backend catalog and safe apply/restart contract exist; it
must not copy source URLs or change QGC's own video source independently.

## 4a — normal tracker-to-following checkpoint

| Step | Implementation and evidence | Completion condition |
| --- | --- | --- |
| 1. Following contract | Add typed authenticated follower choices, configured/effective profile, readiness reasons and active status. Use backend schema and runtime validation. Include aircraft/target/source generations and the effect of saving a profile. | QGC never guesses compatibility or treats persisted configuration as already active. Viewer and companion-only clients cannot start. |
| 2. Guarded operations | Add dedicated typed native Start, Stop and profile-selection routes. Start requires verified aircraft, fresh usable target, compatible profile, existing PX4 preflight, confirmation, idempotency and audit. Backend rechecks context inside the flight-owner lifecycle barrier. Stop can cancel a captured pending Start. | Stale, mismatched, replay, inhibited or already-active starts fail closed. Stop retains its original companion and does not require fresh imagery. |
| 3. QGC ordinary workflow | Show the compatible follower choice in operational Options; show concise readiness and an explicit Start following action only when permitted. Keep Stop following separately available with per-aircraft status if video or another vehicle is selected. Reflect dashboard changes from authoritative reads. | Tracker, video source, following and standard QGC controls have clear ownership. A late response cannot update another vehicle. |
| 4. Mocks and UI review | Test authorization, profile compatibility, stale generations, shared dashboard actions, retry/idempotency, reconnect, target loss, background vehicle and Stop/Abort routing. Review private screenshots at normal/compact size and retain raw feedback. | Meaningful backend/QGC tests pass; no extra permanent Fly View clutter. |
| 5. Simulation | Use an exact, isolated PX4 SIH/SITL build or pinned container with separate QGC/MAVSDK ports. First prove normal profile command preview and no PX4 commands for replay. Then use synthetic fresh target input with authorized SIH to check Offboard entry, publisher continuity, Stop/Abort, loss and vehicle route. | Save exact image/commit/config/ports/logs and observed PX4 state; success is limited to tested simulation. |
| 6. User checkpoint | Prepare a repeatable QGC session and ask the user to test ordinary Classic/Smart choices, target selection, profile/readiness and Stop. | User feedback and correlated logs are reviewed before the camera checkpoint. |

The user authorized Docker PX4 SIH/SITL for simulated follower tests. Real
aircraft takeoff, field flight and physical camera movement are separate gates.
Do not disable the command circuit breaker outside the isolated SIH setup or
claim a replay demonstrates actual follower movement. PX4's documented SIH
example is `make px4_sitl_sih sihsim_quadx`; QGC receives UDP on 14550 and
MAVSDK uses 14540 for a single instance. Record the exact image or source
revision used rather than relying on a moving `latest` tag.

## 4b — external camera and gimbal checkpoint

The existing provider is **Topotek SIP UDP** over Ethernet with matching RTSP
video. The [recovered hardware notes](reviews/next-gimbal-notes.md) distinguish
completed dashboard code from limited physical acceptance. No second provider
or vendor packet implementation is planned. Camera Smart is firmware fuzzy
selection, distinct from local AI Smart and its model choice.

1. Read advertised provider modes, axes, presets, bounds, permissions and Stop
   behavior. Present camera controls only when the external tracker and camera
   control are enabled; preserve the ordinary panel as the common case.
2. Verify matched full-frame RTSP input into PixEagle and the authenticated,
   frame-associated video displayed by QGC. Exercise camera Classic/Smart,
   point/rectangle where supported, replacement and Cancel. Direct, unrelated
   QGC RTSP display does not qualify native targeting.
3. Add capability-driven pan/tilt/roll/zoom/Home/Stop through the existing
   backend command owner. Tap sends one bounded step; hold repeats serially.
   Release, focus loss, view/vehicle/source change or disconnect ends repetition
   and attempts Stop at the original destination. Stop does not need a frame.
4. Filter follower choices to profiles compatible with external gimbal angles.
   Preserve separate Stop camera, Cancel target, Stop following and Abort.
   Camera movement and mode changes follow the backend's active-following guard.
5. Pass source, geometry, lifecycle and interruption fixtures and review private
   screenshots and touch/keyboard behavior. Then prepare the user bench script:
   passive video/status first, target checks second, bounded movement last.
6. Qualify the actual mounting, axes, firmware and identity at the bench. The
   proposed base-pitched-up 90-degree installation and camera Smart identity
   remain open; do not guess an axis swap or follower direction.

## Slice 5 and explicit later work

Re-run final stock/custom Linux gates, build and qualify Windows and Android,
document onboarding/troubleshooting, and organize coherent backend/custom and
upstream PRs. Preserve default-off and stock flight/mission behavior. Recheck
transport PR status at publication without silently changing the pinned local
baseline. Deployment, HIL and live flight need their own evidence.

The [operator inventory](OPERATOR-COVERAGE.md) tracks recording, OSD/media
administration, detailed configuration, model installation/trust, accounts and
diagnostics. Native segmentation, redetect and tracker restart remain deferred
until their compatibility and interaction guards are specified. The browser
dashboard remains available for those existing functions.
