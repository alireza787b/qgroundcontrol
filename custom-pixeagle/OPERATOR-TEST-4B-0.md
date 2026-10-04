# Operator test — 4b.0 Smart readiness

This tests local PixEagle Smart tracking over the bundled test9 replay. It does
not test the Ethernet camera, camera-owned tracking, gimbal movement, or real
aircraft control.

1. From a desktop terminal in `/home/alireza/qgroundcontrol-pixeagle`, start
   `custom-pixeagle/validation/run-sih-desktop.sh --video test9`. Open the private
   credentials file whose path the launcher prints; it contains `username`,
   `password`, and `endpoint`. Its QGC settings and backend are isolated.
   For this prepared fixture the file is
   `/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/sih-test9-v1/credentials.json`.
   The backend address is `http://127.0.0.1:8094`; the dashboard is
   [the local dashboard](http://127.0.0.1:3040). They are available while the launcher runs.
2. In PixEagle Settings, sign in, choose **Verify vehicle**, and open Fly View.
   The toolbar should identify PixEagle and show separate target/following
   states. The compact panel should show Classic/Smart, the active tracker or
   model, and concise status indicators.
3. Choose Smart in the Fly View panel, then open Options and select `visdrone26m`. Confirm the model name,
   runtime device/fallback text, and that a selected model is not described as
   running before Smart is active.
4. Move the PixEagle panel away from the picture using its grip if necessary.
   Select one detected target, tap a second target to retarget, then Cancel.
   Record whether toolbar and panel states agree. Repeat with `visdrone9m`.
   A tap with no matching detection can be rejected; it must not show Tracking.
   Note any visible target loss/recovery. Leave the simulated aircraft on the
   ground; takeoff and following are not needed for this check.
5. Stop once with Ctrl-C, wait for the shell prompt, and report approximate run
   time plus printed QGC/backend log paths. Do not send credentials.

The replay loops. If video becomes delayed, record the model/device and time;
this is an issue to investigate, not an expected end of clip. The latest private
run used CPU because the NVIDIA driver was unavailable. Short live samples
measured approximately 5.5 fps with `visdrone26m` and 2.3 fps with `visdrone9m`;
expect reduced fluidity on CPU. Start with `visdrone26m`. Smooth playback and
successful current-session target selection are not claimed as passed.

Report the model, displayed device, selection/retarget/Cancel result, and any
unexpected state or pause with an approximate time. Logs remain on disk after
exit: QGC under `slice-4-operator-desktop`, backend under
`slice-4-2026-09-26/sih-test9-v1/logs`, both in
`/home/alireza/.cache/pixeagle-qgc-baseline/`. Earlier logs are archived on the
next launch. This is an operator workflow check, not detection-accuracy or
flight-safety qualification; no camera connection is needed.
