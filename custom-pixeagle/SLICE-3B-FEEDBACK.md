# Slice 3b — direct targeting and simplified controls

2026-09-24: ready for the user's next hands-on test. This extends
[the installed-model checkpoint](SLICE-3B.md); its archived evidence is unchanged.
Following, physical camera/gimbal operation and slice 4 remain outside this demo.

## Changes from operator feedback

- PixEagle remains disabled by default. The demo launcher now also starts it
  disabled; enable it explicitly in Application Settings → PixEagle.
- The video's **Sign in** action opens that settings page.
- **Classic / Smart** is the first control row. Classic shows the advertised
  tracker dropdown; local Smart shows the installed model dropdown in place.
  Choosing a model submits that explicit choice without opening another page.
- **Tap video to select targets** defaults on. A fresh tap selects or replaces a
  target; Classic also accepts a drawn box. With the preference off, **Select
  target / Retarget** arms one gesture. **Cancel selection** disarms it without
  stopping the current track; **Cancel tracking** clears the track.
- Cancel appears for relevant target states. Unavailable-tracker diagnostics
  moved to Settings → Show connection details. Options contains the tap
  preference and secondary model details.
- Gesture ownership, displayed-frame provenance, freshness and mutation guards
  remain enforced. Refreshing a source permits a new tap; it never resends an old
  one. Hidden/compact views cannot select. Model/tracker dropdowns capture their
  destination and stable choice; stale lists cannot redirect an action.
- Model changes show confirmed backend state, waiting and failure text. A retry
  clears old conflict text. Model polling follows the visible Smart view;
  deferred refresh avoids synchronous QML binding loops.

## Validation and limits

Final custom Debug/Release builds and scoped locked pre-commit checks passed.
The final custom `Unit|Integration` run, excluding `Flaky|Network`, passed
**411/411 tests in 239.28 seconds**, including stock UI coverage. Controller
coverage includes direct Classic/Smart/external selection, manual one-shot
behavior, captured-frame/context boundaries, pending actions and model retry.
The earlier focused three-test run also passed.

Private GUI checks verified default-off state, sign-in navigation, direct Classic
selection and replacement, Cancel, and manual gating. The unarmed manual tap
kept target revision 4; one armed tap advanced it to 5; a subsequent unarmed tap
left it at 5. Smart activation and inline keyboard model selection were checked
against authoritative backend state. These are interaction checks, not a new
detector accuracy claim. Remaining Smart target quality tests are handed to the
user as requested. No shared desktop was captured or automated.

The first walkthrough exposed a model-refresh binding loop, corrected before
the final builds. Initial failed coordinate/keyboard-driver attempts and source
refresh conflicts remain in the evidence directory. A compact-window capture
attempt failed in the driver; this pass does not claim new compact-layout
acceptance. Prior stock-test and backend baseline caveats remain in SLICE-3B.md;
stock source did not change during this feedback pass.

The replay helper adds an explicit loopback dashboard origin and temporary admin
role. **30 replay-tool tests, 74 required route/config tests, and schema validation
(43 sections / 606 parameters) passed**, along with fatal Ruff/compile/diff
checks. The dashboard built using existing dependencies. Separate headless
browser sign-in, Settings read access and authenticated WebSocket video passed;
the configured origin was accepted and an unlisted origin rejected. The main
PixEagle checkout remains untouched.

Evidence directory:
`~/.cache/pixeagle-qgc-baseline/slice-3b-feedback-2026-09-24/`.
See [the manifest](slice-3b-feedback-manifest.json) and
[review disposition](reviews/slice-3b-feedback-disposition.md). Reviews are
simulated expert/operator feedback, not a human study.

## Current test session

| Purpose | Address |
| --- | --- |
| QGC PixEagle backend address | `http://127.0.0.1:8097` |
| Browser dashboard, same backend | `http://127.0.0.1:3040` |
| Usual standalone installation | Dashboard 3040; backend 5077 unless configured differently |

QGC needs the backend/API origin, without adding `/api/v1`. For example, use
`http://device-ip:5077` when the standalone dashboard is on port 3040. A configured
reverse proxy may instead provide an API base prefix; use its backend route.

Both demo sign-ins use the private account in
`replay-test4/credentials.json` under the evidence directory. The account allows
dashboard Settings access. The snapshot uses bundled **test4.mp4**, both local
VisDrone models and the Full AI runtime. Test9 was also prepared and checked,
but its 9.43-second loop frequently refreshes the source; test4 runs 53.77 seconds.
Test4 contains recorded tracking graphics, and its existing replay profile keeps
backend OSD enabled. Those overlays are separate from QGC's native control panel.

Enable PixEagle, sign in, and open Fly View. Try both modes and their dropdowns,
direct selection/retarget and Cancel. Under Options, turn off tap selection and
compare the one-gesture Select target/Retarget flow. Use the dashboard alongside
QGC if desired; changes affect this same copied backend. Send approximate times,
model/tracker, actions and observations so the saved logs can be correlated.

`desktop/qgc.log` and `desktop/pixeagle.log` are recorded while the app runs;
`dashboard-root.log` records the frontend server. Closing QGC stops its replay
backend. Persistent dashboard configuration edits beyond installed-model choice
can intentionally fail strict snapshot validation on a later restart; preserve
the edited copy and prepare a new reviewed snapshot instead of bypassing that
check. This limitation concerns the isolated reproducible demo.

To relaunch an unchanged snapshot after closing the app:

```bash
PIXEAGLE_DESKTOP_DEMO_DIR=/home/alireza/.cache/pixeagle-qgc-baseline/slice-3b-feedback-2026-09-24/desktop \
  custom-pixeagle/validation/run-replay-desktop.sh \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-3b-feedback-2026-09-24/replay-test4
```
