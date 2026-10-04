# Slice 4b.1–4b.2 — settings and independent camera controls

Date: 2026-09-30. Status: software/mock gate passed; ready for the 4b.3 Ethernet camera bench.
The user accepted 4b.0 and authorized continuing to the physical-camera gate.
No physical camera or aircraft has been connected in these checks.

## Operator changes

- PixEagle Settings has a backend OSD switch and a collapsible Backend settings
  section. OSD affects PixEagle's video overlay for all viewers; it does not
  remove native QGC instruments or text already present in a recording.
- Saved and running settings are read from the backend. Pending settings show
  their required apply/restart tier. Apply is explicit and confirms the captured
  backend generation; dashboard edits or a restarted backend invalidate it.
- Installed Smart models retain their existing model-selection workflow.
  Broader tracker apply is blocked while Smart or tracking is active, while
  following/Offboard or camera movement is active, or unless an observed aircraft
  is freshly disarmed. No following is resumed after apply.
- The Camera controls entry is contextual to an explicitly configured provider.
  Only advertised axes/actions are shown. The initial modal button grid was
  rejected by the operator and replaced with QGC's thumb-pad visual in a small
  movable panel. Serial acknowledged steps stop on neutral/release, close,
  focus loss, stale owner/state or error. A five-second gesture limit requires
  release. Zoom is adjacent; roll is in a disclosure. This is single-axis
  movement at provider defaults, not simultaneous analog-rate control. Target
  taps are suspended until the camera panel closes.
- Local PixEagle tracking and camera-owned tracking have separate engine choices.
  Camera mode labels and point/rectangle support come from the provider. Camera
  Smart does not expose the local model dropdown. Switching back to PixEagle
  selects an available local Classic tracker; the operator can then choose a
  different Classic algorithm or Smart. Runtime selections do not rewrite the
  saved default tracker.

## Backend ownership and compatibility

One application-owned camera runtime now owns the provider/transport. The
Gimbal tracker borrows it for angle samples, and manual controls use the same
owner while a local tracker is active. This avoids duplicate UDP listeners.
Dashboard and QGC commands share ownership arbitration. Stop invalidates queued
native movement generations, including movement arriving after a release.
Source and provider changes are rechecked before transmission.

Existing configuration keys remain unchanged: there is one provider configuration
under `GimbalTracker`. Normal local tracking with `CONTROL_ENABLED: false` does
not start a camera listener merely because `ENABLED` appears in the defaults.
Provider/transport changes require a backend restart because the camera owner
captures them at startup. The [scenario guide](CAMERA-SCENARIOS.md) gives exact
RTSP/local, camera-owned, and camera-application configurations.

The legacy dashboard OSD toggle now delegates to the same persistent transaction
as native desired-state OSD. Its existing `control:write` permission is preserved;
native OSD also requires `config:read` for the snapshot/generation. General Apply
requires `config:write`. No broader operator-role privilege was added. This is a
behavior change: dashboard OSD enablement now persists instead of being only a
runtime toggle. Failures roll back persisted, runtime and renderer state.

## Explicit restart boundary

The existing backend restart executor exits with code 42. That does not prove
that a supervisor will restart it. Native QGC therefore reports system restart
as unavailable until that ownership contract exists; use the local launcher or
service procedure. It does not send a remote kill or claim restart success.
Cached OSD styling and other unverified immediate settings are conservatively
shown as needing a system restart. Only verified OSD enablement is immediate;
tracker-tier apply uses the existing guarded tracker lifecycle.

## Validation

- Debug and Release builds completed using the pinned container/toolchain.
- New config and camera C++ tests cover permissions, stale confirmations,
  conflicting generations, runtime changes, malformed/oversized replies,
  redirects, expiry, captured-owner Stop and endpoint replacement.
- The target-controller tests cover provider-defined camera modes,
  unsupported rectangles, and switching back to a local engine.
- Backend config tests cover CAS, actor/key idempotency, persistence/renderer
  rollback, dashboard/native authority and apply guards.
- Backend camera tests cover independent provider ownership, late movement
  after Stop, source/owner freshness, concurrent clients and bounded movement.
- Dashboard regression and production build passed; details and exact evidence
  remain in the backend checkpoint and logs.

The first combined Linux run passed **413/413** in 299.94 seconds. A later
four-worker run passed 410/413: the long model-selection test returned unknown,
and LinkConfiguration/FindModule download-helper timed out. The three unchanged
isolated rechecks passed in 24.96 seconds. Preserve both runs; do not describe
the later run as clean. The joystick revision requires a new final check.

The real QGC settings smoke first exposed an outdated copied demo backend, then
a missing required audit argument. The launcher now refreshes stopped demo
source and schema together while preserving private settings/media/models, and
the audit helper is fixed with real-handler HTTP/audit regression coverage.
Live native OSD off/on now produces successful audit entries and matching saved
and running state. The final backend config, route, security, reload and tool
inventory gate passed **136 tests** in 72.88 seconds (one existing Starlette
deprecation warning). The original enabled state was restored. Evidence is under
`slice-4b-2026-09-30/config-ui-audit-fix` in the baseline cache.

The joystick Debug and Release builds and focused input/transport tests passed.
Actual QML tests cover initial neutral axes, centered press followed by drag,
repeated steps, release/recenter, Home followed by another gesture, refusal
requiring release, Stop retry, and panel dragging. The private Release replay
recorded ten pan pulses during a three-second hold followed by Stop, plus Stop
at the five-second gesture cap. The final first-open capture shows the thumb
centered. No binding-loop, reference or type errors appeared in the final UI log.
These are synthetic-provider results, not physical motor evidence.

Independent command review also found that a provider pulse ignored a failed
final Stop transmission. That now returns failure, preventing continued native
steps on false success; explicit Stop remains retryable. The backend camera,
route, reload and tool-inventory regression gate passed **264 tests** in 17.53
seconds. Schema (606 parameters) and generated API inventory checks passed.

The [raw joystick review](reviews/slice-4b-joystick-ui-raw.md) preserves remaining
questions about analog/diagonal expectations, touch discoverability and the
transition back to targeting. The prior modal review and user rejection remain
in their original record. Final Linux Unit/Integration run: **413/413 passed in 465.95 seconds**, two
workers, with the standard `Flaky|Network` exclusions. Debug and Release builds
and scoped code/document lint passed. Source identities, artifact checksums and
prior failed/superseded runs are recorded in
[slice-4b-1-2-manifest.json](slice-4b-1-2-manifest.json). Private demo services are
stopped; the user's shared desktop was not automated. Failed or superseded development runs remain distinct from
final results. No whole-repository lint, Windows/Android or physical qualification
is inferred from these local tests.

## Remaining gate

The [4b.3 operator steps](OPERATOR-TEST-4B-3.md) cover the Ethernet/RTSP bench: identify the actual model/firmware, camera IP,
RTSP path, mount orientation and safe movement limits. Confirm live source
identity, axis directions, Stop, camera-owned modes, target retention and
recovery. Existing Topotek dashboard evidence was partial; it is not substituted
for the new native workflow. Aircraft following with camera-angle targets remains
4b.4 SIH and compatibility work. Windows/Android and release/PR preparation remain
slice 5.
