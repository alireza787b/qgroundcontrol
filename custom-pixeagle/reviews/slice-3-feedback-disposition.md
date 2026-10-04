# Slice 3 operator feedback checkpoint

The adjacent `slice-3-operator-*-raw.md` files preserve the independent agent's
responses without rewriting. These are simulated operator reviews of screenshots,
not feedback from a human operator and not proof of runtime behavior. The reviewer
did not inspect implementation code. Screenshot evidence remains in
`~/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21`.

The first review used `replay-ui/ui` captures. The layout addendum used captures
09 and 10 from that directory. The handoff review used `handoff-ui/ui` captures
02, 03, 04, 05, 07 and 10. The handoff screenshots precede the final catalog
availability and duplicate-name correction.

## Changes before the human test

- Removed the contradictory read-only connection wording for operator sessions.
- Replaced the older recording's burned-in tracking state with the clean soccer
  recording. Disabled optional demo OSD while retaining actual tracker feedback.
- Added authoritative target-loss reporting and allowed Cancel to clear lost
  state without needing a fresh video frame.
- Kept target controls visible in narrow main-video layouts. The expanded panel
  is now above stock compass, telemetry and PiP controls rather than overlapping
  them; portrait capture 10 confirms this placement.
- Moved fullscreen exit away from the connection status panel.
- Corrected the tracker catalog's unavailable aliases and duplicate names after
  the real GUI interaction exposed them.

## Questions carried from the first human test

- Stock aircraft **Disconnected** and companion **Live video** can appear to
  conflict. The demo instructions explain their separate meanings; retain stock
  flight controls and review the user's actual confusion before further changes.
- Expanded setup and backend crosshairs cover parts of the video. Check whether
  the user can select and follow a small moving target comfortably.
- Tracker versus Classic mode, immediate application of a tracker choice, and
  the next step for unavailable Smart mode may need clearer wording.
- Selection gestures are explained after arming; check discoverability before
  arming and clarity of point versus rectangle target extent.
- The recording includes title cards and scene changes. These are part of the
  test media and can legitimately lose a target; they are not PixEagle UI text.
- Physical-companion naming and real camera/model usability need later qualified
  testing. This local demo identifies a loopback companion with no aircraft.

## Human feedback and slice 3a disposition

The user's original observations and later click clarification are preserved in
[raw JSON](slice-3-user-feedback-raw.json). The user reported a successful overall
demo and requested quick replacement gestures, click verification, complete
tracker discovery, and less persistent connection text.

| Observation | Action and evidence |
| --- | --- |
| Retarget required an extra button press | Accepted native selection keeps selection mode available; each next press pins a fresh frame and guard. Explicit finish, context change, uncertain outcome and fullscreen cancel that mode. Focused controller/adapter regressions and real point → point → box replay passed. |
| Single click might not work | Existing logs record accepted small selections but cannot prove their gesture type. New Release replay accepted a real point and direct replacement. Initial arming remains required; Qt's double-click interval prevents fullscreen clicks sending target actions. |
| Only three Classic trackers visible | Choices use backend catalog and canonical identities. Core replay has three runnable implementations; a separate dialog explains models, library or camera requirements for the rest. No arbitrary tracker whitelist was added. |
| PixEagle/title/no-aircraft text is noisy | Normal view uses one compact camera/status button; details reveal ownership and video condition. Errors remain visible. Standard QGC aircraft UI remains. |
| Smart model choice and gimbal controls | Model choice was missing. The [coverage plan](../OPERATOR-COVERAGE.md) explicitly assigns it to slice 3b and movement/following to slice 4, with contract and runtime gates. |

## Independent review of the feedback build

[Raw simulated response](slice-3-operator-feedback-raw.md) is unchanged. The
reviewer saw screenshots before the last wording refinements and did not read
implementation or earlier reviews. Findings are not human usability validation.

- **Selection clarity:** changed the checked button from Selecting to Finish
  selecting, and after acceptance changed the hint to explain replacement.
  Finishing selection preserves the track; it differs from Cancel tracking.
- **Recorded source:** changed Live video/Live to Video active/Active. These
  describe receipt without claiming a camera's capture time or replay awareness.
- **Duplicate status:** final capture exposed independent runtime polling showing
  Tracking inactive below a newer Tracking target badge. The details no longer
  repeat the slower runtime status when the authoritative target controller is
  available; target/following restrictions remain in its visible status.
- **Missing target marker in one still:** read-only observations showed target
  loss/recovery around these captures. Acceptance is not stable acquisition.
  Backend annotations and polled status are not synchronized; retain this limit
  and assess actual Smart/external feedback in slice 3b. Do not draw a guessed
  native box from unrelated telemetry to make the image look consistent.
- **Narrow setup occlusion:** controls fit at 480 × 720 and leave stock compass
  and telemetry accessible. Expanded setup still covers part of the image;
  collapse it during selection. Review layout with model controls in slice 3b.
- **Technical availability reasons, small hints, Classic/Smart wording:** keep
  these as open operator feedback for slice 3b onboarding/layout. The current
  dialog is readable and keyboard Escape/Close works in the private replay.
- **Stock Disconnected and annotation crosshairs:** retain scope distinction;
  the aircraft is absent, and annotations originate in PixEagle. Broader OSD
  controls remain in the tracked media follow-up.

The [feedback checkpoint](../SLICE-3-FEEDBACK.md) records exact test scope,
intermediate failures and evidence. Current fixes do not qualify camera hardware,
Smart performance, following or the Windows/Android release.
