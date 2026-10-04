# 4b.4a camera-only axis characterization

This check measures the physical camera convention for the base-pitched-up
90° installation. It does not command an aircraft or qualify following. The
camera must have clear mechanical travel and a fixed scene with recognizable
left/right/up/down landmarks. Stop if it strains, oscillates, or approaches a
mechanical limit.

For the 2026-10-01 camera-v6 bench, run
`~/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/run-camera-v6-desktop.sh`
from the desktop terminal. The private login is in
`camera-v6/credentials.json` under that same directory. The launcher uses the
current QGC Debug build and current PixEagle source snapshot, with isolated QGC
settings and logs. It refuses an occupied camera/backend port instead of
closing another operator session.

The bench profile differs from an ordinary PixEagle installation: Camera
Classic (`Gimbal`) is selected, RTSP uses `192.168.0.108`, PixEagle flight
commands are blocked, Follower Test is configured, MAVLink and telemetry are
disabled, and no aircraft is connected. The two saved follower mount values
remain `HORIZONTAL`; following must stay off, so they have no effect on this
camera-only check. QGC integration is enabled only in the isolated bench
settings; the product default remains off.

1. Sign in to PixEagle in QGC and confirm there is no vehicle. In Options,
   verify the command block is confirmed and Follower Test is labelled as a
   test. Do not permit PixEagle flight commands.
2. Open the movable camera-control panel and its `…` readings. Confirm
   tracking and following are stopped, and the coordinate label says camera
   body angles. Leave the camera at a pose with room to move on all axes.
3. Apply one small joystick deflection for 0.3–0.5 seconds, then release and
   press Stop. Do this separately for yaw left, yaw right, pitch up, pitch down,
   roll left and roll right. Wait for the camera to settle between pulses.
   Keep the speed low and do not use Home or tracker selection during this
   sequence.
4. For each pulse, record (a) what the **lens** did relative to the intended
   aircraft nose/right/down directions, (b) which way the fixed scene shifted
   or rotated, and (c) which of yaw/pitch/roll changed and its sign in `…`.
   A brief video or screenshot is optional; a six-row written report is enough.
5. If all axes behave cleanly, repeat one lateral and one vertical pulse at a
   second modest pose. Record any sign reversal, coupling or mechanical limit.
   Close QGC to stop the bench.

The expected software behavior is immediate movement, prompt Stop, no delayed
old movement, and stale readings visibly losing their live status. No expected
physical sign is claimed yet. The reported observations plus the private
`camera-v6/logs/` and `desktop-v6/qgc.log` will determine the shared aircraft
mount transform. Synthetic formulas cannot substitute for this observation.

## 2026-10-01 first-pose observation

The operator reported that all six camera-only controls moved as intended.
Specific observations: joystick yaw-left panned the image left while the
reported **roll increased**; pitch-up reduced reported **pitch** from about
90°; roll-left reduced reported **yaw**. The paired opposite-direction controls
were also judged correct, but their numeric signs were not separately reported.
The operator confirmed that the neutral lens faces the intended aircraft
forward direction and that left/up controls retained their expected directions
at a second modest pose without reversal. The backend log records separate
pan, tilt and roll gesture dispatches with a Stop after each, while MAVLink and
following remained disabled. These raw observations establish local axis/sign
behavior for this camera and installation. They do not independently establish
the full Euler rotation order, all mechanical limits or PX4 response. Do not
infer a universal yaw/roll swap for other installations.
