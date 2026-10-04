# Slice 3a — Classic demo feedback checkpoint

Recorded 2026-09-22. The four human feedback items are addressed: direct
retargeting, real single-click verification, catalog-driven tracker choices with
unavailability reasons, and compact connection/status presentation. The original
slice 3 handoff manifest remains unchanged. Full slice 3 remains open for the
[Smart model and target-mode follow-up](OPERATOR-COVERAGE.md); slice 4 has not started.

## Resulting interaction

- Select target once, then click or drag a box. After an accepted selection,
  click or draw again to replace it. Each press captures its own displayed frame,
  transform, destination and target guard; previous metadata is not reused.
- Finish selecting leaves selection mode while tracking continues. Cancel
  tracking stops the track. Retarget enters selection again when needed.
- Context changes, source retirement, view cancellation, rejected/uncertain
  requests and following restrictions prevent automatic rearming. A late accepted
  reply cannot rearm selection after fullscreen/view cancellation. New actions
  are not retried automatically.
- Double-click fullscreen is preserved through Qt's normal single-click delay;
  the first click of that gesture does not send a tracking request. Standard
  camera gestures retain their own destination and capability checks.
- A small camera/status button replaces the persistent three-line card. Details
  retain companion/vehicle identity and reception status. Active means receiving
  fresh frames, including replay; it does not mean a live camera. Slower runtime
  polling is not repeated beneath authoritative target status.
- Tracking setup lists what the connected backend actually supports. This Core
  demo has CSRT, KCF + Kalman and Sparse Flow. Unavailable trackers explains the
  absent model artifacts, optional dlib, configured camera, or Full AI runtime.

## Validation and exact scope

Evidence root:
`~/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21/user-feedback`.
The date in the cache directory identifies the originating slice, not every run.

| Check | Result and evidence |
| --- | --- |
| Custom Debug and Release | Successful incremental builds; `debug-build-details.log`, `release-build-details.log` |
| Stock Debug and Release | Successful builds; `stock-debug-build-final.log`, `stock-release-build-final.log` |
| Full custom Unit/Integration, excluding Flaky/Network | 410/410 passed; `custom-wording*`; includes StockUI. Before the last detail-label cleanup described below |
| Full stock, same exclusions | 406/406 passed; `stock-final*` |
| Final target/startup/shared-adapter checks | 82 Qt cases, zero failed/skipped; `focused-handoff*` (3 CTest entries) |
| Earlier focused plus GPS rerun | 85 Qt cases; `focused-wording*`; GPS exited normally |
| Changed-source hooks | Passed; `lint-wording.log`; final documentation gate recorded separately |
| Final Release with production Core replay, no PX4 | Point followed by direct point accepted at revisions 1 and 2; `handoff-ui/02-point.json`, `03-direct-point.json`; final labels/details in `04-details.png` |
| Earlier same-behavior Release interaction | Point → direct point → direct box at revisions 1/2/3; Cancel at revision 5; double-click fullscreen kept revision 5; `final-ui/03`–`07` |
| Leave selection without stopping tracking | `final-ui/17` and `18`: revision 6 remained unchanged after Finish selecting (then labeled Selecting) and another video click; tracking intent remained active |
| Layout and availability dialog | Wide and 480 × 720 private captures; `final-ui/09`–`13`; Escape closes dialog; stock compass/telemetry remain accessible |
| Backend | Production source unchanged in this feedback pass; earlier 1224-test broad and 110-test catalog/route gates retained in the original slice 3 record |

The last cleanup changes the compact word Live to Active and suppresses a
redundant slower status line when the target controller exists. After that
cleanup, Debug/Release were rebuilt and the focused suite and actual Release
screen were checked; the broader suite above is identified precisely rather
than represented as running after every text-only edit.

Intermediate results are retained. The first combined run in this pass passed
410 tests before the fullscreen cancellation correction. The subsequent
`custom-final*` run passed 409/410: GPSProtocolReceiverModesTest completed all
three Qt assertions, then timed out at 60 seconds during process exit. Its
isolated rerun and subsequent full 410-test run passed without changing GPS or
loosening its timeout. This is a non-reproduced process-exit failure, not an
established diagnosis. Earlier interrupted builds and test-fixture compile
errors are retained in their original logs.

Private Release logs contain expected unavailable-network map warnings and
baseline locale/Qt warnings. No missing camera icon, ReferenceError or TypeError
was observed in the final private UI run. The recorded soccer clip has scene
changes: an accepted point/box does not establish stable tracking accuracy.
Image annotations and polled status are not synchronized native overlays.
Earlier upstream lint, dashboard CI and backend teardown qualifiers remain;
these local passes do not claim green remote CI or Windows/Android qualification.

## Human feedback, log correlation and review

The [raw human feedback](reviews/slice-3-user-feedback-raw.json),
[independent simulated screenshot review](reviews/slice-3-operator-feedback-raw.md),
and [disposition](reviews/slice-3-feedback-disposition.md) are separate records.
The final wording corrections respond to that review; the reviewer did not see
the final corrected labels. Primary-agent inspection verified those final labels.

Earlier desktop audit analysis found 19 successful native operations: ten
selections, eight Cancels and one tracker switch. One further authorized
tracking-start lacks a retained result, so its outcome is unknown. Two 64 × 64
initializations are consistent with point selection, but that historical log
alone cannot prove the gesture type. New real GUI input supplies that evidence.
See `../desktop-demo-feedback-readonly.json` relative to the evidence root.

## Updated desktop demo

Use the existing private `clean-demo-v4/credentials.json`; credentials are never
saved in QGC settings or source manifests. Launch from the checkout:

```bash
PIXEAGLE_DESKTOP_DEMO_DIR="$HOME/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21/desktop-feedback-demo" \
  custom-pixeagle/validation/run-replay-desktop.sh \
  "$HOME/.cache/pixeagle-qgc-baseline/slice-3-2026-09-21/clean-demo-v4"
```

Sign in under Application Settings → PixEagle, then Open Fly View. Select once,
replace directly with a point/box, Finish selecting, Cancel tracking, inspect
Tracking setup and try fullscreen. Report the approximate time, tracker and
observed behavior. Logs are `desktop-feedback-demo/qgc.log` and `pixeagle.log`;
closing QGC stops the backend started by its launcher. The agent can review
saved logs when working; no continuous monitoring is implied while paused.

PX4 is unnecessary for this companion-only test. Smart model choice, actual
Smart detection and gimbal hardware are not available in this replay. The
[coverage plan](OPERATOR-COVERAGE.md) tracks those gates and the later following,
camera movement, media and administration work.
