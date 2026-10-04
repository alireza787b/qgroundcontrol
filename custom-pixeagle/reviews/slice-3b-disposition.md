# Slice 3b operator review disposition

Recorded 2026-09-24. Both raw reviews are simulated independent AI feedback,
not a human study. Preserve them unchanged. Runtime and interaction reports
establish behavior separately from screenshot interpretation.

| Finding / impact | Action and evidence | Remaining owner |
| --- | --- | --- |
| Initial high: model choice could be mistaken for Smart activation | Added configured/running state and explicit next-step instruction. Follow-up 05/08 finds the sequence clear; GUI check confirms closing keeps Classic. | Human slice 3b acceptance |
| Model refresh could display a different pending choice | Restored binding after inventory updates; 06/08 show keyboard-selected model and authoritative applied state. | Resolved in slice 3b |
| Keyboard focus was weak | Added QGC-palette focus outline; 06 and actual Tab/Space application check. | Broader accessibility/platform acceptance in slice 5 |
| Compact tracking panel overlapped flight instruments | Use actual QGC tool insets; 1024 × 768 collapsed capture clears compass/telemetry/map. | Expanded/touch layouts remain later acceptance |
| Crowded passive labels obscure imagery | Existing passive-label option disabled only in the private replay; source race graphics retained and identified. No production detection thresholds changed. | Human scene-dependent feedback |
| Selection/retarget discovery | Armed gesture instruction and replacement text; 11/18 plus distinct accepted target revisions. Recording loop disarming is documented. | Human slice 3b acceptance |
| Follow-up medium: essential instructions are too small | Retained verbatim; no blocker established by screenshots. Ask operator to assess readability on the real display before changing layout again. | Slice 3b feedback pass |
| Follow-up medium: aircraft Disconnected versus active companion | Preserve stock aircraft banner; connection details identify companion-only state; handoff explicitly identifies recorded source and no PX4 requirement. | Human feedback, onboarding/status refinement |
| Follow-up low–medium: map/video switching icons require discovery | Stock inset controls preserved; map and fullscreen transitions checked (23–25), explicit Exit fullscreen available. | Slice 5 onboarding/accessibility; collect feedback now |
| Follow-up low: Finish selecting, pending versus applied model wording | Document exact semantics in handoff; retain the raw interpretation for human evaluation. | Slice 3b feedback pass |
| Follow-up low: dormant Classic setup, device terminology and target identity | Classic field labeled; device detail kept in model dialog. Still images cannot prove tracking quality. | Human feedback and later settings refinement |

No high-severity usability blocker was established in the follow-up screenshots.
That does not establish usability, touch accessibility, contrast compliance or
detector accuracy. The user explicitly requested a hands-on handoff rather than
prolonged automated Smart testing; remaining presentation feedback is carried
forward to that checkpoint. Gimbal hardware and following remain outside it.
