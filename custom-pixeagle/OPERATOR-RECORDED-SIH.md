# Recorded-video SIH handoff — v21

This camera-free session uses about **4 minutes 50 seconds of test11 thermal
footage**, local CSRT/installed Smart models and isolated PX4 SIH. It is prepared
but stopped. Final physical camera acceptance is postponed until your return.

This profile permits actual SIH commands through the existing follower path.
Dashboard **Follower Test** remains preview-only. The earlier v17 instructions
incorrectly implied replay could already start aircraft following; use v21.

## Launch

Keep terminal 1 running:

```bash
cd /home/alireza/PixEagle-qgc-integration
bash tools/run_gimbal_sih_probe.sh --hold \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-03/recorded-sih-operator-v21
```

Wait for **Isolated SIH stack is ready**. In terminal 2:

```bash
cd /home/alireza/qgroundcontrol-pixeagle
PIXEAGLE_QGC_BINARY=$PWD/build/pixeagle-custom-debug/Debug/PixEagle-QGroundControl \
  bash custom-pixeagle/validation/open-gimbal-sih-qgc.sh \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-03/recorded-sih-operator-v21
```

Sign in with the private
[v21 credentials](/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-03/recorded-sih-operator-v21/credentials.json).
Backend: `http://127.0.0.1:8096`. Simulated vehicle UDP: **14560**; the QGC helper
creates the isolated link and settings. Verify the simulated vehicle in PixEagle.
No physical camera is needed. Keep real aircraft links outside this demo.

Optional Dashboard, in terminal 3:

```bash
/home/alireza/PixEagle-qgc-integration/dashboard/node_modules/.bin/serve -s \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4-2026-10-03/recorded-sih-dashboard-build \
  -l tcp://127.0.0.1:3040
```

Open `http://127.0.0.1:3040` and use the same login. This compiled Dashboard targets
backend port 8096. QGC's Dashboard link opens this address.

## Configuration differences

- `VIDEO_SOURCE_TYPE: VIDEO_FILE`, `VIDEO_FILE_PATH: resources/test11.mp4`, paced real-time playback and Loop at EOF.
- PixEagle CSRT startup, installed VisDrone local Smart models; no gimbal provider/control.
- `FOLLOWER_MODE: mc_velocity_position`; ordinary Position remains yaw-only, without horizontal pursuit or automatic altitude guidance.
- Existing `FOLLOWER_EXECUTION_MODE: PX4`; advanced `SIH_RECORDED_VIDEO_FOLLOWING: true` in this explicit test profile only.
- Altitude safety and **Block PixEagle flight commands** initially enabled. Gains, speed and acceleration limits are unchanged.

For other installations the SIH setting stays off by default. It lives under
Dashboard Configuration → Follower, requires a backend restart, and only admits
replay within a launcher-owned verified isolated SIH stack. It is not a general
hardware-replay override. QGC has no duplicate setup switch.

## Checks already completed

Final gates: 3,460 backend Unit, 269 Integration/route/config and 48 API/docs/test-hygiene checks.
The unchanged QGC/Dashboard retain their preceding 414/491 results. Actual recorded CSRT → existing publisher → SIH:
263 successful publications, zero failures, successful target acquisition after switching
to Chase and Visual Centering, and confirmed Hold after each Stop. The preceding restart check returned ready in 7.559 seconds, restored the block,
retained sidecars and resumed no operation. This does not qualify moving-object accuracy
or visual convergence. See [the checkpoint](SLICE-4B-RECORDED-SIH.md).

## Short operator test

1. With commands blocked, acquire a high-contrast feature using CSRT. A drawn box
   can work better than a single tap on this moving footage. Retarget, then Stop.
   Smart is available in Options; CPU inference can be slower than video.
2. Verify the simulated aircraft, take off to about **10 m**, and wait for airborne
   state and the normal safety margin. Acquire a fresh tracked target. Explicitly
   unblock commands in Options and hold Start. Expect **SIH following**. Position
   turns toward image error while maintaining position; it does not pursue or
   descend toward the image target.
3. Stop: expect confirmed Hold. While following is inactive, deliberately select
   Chase or Visual Centering/Distance, select a new target, and start again.
   Confirm that acquisition works after each follower change and after Stop, including
   a return to Position. These modes can
   translate. A lost/prediction-only target may prevent startup or end following
   under its configured policy. This allowance bypasses neither freshness nor
   ordinary local-follower recovery settings.
4. Finish stopped, re-enable the command block, land/disarm the simulator and stop
   tracking/detection. Backend Restart should reconnect to a new ready runtime
   with the command block enabled and no resumed tracking/following.

The footage is independent of SIH pose. Check commands and aircraft response,
not visual convergence. EOF looping changes source epoch and invalidates the old
target; select again after the loop. No gain-tuning or physical-flight claim is
made by this test.

Logs: `recorded-sih-operator-v21/logs/`, including `pixeagle.log`, historical backend
logs, `security-audit.jsonl` and `traces/`; QGC:
`recorded-sih-operator-v21/qgc-desktop/qgc.log`. Report approximate times and the
selected tracker/follower so these can be correlated with your observations.

Close QGC, stop the optional Dashboard server, then Ctrl-C terminal 1 to stop its
owned containers. The launcher cleans orphaned owned stacks; it refuses unrelated
listeners or a stack still owned by a live launcher. Configuration manifests are
immutable: saving backend configuration is supported, but prepare a fresh profile
before a later relaunch instead of editing the manifest.
