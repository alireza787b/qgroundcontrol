# Slice 4a camera mode architecture review

AI architecture review, 2026-09-27. Source inspection only; this is not an operator
interview or physical camera acceptance. No hardware commands were sent.

## Mode hierarchy

Preserve two concepts: processing source (PixEagle or camera) and selection mode
(Classic or Smart). Keep Classic/Smart in the compact control. For local processing,
show the selected tracker or AI model. For external processing, show Camera and its
Classic/Smart selection mode; never show a local AI model as camera Smart's identity.
Put source/provider configuration in Options/settings rather than adding a third
always-visible Gimbal button beside Classic and Smart.

The existing contract already expresses `mode=classic|smart|external` and
`external_selection_mode`. `PixEagleTargetController::smartMode()` reads both paths;
`setSmartMode()` routes external selection to `gimbal_control/set_mode`. Camera Smart
is firmware fuzzy click selection, not PixEagle's local SmartTracker. Hide local
model selection for external modes. Provider capabilities determine camera controls.

## State correctness

Use fresh `target_status == "tracking"` for a positive tracking indicator. Backend
external `tracking_active` includes `target_lost`; it represents an ongoing session
and can justify showing Cancel, but does not prove a current lock. Pending commands,
stale telemetry and command acceptance must not look like confirmed tracking.

Follower compatibility remains backend-owned: local modes produce `POSITION_2D`,
external mode produces `GIMBAL_ANGLES`. Keep Start gated by current backend readiness
and Stop tied to the existing aircraft/session. Mode changes and retargeting require
following stopped. The native target route currently permits camera select, cancel
and set_mode only; camera movement needs the later slice 4b control contract.

## Why no backend rewrite now

The provider, normalized samples, optional controls, target guards and follower
boundaries already separate vendor behavior. This UI change can consume them.
Only Topotek SIP UDP is currently implemented. Its `set_mode` capability assumes
Classic and Smart; a future camera with different modes should extend the typed
mode catalog when that device creates a concrete requirement.

## Remaining slice 4b questions and evidence

- Confirm physical model/firmware, current mount and private Ethernet/RTSP setup.
- Verify matching full-frame video, fresh state, rotation/flip and target coordinates.
  Existing selection requires the same literal host for RTSP and the UDP provider.
- Qualify bounded movement, release/Stop, disconnect recovery and actual axis signs.
  A proposed 90-degree mounting remains unqualified; do not infer axis swaps.
- Establish Classic rectangle retention/retarget/cancel and camera Smart acquisition
  from observed camera behavior. Camera intent or command transmission is insufficient.
- Qualify the compatible follower separately after camera bench acceptance.

## Inspected references

- QGC: `custom-pixeagle/src/PixEagleTargetController.cc`, `PixEagleVideoView.qml`.
- Backend: `src/classes/api_v1_native_targets.py`, `api_v1_native_following.py`,
  `gimbal_control.py` in the PixEagle integration worktree.
- Backend docs: `docs/trackers/05-development/external-gimbal-providers.md` and
  `docs/trackers/02-reference/gimbal-tracker.md`.
- [Prior hardware provenance](next-gimbal-notes.md).
