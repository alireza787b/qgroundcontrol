# Final recorded-video/SIH handoff — 2026-10-04

This is the last camera-free qualification profile before the physical camera
and onboard Pi checkpoint. It uses an isolated, launcher-owned PX4 SIH stack;
it cannot prove visual closed-loop convergence or real aircraft safety.

## Automated evidence

The fresh `recorded-sih-final-v22` profile used the longer `test11.mp4`, local
CSRT and `mc_velocity_chase`. Its command-blocked startup probe passed. The
authenticated CSRT → following → Stop probe recorded **169/169 successful
Offboard publications**, an independently observed **−163.93° simulated yaw
change**, and **confirmed Hold** after Stop. The circuit breaker was restored
and the simulator landed during cleanup.

Evidence is under:

`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/recorded-sih-final-v22/logs/`

The separate `recorded-smart-final-v24` profile loads the installed
`visdrone9m.pt` model with the Full-AI interpreter and produced live detections
from `test9.mp4`. Automated clicks were rejected when the detection snapshot
and the displayed frame no longer matched; this is the intended stale-frame
guard, not a successful Smart selection claim. Use the fresh QGC handoff below
for the operator click test. The profile's Smart logs are under:

`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/recorded-smart-final-v24/logs/`

## Final QGC operator check

The v24 credentials are in [credentials.json](/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/recorded-smart-final-v24/credentials.json).
Start the owned stack:

```bash
cd /home/alireza/PixEagle-qgc-integration
bash tools/run_gimbal_sih_probe.sh --hold \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/recorded-smart-final-v24
```

In another terminal:

```bash
cd /home/alireza/qgroundcontrol-pixeagle
PIXEAGLE_QGC_BINARY=$PWD/build/pixeagle-custom-debug/Debug/PixEagle-QGroundControl \
  bash custom-pixeagle/validation/open-gimbal-sih-qgc.sh \
  /home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/recorded-smart-final-v24
```

Sign in with the linked credentials, verify the simulated vehicle, and keep
**Block PixEagle flight commands** enabled for the tracker-only check. In
Options, choose Smart, wait for a visible detection, click the target once,
retarget once, and Stop. Then switch to Classic, draw/select a target, and
Stop. For the optional SIH-following check, take off in the simulator, clear
the block only after QGC shows the verified aircraft, hold Start, observe
`SIH following`, and press Stop. Finish with the block enabled and the vehicle
landed. The longer v22 CSRT SIH result already covers the command-publication
path; this v24 session is primarily the Smart and operator/UI check.

No physical camera is required for this handoff. Close QGC and press Ctrl-C in
the launcher terminal; its owned containers are cleaned automatically.
