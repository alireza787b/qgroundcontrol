# Slice 3 — target operations

Status: the Classic demo feedback is addressed at the
[slice 3a checkpoint](SLICE-3-FEEDBACK.md). Slice 3 remains open for the installed
Smart model workflow and target-mode runtime validation in
[slice 3b](OPERATOR-COVERAGE.md). Slice 4 has not started. The completed slice 2
checkpoint and original early-demo manifest remain historical records.

This slice adds native Classic point/rectangle selection, Smart selection,
advertised external-camera target selection, tracker choice, retargeting, and
tracking-only Cancel. Following and gimbal movement remain outside this slice.

The existing QGC video gesture owner captures the companion client, target-state
guard, displayed frame, and image geometry at press time. The backend validates
these against retained original analysis pixels and authoritative target state.
Expired frames and changed sessions, sources, modes, revisions, or aircraft
bindings fail explicitly. Mutations are not retried automatically.

Validation includes focused client, surface, and shared-gesture tests; stock
and custom Linux builds and regression suites; backend route/config/schema and
target-operation gates; isolated recorded-video screenshots and raw simulated
operator reviews. Results and any limitations will be recorded before a manual
tracker demo is handed to the user. That demo uses the bundled recording without
PX4; it does not establish aircraft or hardware qualification.

## Implementation checkpoint

- `PixEagleTargetController` exposes native selection and authoritative status.
  The existing Fly View gesture dispatcher chooses one owner at press time;
  an unarmed native view consumes targeting gestures. Fullscreen double-clicks
  cancel pending single clicks. Panels consume background clicks.
- `PixEagleVideoItem` pins the actual swapped image and its inverse transform,
  including rotation, mirror, letterboxing, layout, and device pixel ratio.
  A separate `selection_geometry` envelope preserves provenance v1 semantics.
- `PixEagleClient` captures session/context generation and backend target revision,
  allows one mutation at a time, and refreshes authoritative state after uncertain
  outcomes. It never automatically retries a target mutation.
- Tracking-only Cancel requires current target state but no fresh video frame.
  All native target mutations reject active following. Companion-only writes
  require both backend aircraft connections to be disconnected.
- Tracker and mode choices follow backend availability. The prepared Core replay
  supports Classic tracking; Smart AI requires its model/runtime, and external
  selection requires a supported configured camera and matching video source.

## Original early-demo evidence

Evidence root: `~/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21`.

| Check | Recorded result |
| --- | --- |
| Full custom Unit/Integration, excluding Flaky/Network | 409/409 passed before final startup/catalog corrections; `custom-full-1*` |
| Full stock with the same exclusions | 406/406 passed; `stock-full-1*` |
| Linux stock/custom Debug and Release | Built successfully; final handoff rebuild logs recorded separately |
| Client auth, guards, envelopes, outcomes | 102 Qt cases passed; `client-surface-1*` |
| Presented pixels, geometry, expiry, cancellation | 65 Qt cases passed; same run |
| Threaded OpenGL with device scale 2 | 65 passed, zero skipped; `surface/threaded-highdpi.log` |
| Target orchestration and held control ownership | Passed in full custom run; five catalog alias cases added afterward |
| Shared native/stock gesture adapter | 10 Qt cases passed; focused and full runs |
| Backend broad gate before catalog correction | 1224 passed; unchanged schema 43 sections/606 parameters |
| Backend catalog correction | 51 focused plus 59 route/candidate tests passed; fatal lint/static passed |
| Production backend without PX4 | Seven point/box/retarget/Cancel/shared-revision operations passed on clean-v2 |
| Clean video ACKs across a file loop | 820 JPEG pairs: 819 fresh, 1 truthfully cached; clean-v3 |
| Actual Release GUI, clean-v3 | Point, box, Cancel, KCF switch, fullscreen, PiP and Plan return; `handoff-ui` |
| Final startup and target-controller regression | 46 Qt cases passed, zero skipped; `handoff-focused*` |
| Final corrected Release and clean-v4 | Persisted-enabled startup, three-choice catalog, point, Cancel and KCF switch; `handoff-final-ui` |
| Final changed-source hooks | Passed; `handoff-lint.log` |

The Release GUI exposed a persisted-enabled startup race: the view loaded before
its controller was published. Publishing the controller before activating its
view fixes this. Fresh Release startup with saved preferences showed live video
without toggling video off/on (`handoff-ui/ui/03-persisted-video-live.png`).

The startup regression fixture first failed because SettingsFact ignores saved
preferences in unit tests, then because two fixture controllers competed for the
singleton video manager. Its corrected fixture uses the actual plugin manager;
the final focused rerun is recorded separately. These fixture failures are not
silently omitted from the evidence.

The GUI also exposed inconsistent availability across tracker aliases and a
CSRT duplicate. Both backend catalog families now share canonical identities
and availability; QGC deduplicates by factory identity while preserving the
server request alias. Missing model artifacts/libraries are unavailable.
`clean-demo-v4` contains this correction; old snapshots remain unchanged.

Final source hooks and focused checks are recorded separately from the earlier
broad suites. Previous upstream lint, dashboard CI and MAVSDK teardown qualifiers
from slices 0–2 remain; this is not a claim of green remote CI.

## Human tracker test

Use the immutable operator replay at
`~/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21/clean-demo-v4`.
Its private `credentials.json` supplies the login. The clean bundled `test1.mp4`
has scene changes and title cards which can legitimately lose a target. Optional
demo OSD is off; actual tracker feedback remains visible.

```bash
PIXEAGLE_DESKTOP_DEMO_DIR="$HOME/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21/desktop-feedback-demo" \
  custom-pixeagle/validation/run-replay-desktop.sh \
  "$HOME/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21/clean-demo-v4"
```

1. Open Application Settings → PixEagle, sign in, and choose Open Fly View.
2. Select target, then click a textured target or drag a tight rectangle around it.
3. After an accepted selection, click or draw again to replace the target directly.
   Finish selecting leaves selection mode while preserving tracking; Cancel
   tracking stops it. Compare image markings with status text.
4. In Tracking setup, try CSRT, KCF + Kalman and Sparse Flow. Choices apply
   immediately; select a new target after changing tracker.
5. Try fullscreen, map/video swap and resizing. Report anything unclear or noisy.

The stock Disconnected toolbar refers to the absent aircraft; PixEagle's own
Video active status refers to the companion. PX4 is not required. Following and
aircraft movement are unavailable. Smart AI and external camera controls need
additional runtime/hardware; mocked contract checks do not qualify them.

Closing QGC stops the backend started by the launcher. Logs are saved to
`desktop-feedback-demo/qgc.log` and `desktop-feedback-demo/pixeagle.log`. Report approximate time,
tracker, action, expected result and actual result. Keep raw observations separate
from diagnosis. The agent can inspect saved logs on resumption; no claim is made
of continuous agent monitoring while paused.

## Feedback and remaining work

Independent screenshot responses are preserved in `reviews/slice-3-operator-*-raw.md`.
[The disposition](reviews/slice-3-feedback-disposition.md) records fixes and open
clarity/occlusion questions. These are simulated reviews, not human validation.

The [feedback checkpoint](SLICE-3-FEEDBACK.md) records human observations, log
correlation, fixes and subsequent regressions. The [coverage plan](OPERATOR-COVERAGE.md)
adds installed Smart model choice and target-mode completion before slice 4.
Windows/Android release checks, real Smart runtime, external-camera
hardware, SITL, flight qualification, provisioning and deployment remain outside
this local demo checkpoint.
