# Slice 3b user-feedback revision: independent visual follow-up

Date: 2026-09-24
Reviewer: Codex simulated UX expert/operator perspective. This is not a real operator study or a flight qualification.

Method: inspected eleven supplied screenshots from a private test display. No implementation source, test results, or action logs were inspected for this review. The user's request for immediate hands-on handoff and willingness to test detection were taken into account. No new automated tracker work was performed.

## Material reviewed

Under slice-3b-feedback-2026-09-24:

- ui-initial/01-default-off.png
- ui-initial/04-settings-disabled.png
- ui-initial/07-sign-in-opens-settings.png
- ui/01-classic-default-tap.png
- ui/02-direct-target.png
- ui/05-manual-mode.png
- ui/06-manual-unarmed.png
- ui/08-manual-no-auto-retarget.png
- ui/11-smart-ready.png
- ui/12-inline-model-change.png
- ui/13-model-confirmed.png

These are 1440 by 900 screenshots. There is no fresh successful 1024 capture in this set. I make no new compact-layout validation claim.

## First interpretation of the revised interface

This is substantially easier to understand than the previous interface. Classic and Smart are now the first choices my eye finds in the panel. The active mode is visible, and the row beneath it has only the relevant choice: Tracker with CSRT in Classic, Model with visdrone9m or visdrone26m in Smart. I no longer have to reason about a dormant Classic tracker while using Smart, or leave the video to choose a model.

The prominent Unavailable trackers action is gone. The remaining Options control has lower visual weight than the mode choice. That is appropriate for secondary preferences. The screen no longer implies that the operator should investigate unavailable algorithms before using the active tracker.

In the direct-selection Classic screenshots, the instruction is plain: Tap a target or drag a box around it. Once tracking, Cancel tracking appears and the instruction changes to Tap another target or drag a box to replace it. This communicates the user's requested quick retarget mental model much more directly than Finish selecting did.

In manual mode, Select target appears when idle; Retarget and Cancel tracking appear when tracking. These have distinct names and sufficient separation. The options explanation says that a selection action is required before each selection. The checkbox is labeled Tap video to select targets, consistent between the visible Settings and Options screens.

The Smart idle screen says Tap a detected target in the video. That correctly narrows the gesture to a detection instead of implying any arbitrary point will always work. The chosen model is visible directly below the active Smart mode.

## Default-disabled and sign-in presentation

The default-off Fly View image has no PixEagle tracking panel or sign-in prompt. The Settings page shows an unchecked Enable PixEagle checkbox with a short explanation. This visually matches the requested optional integration.

The supplied sign-in destination image shows the PixEagle Settings form, backend address guidance, empty credentials fields, and the direct-selection preference. The destination is sensible and avoids duplicate login forms. A screenshot alone cannot prove which click navigated there; that behavior must be backed by the root agent's interaction evidence or the user's test.

The address label now says PixEagle backend address and the help explicitly says to use the backend URL and port without /api/v1. This directly addresses the user's dashboard-port confusion. The real current dashboard URL still needs to be given accurately in the handoff text.

## Remaining observations, not blockers to this handoff

1. The model-changing screenshot has Updating target at the top while the model dropdown says Updating and the panel says it is waiting for PixEagle to confirm the action. The operation is visibly busy, but Loading model would be more precise than Updating target. This is a small wording refinement, not a reason to delay the user who has offered to test now.
2. The top No target selected pill remains visible when idle. It is small enough here that it does not prevent the task. If the user still finds the screen noisy, hiding or reducing this idle state is a reasonable next discussion; do not remove useful tracking/lost feedback merely to reduce text.
3. The mode highlight is replaced by disabled neutral controls while loading. The changed model is only confirmed in the next screenshot. This makes the request state clear enough for a handoff, but latency and focus stability cannot be judged from still images.
4. test4 has its own recorded TRACKING label, crosshairs, and other OSD. Some frames therefore show recorded TRACKING while QGC says No target selected. That can be confusing even though it is not evidence of a QGC state bug. The user should be told which status comes from QGC and that part of the test recording already contains tracking graphics. Do not alter this source recording just to make the screenshot cleaner.
5. The native QGC top banner still says Disconnected because no aircraft is connected. The settings explanation clearly separates permitted target/video operation from aircraft controls. Keep that distinction in the handoff; it is not a reason to remove QGC's standard aircraft status.

## Handoff recommendation

No visual blocking issue was found in this set. Proceed to the user's requested hands-on test with test4, a verified current backend/dashboard address, and saved logs. Let the user test the actual Smart detections, direct retarget, manual option, model changes, and Cancel rather than adding another automated detection qualification loop before the handoff.

The next user observations should carry more weight than this simulated review. Ask for what they tried, which model/mode was active, and approximate time if an issue occurred, so the saved logs can be correlated afterward.

## Limits

The screenshots support visual hierarchy and state presentation observations only. They do not independently establish that unarmed clicks never mutate targets, rapid taps are ordered correctly, source/client guards work, sign-in navigation is fixed, a Cancel request succeeds, model accuracy is adequate, or touch and keyboard input are accessible. No fresh compact-resolution result, hardware gimbal test, following behavior, or flight qualification is claimed.
