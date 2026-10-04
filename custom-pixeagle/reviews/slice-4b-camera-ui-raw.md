# Slice 4b camera UI — raw AI operator review

Date: 2026-09-30. Reviewer: Codex agent acting as an operator/UX reviewer.
These are AI review observations, not interviews or human operator acceptance.
The reviewed Release replay used a mock camera provider and synthetic video;
no physical camera, gimbal, or aircraft was connected.

## Evidence inspected

Screenshots under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b-2026-09-29/ui/`:

- `30-camera-final-fly.png`
- `31-camera-final-options.png`
- `32-camera-final-controls.png`
- `34-camera-final-release.png`

The fresh application log is
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b-2026-09-30/camera-ui/qgc-final.log`.
Inspection found no `Binding loop`, `TypeError`, or `ReferenceError` entries.
The fixture audit at `/tmp/pixeagle-native-fixture-g53tirw_/security-audit.jsonl`
records successful `pan` and `stop` operations at 03:42:46 UTC. This establishes
mock command execution, not measured hardware motion or physical stopping.

## Raw observations

- The compact panel is identifiable as PixEagle. Classic, the active tracker,
  and No target are readable without opening a menu. The separate toolbar
  entry does not impersonate aircraft flight mode.
- Options no longer shows an empty Smart model selector or an indefinite
  model-loading message for this camera-only fixture. The remaining Smart
  unavailable explanation is relevant and confined to the detailed view.
- Settings, Dashboard, and Camera controls have clear destinations. I can
  locate manual camera control without changing the displayed local tracker;
  camera movement and camera-owned tracking are not presented as synonyms.
- The camera popup has consistent axis rows and readable direction buttons.
  “Each press moves one step. Release to stop sooner.” clearly limits the
  expected behavior; this is not a continuous joystick or repeated hold.
- The popup obscures the central video area where I would look for the result
  of a movement. The underlying Options dialog also peeks around its edges.
  Record both observations for the camera bench checkpoint; they do not justify
  another cosmetic redesign before real operator feedback.
- In the release screenshot, Right remains highlighted while Stop camera is
  disabled. A highlight may be focus, not motion. Do not use button color as
  evidence that movement continues or has stopped; ask the bench operator
  whether the focus indication is confusing during actual use.
- The global QGC Disconnected message remains visible with no aircraft in this
  fixture. The PixEagle panel is still present. This correctly preserves QGC's
  independent aircraft state, but onboarding should continue to explain that
  companion connectivity and aircraft connectivity are separate.

## Review boundary and next checkpoint

No layout clipping or overlapping control text was visible at the captured
1440 × 900 desktop size. Touch sizing, small windows, physical axis signs,
movement distance, latency, camera-owned tracking, and network-loss hardware
behavior remain unqualified by these screenshots. The audit does not replace
owner/staleness tests, and a successful mock Stop does not establish a hardware
watchdog guarantee.

The two demonstrated UI defects were addressed before this replay: camera
release/cancellation queues Stop with its captured original owner to avoid
synchronous binding recursion, and unavailable empty Smart model controls are
hidden. No further cosmetic edits are requested from this review. Carry the
modal overlap and focus-highlight observations into the Ethernet camera bench
feedback, then decide from that evidence.

## Subsequent operator decision

The user rejected the modal axis-button design after seeing the screenshot and
requested a nonintrusive native QGC-style joystick. The preceding AI assessment
is preserved as raw feedback, not operator approval. Its suggestion to defer
the obstruction concern was superseded by this explicit feedback. The next
revision uses the QGC thumb-pad visual in a movable panel and must receive a
fresh input/screenshot review before hardware handoff.
