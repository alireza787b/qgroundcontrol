# Slice 3b feedback disposition

The [initial review](slice-3b-feedback-design-raw.md) and
[visual follow-up](slice-3b-feedback-followup-raw.md) are preserved unchanged.
Both are simulated expert/operator feedback, not human acceptance.

| Observation | Disposition and evidence |
| --- | --- |
| Configuration competes with target operation | Classic/Smart now leads; only the relevant tracker/model dropdown is shown. Final private screenshots confirm the hierarchy. |
| Unavailable algorithms look like required actions | Diagnostics moved into Settings connection details. |
| Selection controls obscure direct retargeting | Default-on **Tap video to select targets** removes arming controls; manual preference retains one-gesture Select/Retarget. Controller tests and private Classic revision checks pass. |
| Disabled integration should leave Fly View clear | Default remains false; launcher now matches. Private disabled-state capture confirms no overlay. |
| Sign in must have a destination | Opens the existing PixEagle settings page; private interaction and destination capture verified. |
| Busy model choice must not claim success early | Dropdown reflects confirmed backend state; busy feedback, guarded choices, deferred refresh and retry notice clearing tested. Inline change to visdrone26m confirmed by backend. |
| Model loading says Updating target | Minor wording follow-up retained for the next feedback pass; does not block the user-requested handoff. |
| Recorded TRACKING conflicts with native idle state | Explain test4's existing recorded graphics in the handoff. Do not infer current target state from the recording. |
| Compact layout, touch, double-click/fullscreen, latency | No new compact-layout qualification claimed after a driver failure. Keep these as human/future coverage; still images do not establish gesture behavior. |

No visual blocker was found in the final review. Proceed with the requested
hands-on Smart/model/retarget test, then correlate user observations with saved
logs before further changes. Physical gimbal/camera and following remain later
work. See [the checkpoint](../SLICE-3B-FEEDBACK.md) for executable evidence and
remaining limits.
