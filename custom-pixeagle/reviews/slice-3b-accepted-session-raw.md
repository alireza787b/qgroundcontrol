# Accepted slice 3b session: independent read-only review

Reviewed 2026-09-26. The user explicitly reports everything tested is working as expected.
This is acceptance of their observed desktop workflow, not a claim that every planned
scenario or target platform was tested by them. No desktop inspection, input,
process control, credentials, or product edits were used for this review.

## Saved session evidence

The desktop backend run `native_replay_5ede6d114ffa7695` spans **2026-09-24
04:38:16.651–04:47:40.007 UTC** (08:08–08:17 Tehran). Its runtime JSON log
contains 1,192 entries: 1,188 INFO and four WARNING; no ERROR or CRITICAL. It
ends with signal 15 and completed graceful shutdown. The audit file also includes
an earlier private automated run; counts below filter to this desktop window.
They are accepted/rejected commands, not unique test cases or an inferred test script.

| Authoritative native action | Observed desktop outcome |
| --- | --- |
| Classic tracking-start | 4 success records, HTTP 202 |
| Smart target click | 13 success records, HTTP 202; 3 failure records |
| Smart mode toggle | 5 success records, HTTP 202 |
| Cancel tracking | 6 success records, HTTP 202 |

The three Smart failures align with two **no object matched** messages and one
**multiple nearby objects; selection held** message. Subsequent accepted selections
appear in the log; there is no evidence here that a failed click was automatically
replayed. Successful Smart selections name car/van detections and retained displayed
frames. The running model was **VisDrone26m on CUDA**, loaded on three Smart entries.
This desktop window contains no native model-select or tracker-switch audit, so it
cannot independently establish that the user switched models or exercised all six
Classic trackers. Earlier automated evidence remains separately available.

There are four invalid-credential login denials and then successful login records.
They do not establish an authentication implementation defect. The preceding chat
records a delayed handoff of the new credential file while older credential files
were open. Do not export account names, credentials, cookies or tokens in this review.

All 38 sampled SYSTEM entries show **Following: Inactive**, **MAVLink: Disabled**,
and **PX4: Disconnected**. Classic CSRT and Smart standby appear. No aircraft or
physical gimbal operation is qualified by this run.

## Warnings, losses and limits

- QGC's six-line log records C-locale resource warnings, the existing QQuickPinchArea
  property warning and host geolocation access denied. It contains no QML binding
  loop, TypeError, crash or PixEagle request exception. Geolocation denial concerns
  the host positioning permission and did not block the accepted PixEagle workflow.
- Backend warnings: source 1280×720 differs from configured 640×480; one confirmed
  CSRT appearance loss and bounded recovery; OSD auto-degrade from balanced to fast
  after a 25.42 ms render against a 25.00 ms budget. These remain observable baseline
  conditions, not hidden clean-run claims. Frame geometry checks and prior test
  evidence remain the basis for targeting correctness; the log warning alone does
  not establish a mapping error.
- Smart tracking exhausted on occlusion once and left-frame twice; logs require
  reselection. This is not a detector-accuracy or persistent-identity qualification.
- Runtime logging has no access-log HTTP status census. Count only the specific audit
  events above; authorization `allowed: 200` is not a successful action response.
- This saved session stopped Sep24. Do not imply that an app/server is still running
  or that live monitoring occurred during agent idle time.

No new release-blocking integration defect is established by these logs. Retain loss,
ambiguity and render/load observations for operational validation, rather than
loosening target acceptance to produce a cleaner demo.

## Position in the agreed plan

The original attached brief and current OPERATOR-COVERAGE agree on this sequence:

1. **Slices 0–2:** implemented baseline workspace, default-off authentication and
   per-vehicle binding, read-only authenticated video and displayed-frame provenance.
   Keep historical baseline caveats visible: older stock suite 405/406 plus unchanged
   isolated pass; pinned-base backend hygiene failure in the older broad gate.
2. **Slice 3 / 3a / 3b:** Classic and Smart selection/retarget/Cancel, advertised Classic
   choices, installed Smart models, shared state and simplified direct targeting are
   implemented; the latest custom suite passed 411/411. User acceptance was received
   Sep26. External-camera target contracts/mocks exist, but **physical external-camera
   tracker qualification is still open**. Accepted local replay does not close that gate.
3. **Next, within slice 4:** capability-driven gimbal motion and following readiness,
   start/stop/abort, follower profile choice, background vehicle-specific status/Stop.
   These are not implemented operational controls merely because target contracts
   exist. Preserve separate Stop tracking, Stop following and Stop camera intents.
   Implement mocked guards first. User preference prioritizes real gimbal/camera work
   after Smart acceptance; ask for connection details/hardware only when its test is
   prepared. Hardware, SITL and actual flight require their distinct authorized tests.
4. **Slice 5:** Linux/Windows/Android release acceptance, onboarding/troubleshooting,
   extension documentation and upstream preparation remain. No new numbered slice is
   required for menu docs/dashboard links; they close current usability feedback.
5. **Explicit later scope:** recording/media administration; detailed config and model
   upload/delete/trust; account administration and diagnostics/restart. Native redetect,
   tracker restart and segmentation are deliberately deferred with documented guards
   and model-compatibility requirements. Full dashboard parity was never silently
   included in the initial operational release. A browser shortcut can provide access
   to these existing dashboard tools without claiming native coverage.

Update OPERATOR-COVERAGE's current boundary and Smart rows from “human acceptance
pending” to the dated user acceptance. Preserve older SLICE-3B as a historical
checkpoint; current feedback/links checkpoint should supersede its old model dialog,
port 8096, recorded test10 and manual-arm instructions. Close any broken manifest or
review links in the current feedback checkpoint before declaring documentation done.

## Evidence files

Source root: `~/.cache/pixeagle-qgc-baseline/slice-3b-feedback-2026-09-24/`.
Hashes below describe the saved logs at review time; source logs stay private.

| Relative file | SHA-256 |
| --- | --- |
| `desktop/qgc.log` | `04d56c64db5ca0ab4b7ffbbc3a048a2c587b357fb14e56f8101abcd4612c15d2` |
| `desktop/pixeagle.log` | `968fc9389194d7036c06f09c312fb7631c534cdce077b8086cb1ec6f2b0523cd` |
| `replay-test4/logs/security-audit.jsonl` | `b1fc24f91e4b7ddf35f6db6b786e438b875f07f5bce5149520054b4536a8f7ef` |
| `replay-test4/logs/runtime/native_replay_5ede6d114ffa7695/components/backend.jsonl` | `be1093ff5760a3de7903483ebe6dd156b32bfc91ca7596bda64d55b94cb6ee23` |
