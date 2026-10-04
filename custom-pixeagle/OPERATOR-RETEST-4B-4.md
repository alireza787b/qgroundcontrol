# Slice 4b.4: v15 camera/follower retest

This historical camera handoff is deferred while the operator is away from
hardware. Use the [recorded-video session](OPERATOR-RECORDED-SIH.md) for optional
camera-free tests. Prepare a fresh camera profile with current source before
resuming physical acceptance; v15 does not contain the shared restart closeout.

This checkpoint uses **simulated PX4**, with the real camera image independent
of simulated aircraft movement. Keep real flight controllers disconnected.
This is a prepared handoff, not a request to run while the camera is unavailable.
Connect the camera at `192.168.0.108` when ready; its accepted vertical
installation is already configured. Geometry remains backend configuration,
with no new QGC setup switch.

The [closeout record](SLICE-4B-4.md) separates automated results, raw AI reviews
and your pending acceptance. QGC debug binary SHA-256:
`f480082711619604e3ea9f3d2aebecec39695217b9dfd79f7cba8bda76cd6c77`.

## Start a fresh session

First close QGC and press **Ctrl-C in the previous SIH launcher**, waiting for
its owned containers to stop. The new launcher reclaims abandoned, labelled
stacks; an active launcher or unrelated port owner is preserved and reported.
Earlier logs remain intact. Preparing v15 did not start services or move the
camera; camera startup is checked by the command below.

In terminal 1:

```bash
cd /home/alireza/PixEagle-qgc-integration
bash tools/run_gimbal_sih_probe.sh --hold \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/camera-sih-operator-v15
```

Wait for **Isolated SIH stack is ready**. If startup fails, retain `v15/logs/`
and report the error. Keep this terminal running during the test.

In terminal 2:

```bash
cd /home/alireza/qgroundcontrol-pixeagle
PIXEAGLE_QGC_BINARY=$PWD/build/pixeagle-custom-debug/Debug/PixEagle-QGroundControl \
  bash custom-pixeagle/validation/open-gimbal-sih-qgc.sh \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/camera-sih-operator-v15
```

QGC should discover the simulated vehicle on UDP **14560**. Sign in with the
[v15 credentials file](/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/camera-sih-operator-v15/credentials.json).
Backend address: `http://127.0.0.1:8096`.

## Configuration differences

This isolated profile starts with **Block PixEagle flight commands on**,
Camera Classic, `gm_velocity_chase`, vertical installation and local AI off.
Both gimbal followers have altitude guidance and bounded recovery:
8 seconds / 4 metres of commanded horizontal travel, 0.5 seconds of stable
confirmation and 0.5 seconds of restoration. New provisional guidance uses a
70% angle blend and half authority; retained motion changes within the existing
acceleration/rate limits. Vector uses coordinated turn. Gains and speed limits
are unchanged. Camera Smart uses camera firmware rather than local AI.

Fresh ordinary PixEagle profiles retain local tracking. Existing saved flat
continuity choices stay valid; this test does not silently migrate them.

## Short operator test

1. **Commands blocked:** acquire a centre target, then select near an image
   edge. An invalid region should be refused without stopping the previous
   target. Briefly try Camera Smart and camera controls; tracking should remain
   available with the control panel open. Return to Classic for a repeatable
   follower test.
2. **Chase, simulated aircraft only:** take off above 5 m, acquire a target,
   explicitly unblock commands in Options and hold to start. Select another
   target left/right while following. Expect **Changing target**, then
   **Reacquiring**, then **Following**. Try a rejected edge selection again:
   existing tracking/following should continue. Make a second valid selection
   after confirmation; note the approximate times.
3. **Vertical response and recovery:** try a target higher/lower, then brief
   loss and reacquisition. Above the lower warning band, a lower target can
   request descent. Near a boundary the UI should report the altitude limit.
   Ordinary loss decays horizontal motion with zero yaw/vertical commands;
   repeated taps do not reset its recovery budget. Exhaustion requests Hold.
4. **Stop:** stop during a target change. Expect **Stopping**, then the retained
   confirmed result; no secondary publisher-failure message. Stop and safety
   restrictions bypass smoothing. A subsequent session clears the old result.
5. **Vector:** while inactive select `gm_velocity_vector` in Options and repeat
   steps 2–4. Coordinated turn should produce simulated yaw response. Finish
   with following stopped and the flight-command block on.

Report approximate times, accepted/rejected selections, unexpected Hold,
visible simulated yaw/altitude changes and Stop responsiveness. Camera motion
alone cannot prove aircraft response. The traces will be checked against
actual submitted commands, publication and independent PX4 observations.

## Logs and remaining checkpoint

Under the v15 directory: `logs/traces/tracker-command.jsonl`,
`logs/traces/offboard-publish.jsonl`, `logs/pixeagle.log`, and
`qgc-desktop/qgc.log`. Close QGC and press Ctrl-C in terminal 1 when finished.

Your acceptance and log review close this retest; constrained-network/load and
camera/process-failure qualification remain **4b.4d**. Platform releases and
reviewed publication are slice 5; Pi/router ground testing with commands
blocked is slice 6. This checkpoint does not qualify real flight.
