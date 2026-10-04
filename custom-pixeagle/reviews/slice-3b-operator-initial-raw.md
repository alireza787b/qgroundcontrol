# Simulated operator review — initial raw feedback

This is a simulated operator usability review by an AI, not real human operator feedback. It is based only on the four supplied screenshots. No implementation, previous conclusions, or functional behavior was inspected. The filename “04-escape-closes.png” is not proof that Escape works. This document preserves the initial feedback and should remain unchanged; follow-up feedback should go in a separate file.

Task: I have a recorded PixEagle stream inside QGC without an aircraft. I want to choose an installed Smart detector, start target tracking, change the target, stop tracking, and return to normal QGC views.

Screenshots inspected:
- final/01-recorded-video.png
- final/02-model-dialog.png
- final/03-model-details.png
- final/04-escape-closes.png

## First interpretation

In 01, I see an aerial video with many pedestrian labels, a small map, a compass, and zero-valued flight telemetry. “No target selected” near the top is reassuringly direct. “Select target” at the bottom looks like the main next action. “Cancel tracking” appears disabled, which is understandable with no target selected. “Tracking setup” sounds like the place to choose my detector.

The prominent top message says “Disconnected - Click to manually connect.” Because I know my task uses a recording without an aircraft, I hesitate: is this expected aircraft status, or does tracking need a connection? Nothing visible distinguishes a recorded source from a live source, identifies a playback state, or explains whether the PixEagle companion is separately connected. Existing pedestrian labels could be recorded annotations or current detections; I cannot tell from these images.

## Actions I would try next

1. Open “Tracking setup,” then “Smart model...” to find the installed detector.
2. In 02, open the model dropdown currently showing “visdrone9m.” Read “visdrone9m running on cuda.” as suggesting that this model is already active. I would use “Model details” to understand what it detects; 03 provides a useful list of people and vehicle classes.
3. If I chose another model, I would expect “Use model” to apply it. In the shown state it is greyed out, so I would assume the listed model is already applied, but that reason is not explicit. I cannot see the available choices in a closed dropdown.
4. Close the dialog. The instruction “Close this panel to enable Smart mode” would make me look for an additional enable step. In 04, the compact control already says “Smart,” beside “Tracker CSRT,” so I cannot confidently tell whether Smart is enabled, merely selected, or waiting for another action.
5. Press “Select target.” I expect to choose something in the video, but the screenshots do not tell me whether to click a detection, click an arbitrary point, or drag a rectangle. I cannot judge the guidance that may appear after pressing it.
6. To change a target, I would try “Select target” again or cancel first. No supplied image shows a selected or tracked target, so I cannot discover the intended replacement action or verify its wording.
7. To stop, I would try “Cancel tracking.” Its relationship to stopping an active track is plausible, although “Cancel” also sounds like leaving an unfinished selection. The images do not show whether the label or enabled state changes during tracking.
8. To return to normal QGC, I would try toggling “Tracking setup” closed and then clicking the small map, or the top-left QGC icon. These are guesses. “Close” clearly exits the model dialog, but no visible control explicitly restores the normal QGC view.

## Findings with severity and image evidence

Severity describes likely task friction from what is visible, not a verified functional defect.

### High — Smart activation is unclear after choosing a model

Evidence: 02 and 03 say “Choosing a model keeps the current tracking mode. Close this panel to enable Smart mode.” The model also says “running on cuda.” In 04, “Smart” is already displayed beside “Tracker CSRT,” without a plainly worded active-mode status or next-step hint. I cannot confidently distinguish a loaded detector, a selected tracking mode, and an active track. The small dropdown presentation makes the activation instruction especially easy to misinterpret as “closing enables it.” A clear statement of current mode and the exact next action would reduce this hesitation.

### Medium — Disconnected status competes with the recording-only workflow

Evidence: every image has the dominant “Disconnected - Click to manually connect” banner, alongside flight controls and zero telemetry. Meanwhile 02/03 say the PixEagle model is running. The screen does not visually distinguish aircraft connection, companion status, and recorded video. I might try to connect an aircraft unnecessarily or misread an expected disconnected aircraft as a tracking fault. This is an interpretation risk, not proof that aircraft connection is required.

### Medium — Target selection and replacement affordances are incomplete in the shown states

Evidence: 01 and 04 expose “Select target” and disabled-looking “Cancel tracking,” while the status remains “No target selected.” The video contains many tiny objects and labels. There is no visible instruction for choosing one, no example selection outline, and no shown “Change target” state. Starting selection is discoverable; the gesture and subsequent replacement path remain uncertain. Later selection/active-tracking screenshots are needed before concluding whether the UI supplies adequate guidance.

### Medium — Return to normal QGC views is not explicit

Evidence: 04 shows the model dialog absent but the tracking setup panel still expanded. The small map at lower left has no visible “Map,” “Restore view,” or expand affordance. The top-left app icon is unlabeled. I have plausible places to try but no clear route. This screenshot cannot establish keyboard behavior or whether a view switch is available on hover.

### Medium — Detection annotation density makes picking a small target harder

Evidence: all four images show large dark pedestrian-label blocks overlapping or crowding the aerial scene, particularly across the lower roadway and upper crowd. The actual people are small relative to the labels. I struggle to associate each label with its object and to see a clean picking location. These annotations may be baked into the recording; the screenshots do not establish their origin or whether QGC can hide them.

### Medium — Readability and control size may limit low-vision or touch use

Evidence: at the supplied 1440 × 900 size, panel body text and bottom control labels are compact, the mode and model disclosure arrows are tiny, and bottom buttons are roughly 35 pixels high. The disabled-looking “Use model” and “Cancel tracking” labels have weak visual separation from their grey backgrounds. The modal dimming helps isolate the dialog, and its white body text is substantially clearer than the video annotations. No measured contrast ratio or physical display size is available, so this is a visual concern rather than an accessibility conformance result. Keyboard traversal, focus visibility during keyboard use, and screen-reader support cannot be assessed from these screenshots.

### Low — Technical wording and unavailable options add setup noise

Evidence: 02/03 show “cuda”; 04 shows “CSRT” and “Unavailable trackers...” next to the Smart controls. These do not explain what I should choose for the task. The model details’ class list is useful, but “Task: detect” and the simultaneous detector/tracker terminology do not clarify their relationship. “Unavailable trackers...” competes with the actions I can actually perform. “Use model” being disabled would be clearer if accompanied by a concise “Already in use” state.

## What is working visually

“No target selected” gives a clear initial target state. The three primary controls are grouped consistently at the bottom. “Tracking setup” visibly highlights when expanded in 04. The Smart model dialog has an obvious “Close” button, a recognizable model dropdown, and a separate optional details area; 03 makes the detector’s supported classes inspectable without burying the initial choice. These are observations about presentation, not confirmation of behavior.

## Limits and follow-up needed

These images show no selection gesture, active track, replacement target, stopped track, or restored normal QGC view. I cannot infer that those functions succeed or fail. Follow-up images of those states, plus an expanded model list and any selection guidance, would let me review the rest of the task. I am waiting for those follow-up screenshots.
