# Slice 2 review decisions

The [raw first review](slice-2-operator-raw.md) and
[raw follow-up](slice-2-operator-followup-raw.md) are simulated screenshot reviews,
not findings from participating human operators. The reviewer received tasks and
captures, not implementation rationale. Still images do not prove routing,
freshness, recovery or gestures; tests and interaction logs supply that evidence.

Evidence root: `~/.cache/pixeagle-qgc-baseline/slice-2-2026-09-21/`.

| Observation / impact | Decision | Follow-up evidence / owner |
| --- | --- | --- |
| Connected setup still asks to sign in; Retry looks like a required next step. Low–medium confusion. | Hide sign-in instructions when authenticated. Add Open Fly View as the primary next step; move Retry under connection details. | `final-replay-ui/ui/04-connected.png`; clicked Open Fly View, resulting moving video in `05-video.png`. Slice 2. |
| Companion-only and aircraft Disconnected appear contradictory. Medium confusion. | Use “PixEagle · No aircraft” and “PixEagle connected. No aircraft in QGC.”; retain the stock aircraft toolbar. | `04-connected.png`, `05-video.png`, `08-portrait-video.png`. Slice 2; human feedback still pending. |
| Activity text does not communicate whether controls are available. | Add “View only” beside live runtime status. Keep explicit target/following restrictions in settings. | Final video captures. Target controls remain slice 3; following is slice 4. |
| Small PiP status was clipped, preventing freshness assessment. High impact. | Use a one-line owner/freshness label in compact video, amber when stale, with a full accessible description. | Final `07-portrait-pip.png` (Live); `multi-ui/ui/12-portrait-pip-frozen-unhovered.png` (Delayed). Hover controls can temporarily cover the very smallest inset; assess touch/device layouts in slice 5. |
| Stale frame gives no next action or age. Medium impact. | Keep stale image visibly amber; full banner now says to check the PixEagle source. Do not invent a paused-vs-network diagnosis or add a backend play control. | Synthetic freeze/drop tests and screenshots. Numeric conservative age and richer recovery UX remain feedback candidates. |
| Aircraft instruments clutter a no-aircraft preview. Medium impact. | Preserve standard QGC UI as required. Fix external-video fullscreen without aircraft; fullscreen removes stock flight chrome for viewing. | `final-replay-ui/ui/06-fullscreen-no-aircraft.png`; entry and exit exercised. Further changes await human feedback. |
| Camera name/source changes are not explicitly identified in the app banner. | Keep aircraft ownership prominent. Provenance clears old pixels on source/epoch changes; technical IDs do not become noisy operator labels. Human-readable source naming is a future UX decision. | Synthetic camera-a → camera-b, raw/OSD and resolution tests; displayed-pixel provenance tests. Carry into slice 3 contract review. |
| Address and credentials require external setup knowledge. | Supply a concrete demo launcher and private credentials handoff. General provisioning stays outside these slices. | README desktop demo steps. |
| Portrait inset controls are hard to discover. | Retain the stock PiP interaction; document map/video swap and double-click fullscreen. Validate on touch targets in slice 5. | 480×720 and 1440×900 captures; no Android qualification claimed. |
| Follow-up: no persistent fullscreen exit action is visible. High discoverability impact on touch. | Add a single Exit fullscreen button, visible only in custom fullscreen video. Raise the external fullscreen surface above the stock gesture area so this button receives clicks; keep double-click exit. | `click-final-ui/ui/01-fullscreen.png` through `03-double-click-returned.png`: both exit paths passed. Custom 8/8 and stock 5/5 focused tests passed. The first button capture looked correct but its click was blocked by the gesture area; preserved in `fullscreen-final-ui` as failed interaction evidence. Final raw addendum rates discovery impact low; actual device touch-target qualification remains slice 5. |

The recorded source itself contains a target box and an older recorded UI; its
pixels are not a current tracker overlay. Current native status reports tracking
and following inactive. This distinction is stated in the hands-on demo steps.

The user deferred hands-on tracker testing until the end of slice 3 and requested
a pause/recap after slice 2, followed by explicit confirmation before slice 3.
The [raw checkpoint decision](slice-2-user-checkpoint.md) is retained; it is a
scheduling decision, not human usability-test feedback. Preserve actual test
feedback verbatim when it arrives, then update decisions separately.
