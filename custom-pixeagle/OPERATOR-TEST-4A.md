# Slice 4a operator checkpoint — local PX4 SIH

This is a **simulation-only** QGC run. The launcher creates a private Docker
network for PX4 SIH and PixEagle, forwards only the authenticated API to host
loopback, and sends simulated MAVLink telemetry to QGC UDP 14560. It opens the
custom Linux Release app with separate settings. Closing QGC stops the
dashboard, PixEagle and SIH containers.

From a desktop terminal:

```bash
cd /home/alireza/qgroundcontrol-pixeagle
custom-pixeagle/validation/run-sih-desktop.sh
```

The private login file is
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/sih-live-v1/credentials.json`.
In PixEagle settings, the launcher fills `http://127.0.0.1:8094` for both
companion-only mode and the detected simulated aircraft. Sign in, click
**Verify vehicle**, wait for **Aircraft association verified**, then click
**Open Fly View**. QGC should show the simulated vehicle and the fixed target
image. Authentication and aircraft verification are separate steps. If you enter
Fly View before verification, its connection card now has **Verify vehicle**
and **Settings** buttons. Click Verify vehicle there; video and the tracking
panel appear when verification and streaming are ready. The dashboard shortcut opens
`http://127.0.0.1:3040` while this run is active; sign in there separately if
you want the detailed settings. The signed-in backend address remains readable
and selectable; sign out before changing it.

1. In Fly View, choose **Classic** or **Smart** directly on the compact panel.
   The small **PixEagle** title identifies the panel.
   The target and aircraft icons identify the tracker/model and follower rows;
   each has a short status and indicator. The **gear** opens Tracking options:
   choose the Classic tracker or Smart model and compatible follower there.
   Its **Settings** shortcut opens QGC's PixEagle page; **Dashboard** opens
   PixEagle in the system browser using the configured dashboard address.
   Tap or drag over the central target. While the
   simulated vehicle is disarmed, Start following should be disabled with
   “Take off before following.”
2. Use QGC's normal simulated vehicle controls to arm and take off. Once it
   reports airborne, Start following should become available. First try a short
   press: it must not start following and should show **Hold to Confirm**.
   Then hold **Start following** through QGC's confirmation animation. Observe
   Offboard and green **Following** for several seconds, then click **Stop
   following** once; Stop has no extra confirmation. PX4 should return to Hold.
   Use QGC to land, then close QGC.
3. For the moving bundled clip, start a new isolated run:

   ```bash
   custom-pixeagle/validation/run-sih-desktop.sh --video test9
   ```

   Open the gear. If Smart is unavailable because no model is selected, choose
   an installed VisDrone model in **Smart model**, then choose **Smart**.
   Select a visible vehicle in the video. Try Classic on the same clip as well. The test9 frames are
   replayed at 30 fps through a timestamped, clock-synchronized GStreamer
   source **only inside this SIH fixture**. The 282-frame loop takes about
   9.4 seconds; a slow consumer drops old frames rather than accumulating lag. If Smart cannot acquire or retain a target, note the on-screen
   reason and continue with Classic; do not force a follow Start. When target
   tracking is stable, repeat simulated takeoff, Start, Stop and Land.

4. Drag the small grip at the top of the panel to uncover the video. Dragging
   must not select a target. Resize the window: the panel must stay reachable.
   **Tracking options → Reset panel position** restores its initial location.
   Normal connection status is the camera indicator in QGC's top toolbar;
   its drawer provides connection details, Dashboard and Settings. A connection
   card appears in the video only when action or recovery is needed and can
   also be moved. **Tap video to select targets** remains in PixEagle settings.
5. When a target is lost, expect amber **Lost**, not a green tracking indicator.
   Freshness loss uses **Checking…**. A last known following session retains
   its Stop recovery action. Report any Start/Stop rejection text; do not treat
   a green tracker alone as permission to follow.

Please report which tracker/model/follower you selected, whether the ground
Start gate, active/Stop state and QGC flight controls behaved as expected, and
any confusing labels or video/OSD issues. The correlated QGC and dashboard
logs are under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-operator-desktop/`;
the selected PixEagle source logs are under the corresponding `sih-live-v1` or
`sih-test9-v1/logs/` directory. Report the approximate test time so they can
be matched. On restart, previous logs move to the operator desktop `history/`
directory. Credentials should stay in the private file.

Outside this isolated demo, an empty backend address resolves to
`http://127.0.0.1:5077`. Clearing the demo address therefore selects the normal
local service; restore `http://127.0.0.1:8094` to use this SIH run. Address and
placeholder text use the QGC text-field palette in light and dark themes.

`--check` starts the stack, checks backend login, authenticated WebSocket JPEG
delivery across the Docker bridge, dashboard availability and MAVLink delivery, then stops everything without opening QGC. The fixed-image
and test9 checks passed on 2026-09-27. The test9 source is a controlled replay;
real camera and real-aircraft behavior are not qualified by this run.

Close QGC or press Ctrl-C once, then wait for the shell prompt before starting
another run. If the launcher is terminated abruptly, the next start removes
its own stale demo services. If an unrelated application owns a required port,
it reports the port number and leaves that application alone.

## Scripted moving target

For a repeatable Classic tracker test with known motion:

```bash
custom-pixeagle/validation/run-sih-desktop.sh --video target-path
```

Choose Classic, select the square marker, and observe a 30-second loop:

| Time in loop | Marker motion |
| --- | --- |
| 0–3 s | Center, stationary |
| 3–7 s | Move right |
| 7–9 s | Hold right |
| 9–13 s | Move left |
| 13–15 s | Hold left |
| 15–19 s | Move up to center-top |
| 19–21 s | Hold top |
| 21–25 s | Move down |
| 25–27 s | Hold bottom |
| 27–30 s | Return to center |

The image includes the frame index and fixture time. Use those for comparisons,
not wall-clock time: the video pipeline may drop old frames under load. The
frame-by-frame reference is
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/sih-target-path-v1/target-path-frames/ground-truth.json`.
It records normalized and pixel bounding boxes, timestamps and PNG checksums.
The marker is 120 × 120 pixels in a 640 × 480 frame; its center moves between
normalized coordinates 0.3 and 0.7. Motion eases between stationary waypoints
and returns smoothly to the beginning of the loop.

This is a Classic tracking fixture. Use **test9** for Smart detection of real
objects. In SIH, the scripted fixture can also exercise Start, follower command
response, Stop and loss/recovery observations. The image does not respond to
the simulated aircraft's movement, so this is not a closed-loop flight-quality
test or evidence of safe real-aircraft following.

The generated fixture and its config are separate from fixed-image/test9 runs.
Its private login file is in `sih-target-path-v1/credentials.json`. The launcher
prints that path and the ground-truth path. Close QGC before changing fixtures.
