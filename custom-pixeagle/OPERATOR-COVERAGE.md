# PixEagle operator coverage and remaining slices

Updated 2026-09-26 after user acceptance of the local Classic/Smart workflow
and the dashboard/help shortcut request. This inventory describes the checked
integration backend and dashboard, not a promise that every web feature already
exists in QGC. Native controls reuse backend behavior and QGC's interface rules.

## Current boundary

Slices 0–2 established the workspace, optional sign-in and vehicle binding,
authenticated video and status. Slice 3 implements guarded Classic, Smart and
external-camera target actions. Classic has user feedback; slice 3b now has
actual Full AI replay evidence for both installed Smart models and six Classic
trackers, including native GUI selection, retarget and Cancel. The user accepted the tested local workflow on September 26. Saved logs confirm
Classic and Smart actions, but do not establish human coverage of every tracker
or model. Physical external-camera qualification remains open.

The [latest slice 3b feedback pass](SLICE-3B-FEEDBACK.md) adds default-on direct
target taps, optional manual one-shot selection, conditional inline model/tracker
choices and a concurrent dashboard demo. Integration itself remains default off.

The feedback checkpoint is **slice 3a**. **Slice 3b** below explicitly adds the
installed-model workflow and remaining target-mode validation before slice 4.
This extends the original target slice; it does not silently call model choice
complete or move gimbal motion into an unqualified tracker demo.

The [current checkpoint](STATUS.md) adds dashboard/help shortcuts and separates
the remaining camera/gimbal and following gates. No new numbered slice is needed.

## Operational inventory

Paths in this table are existing backend routes. Unversioned routes are evidence
of dashboard behavior, not approved new native contracts. New public native
contracts belong under `/api/v1/`, with typed models and route inventory tests.

| Operator task | Existing implementation and contract | Native coverage / planned owner |
| --- | --- | --- |
| Enable, sign in/out, companion address, aircraft association | `/api/v1/auth/*`, `/api/v1/integration/context` | Slices 1–2; per-vehicle sessions, separate companion-only session, default off |
| View video, swap map/video, PiP, fullscreen | Dashboard `VideoStream.js`; JPEG, WebSocket and WebRTC paths | Slice 2 uses authenticated WebSocket JPEG; selecting on another transport needs equivalent displayed-frame evidence, not just playback |
| Select Classic point/box, retarget, Cancel | `/api/v1/actions/tracking-start`, `tracking-stop`; `integration/target-state` | Slice 3a; current replay demonstrates real Core tracking without PX4 |
| Choose Classic tracker | `/api/v1/tracking/catalog`; `actions/tracker-switch` | Slice 3a reads canonical availability; no fixed three-tracker list; missing requirements disclosed |
| Enter/leave local Smart mode; select detected target | `actions/smart-mode-toggle`, `smart-click` | Slice 3b recorded-video checks passed with both VisDrone models; local workflow accepted September 26 |
| Choose installed Smart model; inspect classes/device/fallback | Native `/api/v1/integration/models`, `/{model_id}/labels`, `/api/v1/actions/model-select`; reuses dashboard model manager | Implemented and tested in slice 3b; local workflow accepted; per-model human coverage not inferred |
| External/gimbal tracker; camera Classic/Smart; target and Cancel | `GimbalControlPanel.js`; `/api/v1/gimbal/control`, `actions/gimbal-control` | Slice 3 supports advertised select/cancel/set_mode; supported provider plus matching full-frame source required; real camera acceptance remains open |
| Camera pan/tilt/roll/zoom/home/Stop | Same gimbal contracts; `gimbal_control.py`, `gimbal_motion.py` | Slice 4; show only advertised enabled operations, backend limits and movement presets |
| Follow readiness, start, stop and pending-start cancellation | `/api/v1/integration/following`, `actions/native-follow-start`, `native-follow-stop` | Slice 4a implementation; verified aircraft, target/source generation, profile and Offboard preflight; full SIH and operator acceptance pending |
| Choose follower/profile; inspect effective limits | `FollowerQuickControl.js`; native `actions/native-follower-select`, `/api/config/effective-limits` | Slice 4a native choice uses implemented compatible profiles and blocks active changes; detailed limits remain dashboard-owned |
| Redetect, tracker restart, segmentation | `actions/tracking-redetect`, `tracker-restart`, `segmentation-toggle` | Reviewed and deferred; see SLICE-3B.md for guard, recovery and compatible-model requirements |
| Record start/pause/resume/stop and storage status | `RecordingQuickControl.js`; `/api/recording/*`, `/api/storage/status` | Later media slice after operational gates; PixEagle recording must not masquerade as QGC local recording |
| Recording browse/download/delete; OSD inclusion | `/api/recordings`, `/{filename}`; recording OSD option | Later media/administration slice; explicit storage location, permissions and deletion behavior |
| OSD visibility/presets/colors; output/reconnect | `/api/osd/*`, `/api/gstreamer/*`, `/api/video/reconnect` | Later media/settings slice; distinguish backend annotations from native status and scene content |
| Detailed tracker/follower/config editing | `/api/config/*`, runtime-status, schema, validate/diff/apply/revert/history/import/export | Later administration slice; schema-driven choices, persisted versus effective runtime state, explicit restart requirements |
| Model upload/delete/trust and artifacts | `/api/models/upload`, `/{model_id}`; model artifact/provenance policy | Later administration; keep separate from selecting an already installed compatible model |
| Backend restart | `actions/system-restart`, canonical configuration snapshot | Implemented for verified Linux/SIH supervisors; see SLICE-4B-RESTART.md. Confirmation, permissions, inactive-operation guards and fresh runtime verification required; Windows remains unqualified |
| Account administration/passwords, diagnostics | Auth user/password routes; `/api/v1/logs/*`, `system/about` | Later administration; honor scopes, redact exported diagnostics, preserve user configuration |
| Circuit breaker and flight-system setup | Circuit-breaker status/action, safety/config routes, managed-SIH actions | Diagnostic/readiness input first; explicit later scope for mutation; no implicit SITL, provisioning or hardware operation |

## Slice 3b — Smart model choice and target-mode completion

1. Add typed authenticated model inventory, active model and labels contracts,
   and a guarded model-selection action reusing the existing model manager.
   Use stable advertised model IDs, compatible tasks and provenance status; do
   not copy paths from QGC's machine or execute an untrusted checkpoint.
2. Preserve `models:read` and `models:select` separately from `models:manage`.
   Capture client/runtime/vehicle and model/target generations at interaction
   start. Serialize with existing follower and tracker/model locks, retain
   idempotence and actor audit, reject stale or uncertain actions without retry.
3. Distinguish configured model from running model, effective device and CPU
   fallback. Default device choice to the backend's automatic policy. Keep
   Classic usable during model setup or failure; show classes in disclosure.
   Preserve backend restrictions on active following and selected Smart targets.
4. Keep native setup compact: tracker/mode and installed model choice; detailed
   model installation remains outside Fly View. Model changes by the dashboard
   must invalidate unfinished native selections and refresh authoritative state.
5. Prepare a separate recorded Full AI snapshot after compatibility checks.
   Existing local VisDrone models declare person/vehicle classes, not soccer
   balls; the current soccer replay cannot qualify ball detection. Record asset,
   model, runtime and device provenance and measure selection/loss/recovery.
6. Validate external-camera capability discovery, camera mode, selection/Cancel,
   matched video, stale source and disconnect behavior with fixtures first.
   Real camera acquisition and mounting/orientation require separate hardware
   evidence. Do not fake availability when no camera is configured.
7. Review redetect, restart and segmentation against operator need and retained
   frame semantics. Record an explicit support/defer decision for each; do not
   map an unguarded legacy action to a native button.

Gate: model setup and target selection work with shared dashboard/QGC state;
unavailable, loading, failed, stale and permission-denied states are clear;
Classic remains usable; qualified runtime evidence is distinguished from mocks.

## Slice 4 — following and gimbal operation

- Readiness and explicit following start/stop/abort use PixEagle's existing
  command owner. QGC never supplies its Offboard heartbeat or follower logic.
- Following stays with its original aircraft when views or active vehicle change.
  Keep background per-vehicle status and destination-specific Stop reachable
  without fresh video. Preserve QGC Pause/RTL and normal flight controls.
- Optional camera controls follow enabled capabilities, axis support, speed and
  duration bounds and session presets. Tap sends one bounded step; hold repeats
  serially. Release, focus loss, disconnect, mode/source/vehicle change end the
  hold and attempt any required stop on the original endpoint. Camera Stop must
  not depend on fresh imagery or accidentally become Stop following.
- Reuse camera provider behavior. Do not add vendor packets, assume every axis
  exists, or claim pulse duration implies exact physical position.
- Following and camera actions need their own appropriate destination guards;
  the slice 3 native handler deliberately rejects gimbal movement and active
  following mutations. Extending the UI alone cannot enable these workflows.

Gate: routing, hold cancellation, late replies, shared-client conflicts, denied
readiness and stock flight-control access pass mocks; SITL only after explicit
authorization. Physical hardware qualification remains separately reported.

## Release and later work

Slice 5 retains Linux, Windows and Android acceptance, default-off coverage,
onboarding/troubleshooting, extension documentation and focused upstream review.
The later media/administration items above are tracked follow-ups, not silently
included in the first operational release. Deployment and companion provisioning
remain outside this project slice.

## Inventory provenance

Read against `/home/alireza/PixEagle-qgc-integration`, base
`989d9662173b364de03b307f4208e9d0ca96451f`, with the slice 3 backend source manifest.
Primary code: `dashboard/src/services/apiEndpoints.js`, the components named
above, `src/classes/api_security_policy.py`, `api_legacy_model_routes.py`,
`api_legacy_follower_routes.py`, `api_legacy_recording_routes.py`,
`api_v1_native_targets.py`, `gimbal_control.py`, and
`docs/apis/native-target-operations.md`. The inventory was performed by the
primary agent after the delegated inventory could not run due to model capacity.
No backend configuration, model, dashboard or service was changed for this read.
