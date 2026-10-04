# Slice 4a compact controls: simulated operator review

Date: 2026-09-27.

This is an AI-generated review from an operator perspective. It is not feedback
from a human drone operator, a usability study, or field qualification. The review
covers source inspection and two saved desktop screenshots at 1440 by 900 pixels.

Reviewed evidence:

- `feedback4-ui/compact-final.png`: final compact panel, corrected palette and gear
  icon, connected video, Classic selected, CSRT, no target, follower stopped.
- `feedback4-ui/options-first.png`: earlier Options layout before the palette/icon
  fix. The dimmed compact panel in this image is not evidence of the final colors.

Both screenshots are under the private cache directory
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/`.

## Raw simulated observations

"I can see Classic and Smart without opening a menu. The highlighted Classic
button makes the current choice clear. CSRT and the aircraft follower occupy
separate rows, and No target versus Stopped tells me which system is waiting."

"The target and aircraft icons help distinguish the rows. The short state words
are still useful; a colored dot alone would not explain whether I had no target,
had lost a target, or had lost communication."

"The gear now reads as Options. Opening it gives me the active tracker and
follower choices, followed by Reset panel position. I do not have to read a
tutorial or work through general settings to change these choices."

"The connection icon is with the other QGC indicators, and the previous permanent
connection banner no longer covers the video. Its exact meaning still depends on
the tooltip or connection drawer, which should be checked during the handoff."

"The panel covers some of this portrait video's lower center. The visible grip
and reset command give me a way to move it. I still need to try that while selecting
a target to establish that moving the panel never selects something underneath."

"The aircraft controls and map remain visible in this desktop layout. The large
telemetry text around the recorded scene comes from the displayed video; these
screenshots do not establish that the native PixEagle panel adds those labels."

## Command feedback finding and disposition

The source review found that a failed Start or Stop could be hidden when its
button remained available: the explanatory label was conditional on both actions
being unavailable. A restored Ready state could therefore conceal a rejection.

The implementation now exposes `followingActionError` separately and displays
nonempty action errors independently of Start/Stop readiness. Local rejection of
a stale Start context also sets explicit retry feedback. This resolves the source
finding; neither screenshot exercises a rejected command, so screenshot review
alone does not establish the failure-path behavior.

## Acceptance limits

The shown idle Classic layout is suitable for the next operator checkpoint.
There is no screenshot-based reason to begin another cosmetic redesign.

The following remain interaction checks, not claims established by these images:

- Select and retarget, then verify distinct acquiring, tracking, and lost states.
- Hold Start through the standard QGC confirmation animation; a short press must
  not start following. Stop must act immediately without another confirmation.
- Disconnect or expire status while following; confirmed activity must become
  uncertain, with the appropriate Stop recovery still accessible.
- Reject Start/Stop and confirm the actionable error remains visible.
- Move and reset the panel using mouse and touch; resize and toggle fullscreen
  without losing access to the panel or producing an unintended video selection.
- Change vehicles while holding a control or using Options; the old intent must
  not operate the newly selected aircraft.
- Inspect Smart model selection and the tracking/following layout with a target.
  These screenshots show neither Smart nor active following.

Small mobile screens, sunlight readability, physical gimbal behavior, and real
aircraft operation are outside this screenshot review.
