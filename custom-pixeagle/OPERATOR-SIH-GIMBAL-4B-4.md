# Vertical-camera PX4 SIH operator checkpoint

This checkpoint uses the connected Topotek camera at `192.168.0.108` and a
Docker-only PX4 SIH vehicle. PixEagle flight commands begin blocked. It is a
simulator test, not a real-aircraft test. Keep any physical flight controller
or vehicle disconnected from this computer during the session.

The fresh private profile is
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/camera-sih-operator-v5`.
It starts with Camera Classic, the vertical mount preset, and
`gm_velocity_chase`. The camera installation setting lives in PixEagle's web
configuration; QGC has no duplicate mount setting.

1. In terminal A, run:

   ```bash
   cd /home/alireza/PixEagle-qgc-integration
   bash tools/run_gimbal_sih_probe.sh --hold /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/camera-sih-operator-v5
   ```

   Wait for “Isolated SIH stack is ready.” The launcher verifies the camera's
   live angles and video, PX4 telemetry, the simulated UID and the active
   flight-command block. It also checks that QGC UDP port 14560 receives
   system 1. Leave this terminal open.

2. In terminal B, run:

   ```bash
   cd /home/alireza/qgroundcontrol-pixeagle
   bash custom-pixeagle/validation/open-gimbal-sih-qgc.sh /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-01/camera-sih-operator-v5
   ```

   Sign in through QGC's PixEagle menu using the private login in
   `camera-sih-operator-v5/credentials.json`. In PixEagle Settings, select the
   simulated QGC vehicle and choose **Verify vehicle**. The QGC launcher keeps
   separate settings and checks the live simulated UID and circuit breaker
   first. Because the backend is connected to SIH, target commands remain
   unavailable until QGC has verified that vehicle association.

3. Check Fly View before enabling aircraft following: one simulated vehicle,
   live camera video, fresh camera angles and **Commands blocked**. Select a
   visible camera target with Camera Classic. Confirm tracking and Stop or
   retarget work without unintended camera motion. If these checks fail, stop
   here and send the observations and log paths.

4. For the first flight-control test, use QGC to take off only the SIH vehicle.
   Once it is stable and the camera target is still tracked, open PixEagle
   Options and turn off **Block PixEagle flight commands** with its explicit
   confirmation. Hold to start `gm_velocity_chase` briefly. Observe whether
   the simulated vehicle responds in the expected target direction. Use the
   immediate PixEagle Stop, then re-enable the command block. If behavior is
   unexpected, Stop first and use an ordinary QGC flight-mode change for
   pilot takeover. Do not leave following active when closing QGC.

5. Report the target and approximate camera direction, whether the SIH
   vehicle armed/took off, the start and stop results, any drift or delayed
   response, and the exact time of any warning. Raw observations are useful;
   the logs will be reviewed separately before trying `gm_velocity_vector`
   or a pilot-takeover scenario.

Close QGC, then press Ctrl-C once in terminal A and wait for its prompt. The
launcher stops only its owned containers and archives earlier backend logs on
subsequent runs. Backend logs and `stack-probe-result.json` are under the
profile's `logs/`; QGC logs and its binary checksum are under
`qgc-desktop/`. A profile whose configuration has changed is deliberately
rejected on a new launch; prepare a fresh private profile for another session.

Current automated evidence covers camera/PX4 startup and identity, raw
world-target-to-follower direction tests, and typed injection parsing. Actual
gimbal-follower setpoint delivery, PX4 response, Stop and takeover remain
pending this operator SIH checkpoint.
