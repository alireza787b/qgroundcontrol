# Slice 3b feedback: independent first interpretation

Date: 2026-09-24
Reviewer: Codex simulated UX expert/operator perspective. This is not a real operator interview, field study, accessibility audit, or evidence of flight qualification.

Inputs: the user's latest feedback and four previously captured private-display screenshots. No implementation source was inspected before this review. The user explicitly requested simpler controls, mode-first choices, direct selection by default, optional manual selection, disabled-by-default integration, a functioning sign-in entry, and replay test4 or test9.

Screenshots inspected:

- slice-3b-2026-09-22/ui/compact-final/01-clean-insets.png
- slice-3b-2026-09-22/ui/handoff/10-smart-detection.png
- slice-3b-2026-09-22/ui/handoff/05-model-dialog.png
- slice-3b-2026-09-22/ui/handoff/18-quick-retarget.png

## Immediate interpretation

The existing screen makes configuration compete with the target task. The important mode is a small dropdown after a much larger Classic tracker dropdown. When Smart is selected, the Classic control remains visible and the actual model is hidden behind another dialog. I would infer both controls affect the current Smart operation. This is an avoidable ambiguity.

The Unavailable trackers button is as prominent as the model control. During operation, unavailable choices are not useful destinations. Its prominence suggests an unresolved problem that needs attention even when the current tracker is working. I would move the diagnostic inventory out of Fly View entirely.

Select target, Finish selecting, Retarget, and Cancel tracking represent different concepts but appear in the same place with similar weight. In the screenshot showing Finish selecting, it is not obvious whether pressing it finishes the gesture, stops target tracking, or confirms an unfinished target. The user's direct selection request removes that extra state from the normal workflow.

The compact screen already keeps the panel above QGC flight instruments, which is appropriate. New controls should not expand into another wide row or move over those instruments.

## Recommended visible hierarchy

1. First row: two mutually exclusive native QGC-style controls labeled Classic and Smart. They should be equally sized and visibly indicate the confirmed active mode. Their prominence comes from order and grouping, not oversized graphics.
2. Second row: one inline dropdown, labeled Tracker in Classic and Model in Smart. Do not show the Classic tracker while Smart is selected. Preserve the chosen Classic tracker internally so returning to Classic restores it.
3. Target operation: keep Cancel tracking visible when a target exists or is acquiring/lost. With no target, a short unobtrusive instruction is enough. Avoid a disabled Cancel button taking space indefinitely.
4. Secondary configuration: place the direct-selection preference and diagnostics in settings or a modest expansion, not a permanent competing action row.

An inline model choice should apply the deliberate choice through the existing guarded backend operation. During loading, retain the confirmed value and show a short Loading model status. Do not claim the new choice is active just because it was clicked. If model switching requires stopping the current target, disable the dropdown with a nearby short reason: Cancel tracking to change models. Do not silently cancel a target.

A model loading failure should leave the previous confirmed model visible and give a concise retryable explanation. A briefly stale inventory must not cause the dropdown to flicker between the first item and the current item. No hardcoded model or tracker names should appear as the source of truth.

## Direct target selection

Suggested setting name: Tap video to select targets.
Suggested explanation: Tap another target to replace the current one.

This is a proposed label, not a mandate. If desktop terminology matters, Select targets directly is an acceptable label with the same explanation. Do not use Continuous tracking; that sounds like a tracker algorithm or vehicle-following behavior.

With the preference on by default:

- A tap or click in the live video selects a target in the current mode without an arming action.
- Another accepted tap replaces the current target immediately.
- Classic can retain drag-to-draw as an additional precise selection gesture.
- Smart taps must select a current detected candidate; a miss should preserve the current target and give a brief local explanation.
- Omit Select target, Retarget, and Finish selecting from the normal action row.
- Keep Cancel tracking unambiguous. It ends the current target; it does not secretly turn the direct-selection preference off. A new deliberate tap may start a new target.
- If the source changes or a request is stale, do not replay the rejected tap later. Once the newly displayed source is fresh and valid, the next new tap should work without an invisible arming requirement.

With the preference off:

- Show Select target when idle and Retarget when a target exists.
- One activation arms one selection attempt/accepted selection according to the existing safe behavior; make the active state visually clear.
- Escape or the same visibly checked selection control cancels the gesture without cancelling the existing target.
- After a completed accepted selection, return to unarmed mode. The next retarget requires the button again.

For either preference, taps on dropdowns, QGC controls, PiP, fullscreen controls, or modal dialogs must not also become target taps. Do not intercept the map. Do not permit a press begun on an old video/client to target a new source when released. Guard fresh displayed frames and active endpoint identity. Rapid repeated taps should not queue invisible delayed actions or overwrite a newer accepted action with an older response.

Double-click-to-fullscreen deserves explicit testing because the first click may now select a target. A native explicit fullscreen control is preferable to an undocumented collision between fullscreen and selecting. This review does not prescribe removing QGC's standard gestures; it flags the interaction to verify and explain.

Direct selection must remain distinct from following. Selecting a target should not imply aircraft movement. Do not add persistent following controls in this slice merely to explain that distinction.

## Default disabled and sign-in

On a fresh profile, PixEagle should be off and add no Fly View overlay, prompt, or sign-in invitation. The entry in Application Settings is a sufficient discovery point. Respect the preference after the user explicitly enables it.

When PixEagle is enabled but unauthenticated, any Sign in action must open the same PixEagle settings page and land the user at the credentials flow. A button that appears to do nothing is a high-priority functional defect, not a cosmetic issue. Avoid duplicating the credentials form in the video overlay.

## Compact layout and state feedback

At 1024 by 768, the controls should wrap predictably above the existing instrument insets. Keep touch targets native-sized and labels legible; do not solve crowding by shrinking all text. A two-row mode-and-choice arrangement is preferable to a single cramped row with unrelated buttons.

A small target status is useful when acquiring, lost, or blocked. A large camera icon with No target selected is not needed indefinitely if direct selection is available. The user should see video first. Brief instructional text can appear near the controls and yield to actual status.

Respect keyboard focus and accessible names for mode controls and the current dropdown. The mode selection must not depend only on color. Keep focus stable when inventory/state updates arrive.

## Acceptance tasks for the next screenshots and handoff

- Start a fresh profile: no PixEagle overlay before enabling it in Settings.
- Enable PixEagle, activate Sign in from the integration entry, and verify the credential page opens.
- Classic is clearly selected and shows only its tracker choices.
- Smart is clearly selected and shows only its installed model choices; model loading/result are legible inline.
- With direct selection on, tap a target, then another target, then Cancel. No selection button is required.
- With direct selection off, use Select target once and Retarget once; clicks without arming do not mutate targets.
- Change mode, endpoint, replay loop/source, or stale frame while an interaction is in flight; no old gesture lands on the new context.
- Dropdowns, video/map swap, fullscreen, and QGC instruments still work without accidental target selection.
- At 1024 by 768, neither the target panel nor mode/model choices cover flight instruments.
- Repeat with the user-requested test4 or test9 recording. Model detection quality should be reported separately from UI correctness.

## Limits and independent reservations

The user has explicitly chosen direct selection by default. I agree it reduces friction for this workflow, but it also makes every deliberate video tap a potential target change. Clear current target feedback, visible Cancel, strict fresh-context guards, and a discoverable preference are necessary companions. I would not hide those operational states merely to reduce the number of controls.

No visual review can establish model accuracy, actual touch event arbitration, input latency, multi-vehicle correctness, or flight safety. Those require executable checks and user testing. These recommendations describe a simpler operator mental model; they are not a claim that all requested behavior is already implemented.
