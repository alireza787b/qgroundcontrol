# Independent simulated operator review — slice 3

Reviewer: simulated first-time operator, not a recruited human participant. This is my raw independent feedback from the supplied screenshots. I did not read implementation files or earlier reviews, run the application, or interact with any controls.

Evidence: `02-idle.png`, `03-first-point.png`, `05-direct-box.png`, `07-doubleclick-fullscreen.png`, `08-details.png`, `09-setup.png`, `10-unavailable.png`, and `11-narrow-setup.png` in the supplied `final-ui` directory. I was told the source is local recorded video, no aircraft/PX4 is connected, and following/gimbal motion are future work. I treated the last image as a 480 × 720 application window; the surrounding display's black space is excluded from this assessment. Filenames are identifiers, not proof that the named interaction worked.

## Overall first impression

The basic task is approachable: I see video, a target status, and an obvious “Select target” button. The normal screen is fairly sparse, and the explicit fullscreen exit is reassuring. I would press “Select target,” then click the object or draw a box if instructed.

My main difficulty comes after selection. “Tracking target” at the top and “Selecting” at the bottom appear simultaneously. I cannot confidently tell whether I have finished selecting, whether the current target is still being tracked while I choose its replacement, or whether the next click will change the target unintentionally. That is the most important clarity issue in this set.

Severity here means likely operator confusion from the visible presentation. It is not a claim of a confirmed functional defect or flight-safety failure.

## Findings

1. **Medium — tracking and selection states need a clearer relationship.** In `03-first-point` and `05-direct-box`, the status says “Tracking target,” while the blue action says “Selecting” and the instruction still tells me to click or drag a box. The instruction makes the selection gesture clear, but it does not tell me whether the mode remains active after I choose something. For retargeting, I would probably click another object, but I would be experimenting. I would want a clear indication of “tracking this target” and, separately, “choose a replacement target.” If persistent selection is intentional, say that another click replaces the target. “Cancel tracking” sounds like stopping the tracker; I cannot tell how to leave selection mode while preserving tracking.

2. **Medium — visible target confirmation is inconsistent across the supplied tracking states.** `03-first-point` contains a small yellow box and a very conspicuous yellow cross spanning the video. `05-direct-box` says “Tracking target” but I cannot see an enclosing target box or another unmistakable target marker. As an operator I cannot identify what the system believes it is tracking in that second image. It may be a transient capture or a marker that is not discernible in this frame; still images cannot distinguish that from absent feedback. This deserves an interaction check, especially immediately after choosing a replacement.

3. **Medium — “Live video” misdescribes the supplied recorded source.** The details panel in `08-details` explicitly says “Live video.” Given the supplied local-recording context, I would read this as a current camera feed when it is not. “Video active” would describe reception without making a source-time claim; an explicit recorded-source label would be stronger if the application knows it is replaying a file. The screenshots do not show how much source information the interface actually has. This is a trust issue even in a demonstration, although it is not evidence of an aircraft-control problem here.

4. **Medium — the narrow setup panel fits, but obscures useful video.** In `11-narrow-setup`, the buttons reflow legibly, and I do not see panel text extending past the window. However, the panel sits over roughly the lower third of the visible video and spans most of its width. It could conceal an object I want to select. The “Tracking setup” button looks like the likely way to collapse it, but the open panel has no explicit close/collapse affordance. I would prefer the expanded setup to use more of the available black area beneath the video or otherwise preserve more of the small image while configuring. This concern is about the actual 480 × 720 window, not the unused display space.

5. **Low to medium — tracker reasons are present, but recovery still requires technical knowledge.** `10-unavailable` is readable and tells me that missing requirements belong on the connected PixEagle companion. That is useful: I would not waste time looking for an aircraft parameter to enable these trackers. “Runtime is not installed” and “External camera target controls are not configured” are understandable explanations. “Verified model artifacts” and “Full AI runtime” do not tell a first-time operator exactly what to install or where the configuration lives. I would need a help link, named setup page, or administrator instructions to recover. I cannot judge whether those exist elsewhere.

6. **Low — the setup warning can make the selected tracker look unavailable.** In `09-setup` and `11-narrow-setup`, CSRT is selected, but the immediately adjacent explanatory line is about Smart being unavailable. I can work out that these are different choices, but at first glance I am unsure whether the displayed selection will work. A short positive indication that CSRT is available would remove that doubt. The disabled-looking “Classic” dropdown adds another concept with no visible explanation; I do not know whether it is a mode, a tracker family, or a required setting.

7. **Low to medium — the selection instruction is much smaller than the controls.** “Click the target or drag a box around it” is the most useful first-use guidance in these screens, yet it is tiny relative to the buttons and video. The Smart-unavailable sentence is similarly small. I can read the captures, but I would expect more effort at normal viewing distance or on a small display. Actual text scaling, touch size, contrast over moving scenes, and accessibility cannot be established from these images alone.

8. **Low — some visual noise competes with the task.** The full-video yellow cross in `03-first-point` is far more prominent than the small target box. It helps locate a position, but it also draws my attention across the whole image. The blank light rectangle at bottom left and the disconnected zero-valued telemetry are conspicuous in the normal view; I understand stock QGC controls are retained and cannot attribute their behavior to this change. The fullscreen capture is cleaner. I do not know whether the cross is part of the incoming annotated video or a QGC overlay, so I am describing its visual impact only.

9. **Low — status scope takes interpretation.** “Disconnected” remains the largest connection message while PixEagle can apparently provide video and tracking. The details panel's “PixEagle · No aircraft” helps explain this distinction. Without opening it, I would initially wonder what is disconnected and whether tracking is allowed. “Following inactive” in that same panel sounds like an available function currently switched off; because following is future work, it could create an expectation that I can turn it on somewhere. I am not requesting a following control for this slice, only clear language about current capability.

## What is already clear

- Idle state gives an obvious next step, and the disabled “Cancel tracking” is consistent with “No target selected.”
- “Cancel tracking” is plain language for stopping an active track; the ambiguity concerns leaving selection mode, not the meaning of stopping tracking.
- The click-or-box instruction describes the two selection gestures in familiar language.
- The fullscreen view preserves the tracking controls and presents an explicit “Exit fullscreen” button. I cannot verify the double-click gesture from a screenshot.
- Setup is collapsed by default, keeping tracker configuration out of the main idle view.
- The unavailable-trackers dialog separates tracker names and individual reasons cleanly, includes a visible close action, and is easier to read than the small inline setup warning.
- No screenshot suggests that following or gimbal movement is actively occurring. No aircraft is connected according to the visible status and supplied context.

## Checkpoint judgment

I would be comfortable using these screens for an attended local-video tracking demonstration after a brief explanation. I would not yet call the first-use retargeting flow self-explanatory. The first checks I would want are whether I can tell which object is tracked immediately after a selection, whether I can leave selection without cancelling the track, and whether a second target selection has predictable visual feedback. Those are test requests, not conclusions from these still images. Clarifying the recorded-source status and reducing narrow-screen video occlusion would also improve confidence.
