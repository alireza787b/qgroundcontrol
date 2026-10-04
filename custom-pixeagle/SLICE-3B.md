# Slice 3b — installed Smart models and Full AI replay

Ready for operator acceptance, 2026-09-24. This checkpoint extends the completed
[Classic feedback checkpoint](SLICE-3-FEEDBACK.md). It does not supersede that
checkpoint's immutable manifests or imply hardware qualification.

## Scope

The native model picker lists installed, compatible models advertised by the
authenticated companion. It distinguishes a configured model from a running
Smart detector and shows the effective device. Class names are available under
**Model details**. Model installation, upload, deletion and trust administration
remain separate from Fly View.

The picker captures the destination client, target guard and model generation
when **Use model** is pressed. A changed destination, stale generation or closed
dialog invalidates that intent. Selection shares the existing action serializer
and never retries an uncertain mutation automatically. Model changes use
`models:read` and `models:select`, without granting model management privileges.

Opening model setup ends an unfinished video selection without stopping an
existing track. Choosing a model does not enable Smart mode. Active following
and a selected Smart target prevent model replacement. Shared dashboard changes
invalidate native target state through the same backend revision mechanism.

## Recorded validation

The separate Full AI environment uses PyTorch 2.12.1+cu130, Ultralytics 8.4.95,
OpenCV 4.13.0 and dlib. Both copied VisDrone models ran on CUDA without fallback.
Their existing trust receipts are **operator assertions**, not publisher digest
verification. All four optional Classic ONNX assets match their pinned hashes.
The original PixEagle main checkout remains clean.

| Gate | Result and limit |
| --- | --- |
| Final custom Debug and Release builds | Passed after all UI changes |
| Final custom `Unit\|Integration`, excluding `Flaky\|Network` | **411/411 passed**, 265.61 seconds; includes stock UI coverage |
| Stock Debug build and same test labels | Build passed; **405/406 tests passed**. Unmodified `QGCCameraManagerTest` encountered an unexpected mock `MAV_RESULT_UNSUPPORTED` warning; unchanged isolated rerun passed **1/1**. The full stock run is not reported as green. |
| Focused native Qt cases | Client 102, manager 4, model client 65, video item 65, video controller 5, target controller 70, pointer adapter 13; all passed (324 cases, overlapping the full suite) |
| Backend model/target/security/routes/config gate | **249 passed**; overlapping broader gate **372 passed, 1 baseline failure**: three pinned-base gimbal test doubles contain `sent.append(frame) or True`, rejected by the hygiene check |
| Backend Smart loss-status addendum | Combined gate **260 passed**; final target suite **35 passed**; overlapping counts are not a unique test total |
| Config/schema | 43 sections / 606 parameters validated without drift |
| Replay tooling | **21 passed** after backend freeze; snapshot hashes and isolated configuration validated |
| Actual six Classic trackers | Factory initialization/update/stop and all 19 native switch/select/Cancel mutations passed; KCF had one transient rejected update and VitTrack one lost sample |
| Actual Smart native API, both models | Each retained a spectator for 30/30 tracking samples over 6.82 / 6.69 seconds; distinct retarget, active-target model-change refusal, Cancel and Classic return passed |
| Actual QGC GUI | Keyboard model application, explicit Smart activation, point selection, quick retarget, model-change guard, Cancel, map/video swap and fullscreen passed; 30 target samples over 8.03 seconds were 23 tracking / 7 acquiring |
| UI layouts | Private 1440 × 900 and 1024 × 768 captures; target panel uses QGC tool insets to clear flight instruments |

The GUI did not continuously retain a fresh measurement in every sample. Crowded
`test2.mp4`, dusty `test9.mp4` and thermal `test11.mp4` probes also exposed loss
or identity churn. Raw outcomes are preserved; detector, association and loss
thresholds were not relaxed. This is recorded-source workflow evidence, not
general detector accuracy, identity retention or flight qualification. The
terminal Smart loss-state correction keeps an exhausted track visibly `lost`
until Cancel or another accepted transition; predicted/tentative states remain
stale and unusable for following.

Private GUI automation used separate Xvfb displays, never the shared desktop.
An initial coordinate-driver mistake and a missed candidate click are retained
separately from successful checks. Recording loops change the source epoch and
end selection mode; rearming **Retarget** is then required. No action is silently
resent after a source change or unknown outcome.

Evidence lives under
`~/.cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/`; the directory retains its
start date. See [the source/evidence manifest](slice-3b-manifest.json),
`runtime/qualification-final.json`, `runtime/QUALIFICATION.md`, and
`ui/qualification.json`. Backend checkpoints are
`docs/reporting/agent-ops/codex-modernization/checkpoints/2026-09-24-qgc-native-models.md`
and `2026-09-24-qgc-smart-loss-status.md` in the integration worktree.
The previous slice 3a archive and manifest remain unchanged.

Independent simulated operator feedback is preserved unchanged:
[initial](reviews/slice-3b-operator-initial-raw.md),
[follow-up](reviews/slice-3b-operator-followup-raw.md), and
[disposition](reviews/slice-3b-disposition.md). It is not human acceptance.
Remaining instruction size, status ambiguity and view-discovery feedback awaits
the operator's test; it does not delay this handoff with another polish cycle.

## Additional target actions reviewed

| Action | Slice 3b decision | Reason and later acceptance requirement |
| --- | --- | --- |
| Classic redetect | Defer native control | Existing action searches runtime imagery; it does not identify the displayed retained frame or the operator's intended target. Quick point/box retarget is already explicit. Add a guarded recovery workflow only after its search region and outcome are clear. |
| Tracker restart | Defer to diagnostics/settings | Restart can replace runtime state and is distinct from Cancel or retarget. It needs destination/revision protection, a clear tracking-loss warning and authoritative completion state before native exposure. |
| Segmentation | Defer to a separately qualified mode | Installed VisDrone models are detection models. Segmentation needs an advertised compatible model, selection geometry and mask semantics; a dashboard toggle alone does not establish native support. |

The existing `/api/v1/actions/` handlers explicitly reject these three actions
with a native context. QGC does not bypass that boundary using legacy routes.
Their absence is deliberate and is recorded in the
[operator coverage plan](OPERATOR-COVERAGE.md).

## Operator checkpoint: ready to test

The fresh demo uses `runtime/smart-user-demo`, loopback port **8096**, and bundled
`test10.mp4`. It starts in Classic/CSRT with VisDrone9m configured. The recording
contains race graphics and a split screen; those titles/timers are part of the
source. The repeatable Smart target is a **spectator by the fence near the
bottom-left of the left race view**, not the racecar. Passive class labels are
disabled in this private replay profile; detection boxes and selected/lost cues
remain enabled. No PX4, camera or real gimbal is needed.

1. In **Application Settings → PixEagle**, sign in using this new snapshot's
   private `credentials.json`, then choose **Open Fly View**. The aircraft's
   **Disconnected** banner is expected; the companion can operate independently.
2. Open **Tracking setup → Smart model…**, inspect VisDrone9m / VisDrone26m and
   **Model details**. Apply a different model with **Use model** if desired.
   Closing this dialog does not enable detection: select **Smart** in setup.
3. Wait for detections, press **Select target**, then click a detected spectator.
   Click another detected person to retarget while selection is armed.
   **Finish selecting** leaves selection mode and keeps the current track;
   **Retarget** reopens it, including after a recording loop disarms selection.
4. Try **Cancel tracking**, then change to the other model and repeat. A model
   cannot be changed while a Smart target is selected. Return to **Classic** to
   exercise the six advertised trackers with a point or drawn rectangle.
5. Try map/video swap via the lower-left inset and video fullscreen. Report the
   model/tracker, approximate time, action, expected result and observed result.
   Saved logs will be reviewed against that feedback before further changes.

Launch or relaunch from this QGC checkout (close the existing demo first):

```bash
export PIXEAGLE_DESKTOP_DEMO_DIR=/home/alireza/.cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/desktop-smart-demo
custom-pixeagle/validation/run-replay-desktop.sh \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/runtime/smart-user-demo
```

The launcher uses the snapshot's Full AI interpreter, disables automatic aircraft
connections in isolated QGC settings, verifies companion identity and starts
the Release app. Closing QGC stops this backend. Private `qgc.log` and
`pixeagle.log` are saved in `desktop-smart-demo`; credentials are not included
in the source archive or evidence manifest. Logs are recorded automatically;
live background analysis is not implied while the agent is idle.

Slice 3b awaits this human feedback. Physical external-camera target acceptance
also remains open; mock support does not qualify the actual camera.

Slice 4 retains guarded following controls and capability-driven gimbal motion.
External-camera tracker qualification needs the actual supported camera, matched
video source and an explicit hardware test session. Slice 5 retains platform
release validation, documentation and upstream preparation.
