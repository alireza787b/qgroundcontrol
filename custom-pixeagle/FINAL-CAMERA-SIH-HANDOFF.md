# Final camera/SIH checkpoint — 2026-10-04

This fresh profile uses the real Topotek camera at `192.168.0.108` and an
isolated simulated PX4 multicopter. Keep real flight controllers disconnected.
The camera image is independent of the simulated aircraft pose; this checkpoint
qualifies operation and command delivery, not visual closed-loop convergence.

## Configuration

The private profile selects RTSP `rtsp://192.168.0.108:554/stream=0`, Camera
Classic startup, optional Camera Smart from camera firmware, enabled camera
controls, the previously characterized vertical installation, and
`gm_velocity_vector` with coordinated turns. PixEagle flight commands start
blocked. Both gimbal followers retain altitude safety and the qualified
8-second/4-metre bounded recovery policy. No gains or speed limits changed.
Ordinary video-file/CSRT/PixEagle defaults are unchanged. Local AI is not loaded
for this camera-owned profile.

The profile's read-only preflight passed. Services remain stopped until the
camera is connected and the launcher runs. Credentials are in
[credentials.json](/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/camera-final-v28/credentials.json).

## Launch

Close QGC and stop the previous owned SIH launcher with Ctrl-C first. The
launcher reclaims abandoned labelled stacks but preserves active launchers and
unrelated port owners.

In terminal 1:

```bash
cd /home/alireza/PixEagle-qgc-integration
bash tools/run_gimbal_sih_probe.sh --hold \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/camera-final-v28
```

Wait for **Isolated SIH stack is ready**, then in terminal 2:

```bash
cd /home/alireza/qgroundcontrol-pixeagle
PIXEAGLE_QGC_BINARY=$PWD/build/pixeagle-custom-debug/Debug/PixEagle-QGroundControl \
  bash custom-pixeagle/validation/open-gimbal-sih-qgc.sh \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/camera-final-v28
```

Sign in at `http://127.0.0.1:8096` using the linked credentials. QGC discovers
the simulated aircraft on UDP 14560. Single-vehicle verification should begin
automatically; final association still requires matching fresh backend/QGC
identities. The Verify action remains available for an explicit retry.

## Short operator test

1. Keep **Block PixEagle flight commands** on. Confirm live video, Camera
   Classic acquisition/retarget/Stop, then Camera Smart selection and Stop.
2. With the camera panel open, confirm tracking still works. Try brief low-speed
   joystick, roll and zoom inputs; release and Stop. Check that old motion does
   not resume after release, panel closure, or changing target.
3. For simulated following only, take off above 5 m, acquire a target, explicitly
   unblock commands, and hold Start. Retarget left/right and up/down. Expect
   Changing target/Reacquiring followed by guidance; rejected edge selections
   preserve the existing target. Stop should confirm Hold.
4. While inactive choose `gm_velocity_chase` and repeat one retarget and Stop.
   Finish with commands blocked and the simulated vehicle landed.
5. Check Restart PixEagle in Backend settings only after tracking/following and
   manual movement are stopped. Expect reconnection without resumed operations.

Report approximate times for unexpected Hold, delayed motion, warnings, or
failed selection. Physical link/process-failure testing will be coordinated
separately after this basic test; it must record actual motor behavior rather
than treating UDP Stop transmission as proof of motor stopping.

## Evidence and remaining gates

Logs are below the profile directory in `logs/` and `qgc-desktop/qgc.log`.
Credentials are private; do not paste the password into logs or reports.
Close QGC and Ctrl-C the launcher when finished. Used profiles are immutable.

Physical camera acceptance and real network/process-loss behavior remain open.
Windows/Android qualification, install/upgrade validation, reviewed publication,
and the Pi/router/PX4 ground checkpoint follow. Customized QGC remains unreleased.
