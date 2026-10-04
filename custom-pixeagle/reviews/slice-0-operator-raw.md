Simulated agent operator review — not a real operator study.

I reviewed the seven supplied screenshots without inspecting implementation changes or their rationale. This review assesses what an operator can infer from the images. It does not establish interaction success, touch usability, command routing, latency, flight readiness, or regression relative to stock QGC.

Severity: **High** could affect command destination or timely intervention; **Medium** could cause hesitation or a wrong interpretation; **Low** adds friction. Conditional findings concern future operational use, not a claim that unfinished slice-0 functionality is defective.

**03-fly.png**

- **UXR-01 — Medium: two meanings of “selected” are difficult to distinguish.** “Vehicle 2” appears at the top right and its card is green, while the adjacent panel says “Vehicles Selected: -”. The operator can identify the active vehicle, but could reasonably wonder why the action panel reports none selected. This matters when determining which vehicle would receive a command. The image supports ambiguity in wording; it does not demonstrate misrouting.
- **UXR-02 — Medium: disabled actions give little immediate explanation.** Arm, Disarm, Start and Pause are greyed out. “Vehicles Selected: -” suggests a cause, but the connection is implicit. An operator trying to pause would have to discover the selection model first. Both cards explicitly show Disarmed; the image cannot establish which controls should be available in that state.
- **UXR-03 — High before interactive targeting: the video owner is not visible.** The PiP image is labelled “PXE QGC TEST” and “MJPEG/WS”, without an aircraft or endpoint identity. The nearby active-vehicle indicator does not prove that this image belongs to Vehicle 2. That association must become evident before an operator can safely select targets.
- **Supported positive:** aircraft numbers, mode and disarmed state remain readable for both vehicles, including the inactive one.
- **Fixture limitation:** the blank map and overlapping aircraft labels prevent evaluation of geographic awareness and spatial vehicle selection. Their presence alone does not establish a product defect.

**04-video-main-vehicle-one.png**

- **UXR-04 — Supported positive:** the top-right label changes to Vehicle 1 and the green highlight is on card 1. The screenshot presents consistent active-vehicle cues.
- **UXR-05 — High before interactive targeting: the enlarged image still has no visible vehicle association.** The operator can tell that Vehicle 1 is active, but cannot verify whose camera is being displayed. The synthetic “F2” marking is not an aircraft identifier.
- **UXR-06 — Medium: the multi-vehicle panel occupies a substantial part of the primary video while displaying mostly unavailable actions.** This reduces unobstructed viewing area. Whether it obscures a target in practice requires representative footage; the test pattern cannot answer that.
- **UXR-07 — Low: PiP actions are visible but icon-only.** The map inset includes recognizable window/corner controls and a collapse-like chevron. Their exact outcomes are not explicit in the image. Mouse hover help, keyboard access and touch discovery were not tested.
- **Supported positive:** Takeoff/Return positions and flight instruments remain visible with video as the primary surface.
- **Fixture limitation:** the enlarged test image is visibly pixelated. This is evidence about this captured synthetic source, not proof of a decoder or scaling defect.

**05-fullscreen.png**

- **UXR-08 — High before following is enabled: fullscreen removes operational context and intervention controls.** No aircraft identity, following status, Stop, Pause, Return or flight instruments are visible. A user needing an immediate intervention must first leave fullscreen. This screenshot is a concrete constraint for later following work; it is not evidence of a slice-0 integration regression.
- **UXR-09 — Supported positive:** “Double-click to exit full screen” gives an explicit recovery instruction for mouse users at the captured moment.
- **UXR-10 — Medium, interaction unverified: recovery is described solely as a mouse gesture.** There is no visible exit button or touch instruction. I cannot determine whether double-tap, Escape or another mechanism works, nor whether the displayed instruction persists.
- **Fixture limitation:** the absence of a real scene prevents judging whether the fullscreen presentation preserves enough detail for target selection.

**07-mission-items.png**

- **UXR-11 — Supported positive:** the current task is legible: a waypoint is selected, its relative altitude is shown as 50.0 m, and the path and altitude profile are visible. Open, Save and Upload remain prominent.
- **UXR-12 — High before live upload, verification needed: the upload destination is not apparent in this capture.** Unlike Fly View, the visible header does not identify an active aircraft. With two vehicles present in the session, an operator cannot confirm the destination from this image alone. An upload confirmation or another interaction may provide it; that was not observed.
- **UXR-13 — Low: “Mission Items — 2 items” appears alongside Initial Camera Settings, Takeoff and Waypoint entries.** An unfamiliar operator might pause to reconcile the count with the visible rows. This is existing workflow friction, not a request to redesign mission editing.
- **Fixture limitation:** missing map imagery and overlapping aircraft labels prevent assessment of waypoint placement relative to terrain, obstacles or individual aircraft.

**ws-settings.png**

- **UXR-14 — Supported positive:** the selected source, “WebSocket JPEG Video Stream”, and editable URL are clearly grouped. The reconnect setting has a readable label and visible enabled state.
- **UXR-15 — Medium: source setup is mixed with substantial technical tuning.** The operator encounters aspect ratio, RTP buffering, CPU video path and decoder priority alongside the connection address. In particular, the RTP-specific descriptions do not explain their relevance to the selected WebSocket JPEG source. This can encourage unnecessary tuning while diagnosing a connection problem.
- **UXR-16 — Medium: no connection outcome is visible.** This image does not show connected/disconnected status, the last failure, or a verification result near the URL. An operator would need another view or interaction to determine whether entry succeeded. Other portions of the application may provide that feedback.
- **UXR-17 — High before paired operation: the settings do not identify which aircraft owns this source.** The page appears application-wide. It cannot serve as evidence of a verified aircraft-to-PixEagle association.
- **Fixture limitation:** `ws://127.0.0.1:8095/ws` is a local test endpoint. It does not establish production transport security, authentication handling or secret persistence.

**ws-fly.png**

- **UXR-18 — Supported positive:** Vehicle 1 is consistently identified by the header and green card while Vehicle 2 remains visible.
- **UXR-19 — Medium: the PiP image has no visible interaction affordances in this capture.** Unlike the map inset in `04-video-main-vehicle-one.png`, no swap/enlarge controls are displayed here. They may appear on hover or interaction; the screenshot does not establish that. A new operator could miss the route to the larger video view.
- **UXR-20 — High before interactive targeting: source ownership remains unverifiable.** The PiP test image carries no visible endpoint/aircraft association. Switching the highlighted vehicle alone is insufficient evidence that the stream also switched.
- **UXR-21 — Evidence limit:** a single “F2” image cannot establish that video is live or that stale/frozen frames are identified. No separate freshness indicator is visible.
- The active-versus-group-selection ambiguity from UXR-01 is also present here.

**flight-controls.png**

- **UXR-22 — Supported positive:** the expanded flight-mode menu visibly contains RTL, Land, Loiter and Position Hold. The active Vehicle 1 identity remains visible. This establishes discoverability in the open-menu state, not successful command execution.
- **UXR-23 — Medium: intervention requires understanding two control areas.** The mode menu offers RTL and Land, while the multi-vehicle panel shows a disabled Pause and the left Return control is disabled. The image does not explain how these availability states relate. The disarmed mock state may explain them; actual airborne behavior must be checked separately.
- **UXR-24 — Medium before following controls arrive: “Follow” already names a flight mode.** Adding another undifferentiated “Follow” action would create an immediate interpretation risk. Future screenshots should make it possible to distinguish aircraft flight mode from PixEagle aircraft-following state without prior explanation.
- **UXR-25 — Low: menu recovery is only partly evident.** A circular right-arrow control appears at the top of the open menu, but it is not explicitly labelled as closing or advancing. Dismissal by clicking elsewhere may work; it was not tested.

**Bounded disposition**

The captures support that stock-looking Fly View, vehicle cards, mission editing, video enlargement and the flight-mode menu remain present. They do **not** establish safe PixEagle operation, authenticated binding or preservation of immediate intervention access in fullscreen.

The principal carry-forward checks are video/aircraft association, the distinction between active vehicle and group selection, and fullscreen intervention access. Blank maps, overlapping mock positions and synthetic-image quality should be recorded as fixture limitations rather than attributed to the integration without further evidence. Existing baseline friction need not trigger unrelated redesign during slice 0.

**Neutral prompts for the slice-1 screenshot review**

1. “Which aircraft and companion, if any, are connected here? Point to the evidence.”
2. “Set up the companion at the provided address. Explain what tells you each step succeeded or failed.”
3. “Does this screen establish that the companion controls the intended aircraft? What remains uncertain?”
4. “Switch to the other aircraft. Identify which connection and sign-in state now belong to each aircraft.”
5. “The credentials were rejected or the session expired. Describe the next action you would take.”
6. “This companion reports a different aircraft from the one selected. Explain what is available and what you would do next.”
7. “Disable the integration, then inspect Fly View. Describe what changed and what flight tasks remain available.”

For each prompt, retain the reviewer’s first interpretation, evidence cited and unanswered questions before supplying explanatory context.
