# Slice 4b.0 screenshot review

Date: 2026-09-30.

Reviewer: independent AI-simulated operator and UX reviewer. These are raw review
observations, not feedback from a human operator, a usability study, or aircraft
qualification. One bounded visual review was performed; no service, configuration,
or runtime code was changed.

## Evidence

The reviewer opened these five private screenshots at their original 1440 by 900
pixel size using the image viewer:

- `01-start.png`
- `02-settings.png`
- `03-signedin.png`
- `04-fly.png`
- `05-smart-options.png`

All are under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b-2026-09-29/ui/`.
The images are not copied into the repository. This review does not infer the
duration, frame rate, or successful completion of operations from still images.

## Raw simulated observations

"On the first screen, Sign in is the obvious next action. The small PixEagle name
in the toolbar tells me which integration needs attention. The otherwise black
video area does not tell me whether a camera exists, but the sign-in prompt gives
me an action rather than leaving an unexplained blank screen."

"In Settings, I can read the entered backend address clearly. The note explains
the local default and separates it from the web dashboard. Username and password
are in the expected order. Dashboard, Connection help, and About PixEagle are
actions rather than misleading checkboxes. This image does not test the empty
address placeholder or its contrast."

"After sign-in, Verify vehicle is clearly the next step for the selected Vehicle
1; Open Fly View is disabled. The additional Following status unavailable line
can sound like another fault during this expected setup step. I would prefer the
verification instruction to explain this condition without repeating a second
unavailable message. This is a wording followup, not evidence that verification
or companion-only operation is broken."

"The compact Fly View panel identifies itself as PixEagle. Classic and Smart are
the largest choices; Classic is visibly selected. The target row says CSRT and
No target, while the aircraft row says MC Velocity Position and Stopped. The icons
and words together are more understandable than colored dots alone."

"The panel occupies some of the portrait video, but the grip makes movement
discoverable and the panel is compact. I would move it toward a free area before
selecting vehicles in this scene. The screenshot cannot tell me whether dragging
works or accidentally selects video underneath."

"The toolbar now says PixEagle and shows separate target and aircraft states. It
no longer requires me to interpret a generic camera icon as PixEagle. Its state
words are noticeably smaller than the actual flight-mode label, so I need to
check this on the intended tablet or smaller display. The actual Hold flight mode
remains separately visible."

"Smart Options gives me a model dropdown in place of the Classic tracker choice.
The follower remains a separate choice. Settings and Dashboard are immediately
available. This is a useful division between operating controls and setup; I do
not need another page merely to choose an installed model."

"The Smart model message says it is running on cpu after device fallback. Above
the dialog I can also see Video delayed or paused. Check the PixEagle source. I
cannot tell from the picture whether these are related. Before testing tracking,
I need reliable moving video or a clear explanation and recovery path. It would
be premature to interpret the visible model name as a successful detection test."

"The background has large telemetry labels in addition to the native QGC
instruments. Those labels appear within the video presentation; these images
alone cannot establish whether they were recorded into the clip or composed by
the live backend. The planned OSD setting should make its scope clear. It must
not imply that it removes native QGC instruments or text baked into a recording."

## Disposition

No visual defect in these desktop screenshots warrants another broad layout
redesign. Preserve the accepted compact panel and contextual Options workflow.

The unresolved delayed-video condition shown in `05-smart-options.png` is a
functional handoff gate for a Smart tracking demo: establish its cause, confirm
recovery, and obtain sustained fresh video with a successful Smart interaction.
The screenshot does not prove finite replay completion, CPU fallback as the cause,
or a transport failure. Do not label it resolved based on this visual review.

Bounded followups, with no requirement to expand this checkpoint into cosmetic
iteration:

- Check toolbar status legibility at the intended smaller screen size and UI
  scaling. Preserve text or another non-color distinction for activity states.
- Consider suppressing the redundant following-unavailable sentence while the
  explicit vehicle-verification instruction already explains the next action.
- Use ordinary operator wording and uppercase CPU when device fallback is shown;
  keep detailed device diagnostics in Options or connection details.
- Confirm that video-source faults have an actionable route to backend source
  settings or Dashboard. A button named Settings alone does not establish that
  the destination can resolve the fault.

## Checks still requiring interaction or runtime evidence

- Enabled-by-default behavior is not inferred: these screenshots show an already
  enabled demo profile, not a fresh installation.
- Blank-address default, sign-in failure, session expiry, and recovery.
- No-aircraft use and explicit verification for the selected aircraft.
- Smart detection, selection, retargeting, both installed model choices, and
  selected-versus-running model state through a change or failed load.
- Hold-to-start, immediate Stop, persistent command failure feedback, and loss of
  target/video/status while following.
- Vehicle switching during pending operations and provider changes.
- Panel movement/reset, touch input, resizing, PiP, fullscreen, and preventing
  accidental target selection underneath a dragged control.
- Independent tracking and aircraft-following freshness: No target and Stopped
  must reflect observed current states, not be substitutes for unknown state.

Neither active tracking nor active following is shown in these images. Camera
tracking, manual gimbal movement, outdoor visibility, and physical aircraft
behavior remain outside this review.
