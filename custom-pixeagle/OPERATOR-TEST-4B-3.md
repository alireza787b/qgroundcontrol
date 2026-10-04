# Ethernet camera bench — slice 4b.3

The responsive-control software gate passed. Physical responsiveness, camera
tracking and axis direction now need operator acceptance. PX4 is not required;
aircraft commands remain inhibited throughout this bench.

## Open the fresh session

From your desktop terminal:

```bash
~/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/run-desktop.sh
```

Close the camera-v4 QGC window first. If tracking is active, stop it before
closing. The new launcher will refuse to take over an active older session.

The launcher now opens camera-v5, reuses only the matching authenticated backend,
and keeps QGC settings separate from previous demos. It starts missing services
and cleans up processes it started when QGC closes. If an older camera-v2/v3/v4
backend remains after its QGC window closes, the launcher verifies its identity,
idle tracking/manual state and aircraft inhibit before stopping it. An active or
unknown process is never stopped automatically; close an older QGC window first.

- Backend address: `http://127.0.0.1:8095` (already set in this isolated profile).
- Dashboard: `http://127.0.0.1:3040`.
- Username/password: the private file
  `~/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/camera-v5/credentials.json`.
  Use this fresh file; camera-v1/v2/v3/v4 credentials do not apply.
- Sign in to PixEagle, then choose **Open Fly View**.
- Expected: Camera Classic selected, Camera Smart available, live RTSP video,
  GM PID Pursuit visible, and **Bench: following disabled**. No target should
  start tracking automatically. The normal QGC aircraft-disconnected indication
  is expected.

## Configuration differences from ordinary local tracking

This private snapshot uses the existing configuration groups:

```yaml
VideoSource:
  VIDEO_SOURCE_TYPE: RTSP_OPENCV
  RTSP_URL: rtsp://192.168.0.108/stream=0
  FRAME_ROTATION_DEG: 0
  FRAME_FLIP_MODE: none
Tracking:
  DEFAULT_TRACKING_ALGORITHM: Gimbal
GimbalTracker:
  ENABLED: true
  CONTROL_ENABLED: true
  PROVIDER: topotek_sip_udp
  UDP_HOST: 192.168.0.108
  UDP_PORT: 9003
  LISTEN_PORT: 9004
  COORDINATE_SYSTEM: GIMBAL_BODY
Follower:
  FOLLOWER_MODE: gm_velocity_chase
  FOLLOWER_EXECUTION_MODE: COMMAND_PREVIEW
FOLLOWER_CIRCUIT_BREAKER: true
```

MAVLink and aircraft telemetry connections are also disabled. These are bench
overrides, not changes to normal shipped defaults. This mount is the confirmed
base-pitched-upward-90° setup; raw camera-body angles are preserved. No guessed
axis swap or aircraft transform has been applied.

For RTSP with PixEagle's own tracker, select **PixEagle** in Tracking options →
Tracking engine, then Classic or Smart. This retains camera controls. To save
local Classic as startup, use a local `DEFAULT_TRACKING_ALGORITHM`, such as CSRT.
Compatible follower choices come from the backend; changing the engine never
silently replaces or starts a follower. Camera-application ownership instead
uses `CONTROL_ENABLED: false`, followed by the indicated backend restart.
See [all camera scenarios](CAMERA-SCENARIOS.md) for the complete alternatives.

## Test in order

1. Confirm the live video is from this camera and has the expected orientation.
   Open the gear → **Camera controls**. Drag the panel by its title if necessary.
2. Make brief, small pad movements, pan then tilt, releasing after each. Confirm
   direction and prompt stopping. This adapter uses one dominant axis at a time;
   a diagonal gesture is not simultaneous pan and tilt. Greater displacement
   should increase speed.
3. Test neutral, release outside the pad, panel closure and changing application
   focus. Old commands must not continue or resume. Then, within available
   mechanical travel, test a longer gentle hold and direction reversal. There
   should be no five-second “Release to continue” cutoff.
4. Test roll using the two buttons below the pad and zoom using the adjacent
   buttons. Use Center only if its travel is appropriate for this mount. Open
   **…** to inspect yaw, pitch and roll. These are camera-body angles; no measured
   zoom is claimed because this adapter does not provide it. Camera-generated
   text already burned into the RTSP image is separate from QGC telemetry.
5. Leave the camera panel open and select a target on unobscured video. Test **Camera Classic**
   point/rectangle selection, target loss, cancel and retarget. Then test
   **Camera Smart** point selection using the camera's own detection. Manual
   takeover should disable camera tracking once; releasing must not resume it.
   The camera-v2 `frame_evicted` conflict should no longer occur on ordinary
   clicks. Camera-v4 held a QGC-style delivered frame through a 1.2-second
   delayed dry-run. On a slower or paused link, a frame older than 1.5 seconds
   is rejected safely with an expired-frame message; tap fresh video.
6. Choose the **PixEagle** engine. Test local Classic and installed local Smart
   models on RTSP. Return to Camera and back; each engine should restore its
   prior valid session selection. Runtime switches do not rewrite startup
   configuration. Local Smart can load a model; Camera Smart does not.
7. In Tracking options, choose a compatible camera-angle follower while still
   in this camera-only bench. The choice should save, but Start must remain
   disabled because aircraft commands are inhibited. Switch to PixEagle tracking
   and confirm the list changes to its compatible followers.
8. Repeat a brief manual movement while local Smart is active. Check that control
   does not accumulate delayed commands. Compare dashboard and QGC state; a
   second client must not take over an active manual hold. Following remains
   disabled in both applications.

Report the approximate time, step, mode, visible message and observed movement
for any failure. The physical Stop check matters even when HTTP reports success.
UDP delivery and mechanical stopping are not proven by software timing tests.

## Logs and next checkpoint

Logs are retained under:

```text
~/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/camera-v5/logs/
~/.cache/pixeagle-qgc-baseline/slice-4b3-2026-10-01/desktop-v5/qgc.log
```

The backend records gesture identities, sequences, receipt, dispatch, cancellation
and Stop timing without credentials. QGC camera debug logging is enabled for
this isolated handoff. Logs can be reviewed after your test; this does not imply
continuous assistant monitoring while the conversation is idle.

After camera acceptance, 4b.4 qualifies mount coordinates and angle followers
with SIH. Windows/Android and release qualification remain slice 5. The
[slice report](SLICE-4B-3.md) records automated results and outstanding limits.
