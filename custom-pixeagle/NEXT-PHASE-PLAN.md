# PixEagle–QGroundControl next-phase plan

**Status:** Approved historical 4b baseline; current checkpoint is 4b.4d software closeout — 2026-10-04.
**Current sequence:** Retain the physical camera/failure retest for the user's return, finish the remaining radio/Pi/process-loss evidence, then platform releases/reviewed PRs (5) and onboard command-blocked ground testing (6). While the camera is unavailable, advance the [camera-free qualification](SLICE-4B-4D.md), measured-attitude protection and package preparation independently. The ordinary and gimbal-lab deployment choices are consolidated in [DEPLOYMENT-PROFILES.md](DEPLOYMENT-PROFILES.md). Sensorless fixed-wing guidance requires an explicit qualified policy; silently substituting ground/cruise speed does not close that gate.

The camera-v5/v6 workflow and vertical bench observations supersede the hardware-next status below. The original design rationale is retained; [STATUS.md](STATUS.md) is the current position and acceptance record.

## Decision summary

The accepted slice-4a checkpoint establishes a useful normal-mode workflow: PixEagle is optional, Classic and Smart targeting are separate from QGC flight controls, and following is an explicit PixEagle action bound to a verified vehicle. The next phase should not turn the Fly View into a second dashboard or make camera/gimbal behavior look like aircraft flight mode.

The design should keep four identities separate:

1. **Video source** — the stream currently displayed and the frame identity used for selection.
2. **Target engine** — local Classic, local Smart, or a camera-owned tracker advertised by the connected camera.
3. **Camera/gimbal control** — manual or camera-owned movement capabilities and their safety state.
4. **Aircraft follower** — a PixEagle follower that publishes commands for one verified aircraft.

These can be shown together in one compact surface, but they must not share an ambiguous “mode” or a single enabled flag. A camera may have its own Classic/Smart terminology; that is a camera capability and must not be confused with PixEagle’s local algorithms.

The recommended next sequence is 4b.0 through 4b.4. The Ethernet camera is connected only at 4b.3, after the mock and protocol gates pass. The user approved this sequence; 4b.0 is operator accepted and 4b.1–4b.2 are implemented. Hardware starts at 4b.3.

The implementation is allowed to change configuration structure and PixEagle internals when that removes ambiguity, duplicated state, unsafe ownership, or a legacy path that cannot satisfy the native contract. We will preserve an old route or field only when an external dashboard/client still needs it or a migration requires it; each retained compatibility path must have one authoritative owner, an explicit translation boundary, tests, and a removal condition. New QGC code will not expose redundant legacy names merely to avoid a deliberate cleanup.

## Evidence boundary

The user accepted the local Classic/Smart and normal-following workflow after the slice-4a desktop checkpoint. The saved target-path audit contains Classic tracking starts/stops, a tracker switch, and native-follow start/stop. It contains no Smart action in that particular run. The separate test9 fixture has the installed Smart models and remains the correct next Smart demonstration. Existing logs show operation, not continuous detection accuracy, closed-loop aircraft following, camera-axis accuracy, or field safety.

The prior hardware notes concern a Topotek SIP/UDP Ethernet controller paired with RTSP. Earlier dashboard hardware work was partial: pan behavior was observed, while rectangle retention, camera-owned Smart identity, and the proposed rotated installation remain unqualified. Local Smart and camera-owned Smart must remain separate in all evidence and labels.

## Operator workflow and placement

The compact Fly View surface remains the primary operational surface. It should contain only:

- a recognizable **PixEagle** label or wordmark;
- one target activity token with a fresh/stale/error state;
- one aircraft-following token with a fresh/stale/error state;
- the active target engine and follower names when available;
- the primary action appropriate to the state: Select/Retarget, hold-to-Start, or Stop;
- an Options button for configuration and capability details.

The tokens are status indicators, not a replacement for QGC’s flight-mode display. They must not claim “flight mode”, “armed”, or “safe” based on PixEagle state. Following remains visibly distinct from tracking. An unavailable capability is hidden from the compact operation surface and explained in Options, rather than occupying space as a dead icon.

The toolbar entry should use a PixEagle identity and independent target/follow indicators. A green camera dot is insufficient because it reports video rather than PixEagle activity. The toolbar can show concise states such as `PixEagle · Tracking`, `PixEagle · Following`, `PixEagle · Ready`, or `PixEagle · Attention`, with details in its existing QGC drawer. Freshness, source, active vehicle, and the reason for an unavailable action belong in the drawer or Options dialog.

The Options dialog should be a small, capability-driven configuration surface:

| Area | User-facing choices | Owner |
| --- | --- | --- |
| Target engine | PixEagle Classic or PixEagle Smart | Native QGC, values from backend catalog |
| Classic engine | Available Classic tracker catalog | Native QGC, backend capability contract |
| Smart engine | Installed compatible model catalog, with device/status disclosure | Native QGC, backend model inventory |
| Follower | Compatible aircraft follower/profile | Native QGC, backend readiness contract |
| Camera/gimbal | Only when a camera provider is present: camera source, camera-owned modes, manual controls | Native QGC, backend capability contract |
| Links | PixEagle Settings, Dashboard, setup/help, About/source/license | Native QGC; browser link uses configured endpoint |

The detailed backend administration remains in the dashboard. QGC should not expose arbitrary pipeline strings, model file paths, account administration, or a large checkbox list in Fly View. Settings should use grouped sections and progressive disclosure: Connection, Video and targeting, Camera and gimbal, and Advanced. Checkboxes are reserved for true preferences such as optional OSD visibility; stateful actions use buttons, choices, or explicit apply controls.

The default is PixEagle disabled. Help and About remain available without enabling it. The default endpoint uses the local backend host and port 5077; the dashboard shortcut uses the saved complete dashboard URL or the documented default frontend port 3040. QGC must not infer a frontend URL from CORS origins. A user override may include host, port, and proxy path.

Sign-in, backend connection, and vehicle verification are separate states. Companion-only video and permitted targeting can work without PX4; aircraft following requires a verified aircraft. A user who has not configured PixEagle should see stock QGC with no new operational friction.

## State and concurrency contract

Every native snapshot should identify the backend/client generation, video-source identity, target state, camera capability state, and per-vehicle following state. Each state has its own freshness and error timestamp. `Unknown`, `Stale`, `Unavailable`, and `Stopped` are different values; a command acknowledgement is not proof that the observed state changed.

Actions capture the appropriate owner and generation at the start:

- target actions bind to the displayed frame/source and target session;
- following actions bind to the verified vehicle and follower session;
- camera movement binds to the original camera/provider endpoint;
- Stop remains available for the captured owner and does not require a fresh video frame.

Switching the active QGC vehicle, video source, target engine, camera mode, or dashboard client must not transfer an active follow or movement lease to a different owner. Vehicle switching should show background status and provide a safe destination-specific Stop. Focus loss, release, disconnect, source change, stale capability, and mode change terminate a bounded camera hold. The UI must refresh from authoritative state after retries and must never convert an uncertain result into a new command automatically.

## Configuration and restart behavior

The native settings page needs a clear three-state presentation: **saved**, **running**, and **restart pending**. The backend already classifies relevant settings:

- OSD enablement is intended to be immediate;
- tracker/model/provider changes require a tracker restart or controlled runtime swap;
- video-source and pipeline changes require a system restart;
- changes while following or Offboard are guarded.

The native UI should show the reload tier returned by the backend, offer an explicit Apply action, and request confirmation before a system restart. It must report when the service has reconnected and whether the requested value became effective. It must not silently kill a backend process or claim that a supervisor restarted it. After a restart, pending model/target selections are cleared and following is never resumed automatically.

The dashboard and QGC must use one authoritative configuration owner. The QGC API additions belong under versioned `/api/v1/` routes with typed request/response models, permissions, CSRF protection where applicable, generation or CAS checks, idempotency keys, audit records, and rollback on failed runtime application. The existing legacy route names should not be exposed as “restart follower” when they actually restart a tracker.

## OSD scope

A small OSD control can be included in 4b.1, but it must be an explicit desired-state operation (`enabled: true/false`), not a blind toggle. It should report saved/effective state, permissions, generation conflicts, and whether the change affects the backend overlay, QGC-native status, or the recorded stream. It must not hide PixEagle status or burn native QGC state into the video without an explicit user choice.

The current legacy OSD toggle is immediate but not authoritative/persistent enough for a native control. Therefore the first OSD work is a shared typed contract and a bounded enable/disable control. Presets, colors, recording inclusion, media administration, and overlay composition remain a later media slice.

## Camera and gimbal architecture

The provider contract must advertise capabilities instead of making QGC infer behavior from a vendor IP or a fixed list of axes. The first implementation may retain the Topotek host match as a safety check, but it should add an abstract source/provider identity, a selection-mode catalog, readiness state, supported axes, limits, and camera/video association. The backend owns vendor protocol details; QGC renders the advertised result.

Camera-owned Classic/Smart is represented as a camera capability and labelled accordingly. It is not inserted into the local PixEagle tracker catalog. A provider may expose camera-owned target selection, camera-owned tracking, manual pan/tilt/roll/zoom, presets, and Stop independently. If a camera has no control or no matching full-frame video, the related option is omitted or explained as unavailable.

Manual movement is bounded and cancellable. A tap sends one bounded step; a hold repeats serially with a maximum duration and rate. Release, focus loss, disconnect, source change, active-vehicle change, and capability loss end the hold. A distinct emergency Stop must not depend on fresh imagery. The implementation must not promise that a network pulse equals an exact physical angle until the real device is measured.

The native camera target contract needs retained-frame/source and orientation guards, including letterbox/crop and rotation metadata. A click from an unrelated QGC RTSP display must not be treated as a qualified PixEagle frame. In-camera Smart selection is a provider feature; it is not evidence that a local AI model ran.

## Phased implementation and gates

### 4b.0 — workflow freeze and Smart evidence (no hardware)

- Correct the stale help text that says following is unavailable.
- Finalize the compact PixEagle indicator, Options grouping, and state vocabulary.
- Run test9 with both installed Smart models, Smart target selection, retarget, loss/recovery, Cancel, and model switch. Record model/device/provenance and distinguish automated evidence from human coverage.
- Use the existing native model `available` and `unavailable_reason` fields: availability already includes verified Smart task compatibility and runtime support. Do not add a redundant flag or infer compatibility from a filename.
- Add a deterministic operator-path fixture/report with normalized coordinates, frame timestamps, drops, selection latency, and tracker loss/recovery. Do not infer aircraft-following accuracy from an open-loop video.
- Verify default-off, companion-only behavior, no-PX4 behavior, permissions, two clients, stale source, and active-vehicle switching.

**Gate:** user can approve the normal and Smart workflow without hardware; unavailable and stale states are understandable.

### 4b.1 — native configuration, apply/restart, and bounded OSD

- Add typed capability/config snapshots and saved/running/pending-restart presentation.
- Implement controlled model/tracker/provider apply and explicit system-restart workflow with flight/Offboard guards.
- Add the narrow OSD desired-state contract and settings control.
- Make dashboard and QGC refresh from the same authoritative generations.

**Gate:** settings changes are predictable, auditable, reversible on failure, and never silently resume following.

### 4b.2 — camera capability and mock controls (no hardware)

- Add source/provider identity, selection-mode catalog, readiness, orientation, and camera-owned mode contracts.
- Implement mock camera selection, camera tracking mode, bounded movement, release/Stop, late replies, disconnects, and concurrent-client ownership.
- Keep local Classic/Smart and camera-owned modes visibly separate.

**Gate:** old/no-camera providers remain unaffected; unsupported controls are absent; safety and owner guards pass.

### 4b.3 — Ethernet/RTSP bench checkpoint

This is the first point at which the user should connect the known Ethernet camera/gimbal. Before connection, record the model, firmware, network topology, RTSP URL shape without credentials, mount orientation, supported control protocol, and safe bench limits. Test video identity, latency, frame pacing, orientation, pan/tilt/roll/zoom/home/Stop capabilities, camera-owned mode catalog, rectangle retention, and disconnect recovery with props and aircraft motion disabled.

**Gate:** no flight is authorized by this project. The bench report must state which axes and modes were actually observed, which remain unsupported, and whether the video used for selection is the same frame source used by the provider.

### 4b.4 — compatible camera follower and SIH integration

- Add a camera-follower profile only when backend capability negotiation proves compatibility.
- Exercise local Classic, local Smart, camera-owned tracking, and aircraft following as distinct combinations.
- Run PX4 SIH for command ownership, vehicle switching, target loss, Stop, and stock QGC flight controls. Keep camera motion open-loop unless a measured closed-loop interface exists.

**Gate:** normal local mode remains the default and works without a camera; camera mode cannot hijack normal follower or QGC flight control.

### 5 — release and upstream preparation

Retain Linux regression, then build/test Windows and Android, verify touch and resize behavior, document onboarding and recovery, review extension boundaries against pinned QGC architecture, and prepare focused reviewable commits/PR material. SITL/SIH and bench evidence do not replace aircraft, payload, regulatory, or deployment qualification.
The customized QGC branch remains private until those gates and a separate
publication decision pass; see [publication status](PUBLICATION-STATUS.md).

## Test matrix and failure policy

Each checkpoint must cover: no backend, wrong credentials, expired session, companion-only, no PX4, multiple vehicles, active-vehicle switch, two native clients, dashboard/native concurrent edits, stale source, model unavailable, permission denied, tracker loss, late response, retry with unknown outcome, disconnect, and restart while a follow or camera hold is active.

Smart reports must include both installed models and identify device/fallback. Classic reports must use the advertised catalog rather than a hardcoded three-item list. Video reports must include frame pacing and real-time behavior. Camera reports must include measured capability and orientation; no “all axes work” conclusion is valid from a single command acknowledgement.

UI review is performed once per checkpoint by the operator-focused review group against the QGC interaction pattern and the actual workflow. Cosmetic iterations outside a demonstrated usability or safety issue are deferred, preserving token and review efficiency.

## Deferred scope

Native recording and storage administration, model upload/delete/trust, account administration, arbitrary pipeline editing, detailed OSD/media composition, diagnostics export, provisioning, deployment, and real aircraft qualification remain later work. They should not be smuggled into the first camera slice through a generic settings editor.

## Approval gate

This document is the proposed next-phase baseline. The user has authorized architectural/configuration cleanup when it improves clarity and future compatibility, subject to the evidence gates above. The user approved this baseline. Implementation starts at 4b.0 with the test9 Smart checkpoint and the compact workflow/state contract. The Ethernet camera is not needed until 4b.3; the user will be asked for the exact bench connection details at that gate.

## Reference guidance

- [QGroundControl Fly View toolbar](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/fly_view/fly_view_toolbar.html) — status and detail patterns; PixEagle must not impersonate flight mode.
- [Auterion camera and gimbal controls](https://docs.auterion.com/vehicle-operation/auterion-mission-control/ui-breakdown/fly/camera-and-gimbal-controls) — contextual camera controls when a camera is connected.
- [Auterion quick-actions sidebar](https://docs.auterion.com/vehicle-operation/auterion-mission-control/ui-breakdown/fly/quick-actions-sidebar) — hide unavailable actions and confirm consequential actions.
- [Auterion Visual Tracking API](https://docs.auterion.com/app-development/auterion-sdk/visual-tracking-api) — separates image selection, tracking service, and vehicle integration.
- [MAVLink Gimbal v2](https://mavlink.io/en/services/gimbal_v2.html) — capability and control ownership principles; the current Topotek provider is not assumed to be MAVLink.
- [PX4 Offboard mode](https://docs.px4.io/main/en/flight_modes/offboard) — QGC must not become a second Offboard setpoint publisher.
- [DJI Zenmuse H30 Series User Manual](https://dl.djicdn.com/downloads/zenmuse_h30_series/20240625/Zenmuse_H30_Series_User_Manual_v1.0_en.pdf) — camera-view, mode, gimbal, and preflight controls are contextual to the connected payload; this is design inspiration, not an API specification.

## Sensorless fixed-wing qualification requirement

The operator requests airspeed-less fixed-wing support. First qualify a typed
PX4 measured or validated synthetic speed observation with source/freshness and
wind validity. Separately evaluate an explicit restricted ground-speed policy,
with airframe-specific trim, turn/climb and wind assumptions; never use ground
speed as proof of stall margin. Keep the ordinary recorded-video/CSRT/Position
baseline unchanged. The opt-in `FW_ATTITUDE_RATE.ALLOW_GROUND_SPEED_FALLBACK` setting is
implemented separately from flight qualification; it stays false by default. See [configuration scenarios](CAMERA-SCENARIOS.md).
