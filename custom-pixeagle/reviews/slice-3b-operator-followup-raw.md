# Simulated operator review — follow-up raw feedback

This is a simulated operator usability review by an AI, not real human operator feedback. It records my first interpretation of the supplied follow-up screenshots and should remain unchanged. It does not replace or edit operator-initial-raw.md. I inspected no implementation and did not inspect handoff/initial-driver-attempt. Screenshot names are identifiers, not evidence that any action succeeded. The video contains its own race graphics; appearance alone cannot establish tracker accuracy or the origin of every overlay.

Task: With a recorded PixEagle stream in QGC and no aircraft, choose an installed Smart detector, start tracking, change target, stop tracking, and return to normal QGC views.

Images inspected under /home/alireza/.cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/ui:
- handoff/05-model-dialog.png
- handoff/06-model-keyboard-focus.png
- handoff/08-model-applied.png
- handoff/11-select-instruction.png
- handoff/13-smart-tracking.png
- handoff/18-quick-retarget.png
- handoff/20-other-model-disabled.png
- handoff/21-smart-cancelled.png
- handoff/23-map-view.png
- handoff/24-fullscreen.png
- compact-final/01-clean-insets.png (1024 × 768)

## First interpretation and next actions

In 05, I understand the state much better: “Selected: visdrone26m. Smart mode is not running.” explicitly separates the chosen model from running detection. “To start detection, close this panel and choose Smart in Tracking setup.” gives me a concrete sequence. I would choose a model, apply it, close, and change Classic to Smart. I no longer read closing the dialog itself as enabling detection.

In 06, the dropdown contains visdrone9m while the status still identifies visdrone26m. I infer that I have picked a candidate that is not yet applied. “Use model” is now legible and has a visible white outline, making it look ready for keyboard activation. In 08, the status and dropdown both name visdrone9m and “Use model” is greyed out. That is coherent visible feedback for an already-selected model. I cannot verify keyboard traversal, focus trapping, or the application event from still images.

In 11, I notice the prominent “Finish selecting” button first. The status remains “No target selected,” so my initial reaction is to wonder whether I should press that button after choosing something. Reading the much smaller instruction resolves the immediate gesture: “Click a detected target in the video.” I would look for a detection marker and click it. At this particular instant I do not see an obvious selectable detection marker in the race scene. That could be a normal moment without a candidate, not a defect. The text now gives useful guidance that was missing from the initial screenshots.

In 13, “Tracking target” at the top is a clear state statement. “Retarget” and enabled-looking “Cancel tracking” at the bottom give obvious next actions for changing or stopping. I see yellow lines and a small marked region toward the left, plus the recording’s large race numeral, but I would not use those graphics to judge tracking accuracy. To change target, I would press Retarget.

In 18, I read “Acquiring target” and “Click another detected target to replace the current one.” Together they communicate that replacement is in progress and specify the next action. “Cancel tracking” remains available-looking. I would click another detected object; if I decided against replacement, I would try “Finish selecting,” although that label does not explicitly say whether it preserves the current target.

In 20, I can understand why applying another model is unavailable: “Cancel tracking before changing models.” I would Close, use Cancel tracking, then reopen Smart model. This makes the restriction actionable rather than mysterious. The currently running visdrone9m and pending dropdown choice visdrone26m are distinguishable, although I have to compare two model names.

In 21, “No target selected,” the return of “Select target,” and disabled-looking “Cancel tracking” communicate a cleared target. Smart remains displayed, which I read as the selected operating mode. This screen does not explicitly say whether detection is still running; it does clearly say no target is selected.

In 23, the map visibly occupies the main QGC view and the video is a small lower-left inset. The tracking controls no longer dominate the map. This is a recognizable return to map viewing. The inset has overlapping-window, corner, and chevron icons, plus “PixEagle · Active.” I would try those to resize, switch, or hide the video, but their exact actions are not labeled in the image. In 24, “Exit fullscreen” at the top right is explicit and immediately understandable, so leaving fullscreen is no longer an icon-guessing task.

In the 1024 × 768 compact image, the primary three-button strip is above the lower map/telemetry/compass area and remains readable. I do not see those QGC insets overlapping the tracking buttons. The strip covers a small area of the video, which seems a reasonable tradeoff for keeping controls reachable. This image does not show the expanded setup or a model dialog at that size, so I cannot assess those compact states.

## Remaining findings and actual task impact

No high-severity task blocker is established by these screenshots. The previously high-severity Smart activation ambiguity is substantially addressed by the explicit selected/not-running status and exact instruction in 05/08. The findings below concern remaining presentation friction, not verified failures.

### Medium — Essential selection guidance is visually too small

Evidence: the instruction lines in 11 and 18 are markedly smaller than the nearby button labels, model names, and top status. They sit in a narrow strip between the primary buttons and setup controls. This is the text that explains how to begin and replace a target, so missing it matters directly to the task. A low-vision user or someone looking quickly between the video and controls could miss the required click gesture. Larger text or a more prominent instruction area would improve the most important remaining interaction guidance. No contrast measurement or physical display size is available.

### Medium — Connection and source status still leave the recording-only context unclear

Evidence: 05 through 23 and the compact screenshot retain the dominant “Disconnected - Click to manually connect” banner while model/tracking states are available. The main video views do not label the source as recorded. The map inset in 23 says “PixEagle · Active,” which adds a useful source label but does not explain what is disconnected. A first-time operator could spend time attempting an unnecessary aircraft connection or wonder whether the active companion and disconnected message conflict. Knowing in advance that this is a recording reduces this friction; the images do not establish an actual connection requirement.

### Low to medium — View switching still relies on icon discovery

Evidence: 23 has a recognizable full map and a video inset with three unlabeled visual affordances. The compact video-primary image has a small map without an explicit “Show map” label. The normal map outcome is now visible, and 24 provides a clear “Exit fullscreen” control, so the broad concern that there is no return path is not supported. The remaining impact is discovery time for an unfamiliar QGC operator, especially distinguishing swap, fullscreen, and hide inset actions. Tooltips or hover affordances may exist but are not shown.

### Low — “Finish selecting” can sound like confirmation before a target exists

Evidence: 11 shows “Finish selecting” while the status is “No target selected.” In 18, the same label appears during replacement. I would initially treat it as a final confirmation action, and during retargeting I cannot tell from the label whether it keeps the old target or finishes a pending new choice. The adjacent instruction and explicit Retarget state substantially reduce this problem. This is wording uncertainty; screenshots cannot show whether a user would actually make an error or how the control behaves.

### Low — Pending model choice and applied model use similar language

Evidence: in 06, “Selected: visdrone26m” appears above a dropdown displaying visdrone9m. The enabled-looking “Use model” provides a strong clue that one is pending. “Current model” versus “Choose model” would make the distinction more immediate, but the sequence is understandable and 08 visibly resolves the discrepancy. This is brief interpretation cost, not an apparent block.

### Low — Persistent technical and unavailable controls add minor noise

Evidence: the Smart setup screens still show a “Classic tracker” CSRT dropdown and “Unavailable trackers...” alongside Smart controls. “Classic tracker” is more informative than the earlier generic label; nonetheless, a Smart-only operator may wonder whether CSRT also needs configuring. In 20, “running on cuda” adds technical detail without helping choose the next action. These elements do not prevent the requested steps in the shown states.

### Low — Target identity is less explicit than target state

Evidence: 13 reports “Tracking target” and 18 reports “Acquiring target,” but the status badge does not name the selected object or class. A small marked region and thin crosshair lines are visible toward the left, while the scene and recording graphics remain busy. I would inspect the video to confirm which object I meant. This may be adequate in motion; still images cannot establish selection accuracy, tracking quality, or whether a clearer identifier appears at other moments.

## Accessibility and visual positives

The visible white outline on “Use model” in 06 supplies a stronger focus cue than color alone. The greyed application button has a clear explanation during active tracking in 20. Text states distinguish no target, tracking, and acquisition without requiring interpretation of box colors. Fullscreen has a labeled exit. The compact image keeps its primary controls separate from map, compass, and telemetry. These observations address several initial concerns.

The remaining text-size concern is greatest for the selection instructions, not the model dialog body. Primary buttons and dropdown arrows remain relatively compact; touch usability, keyboard-only completion, screen-reader labels, and measured contrast cannot be verified from images. The collapsed compact state is clean, but it is not evidence for every small-window state.

## Scope limits

These screenshots make the intended operator sequence substantially easier to understand. They do not prove that model loading, keyboard operation, tracking, cancellation, or view transitions function correctly. The recording’s race titles, timers, and numeral are source graphics, not tracker output. No claim about detector or tracker accuracy is made. This feedback is simulated and should not be reported as real human validation.
