# Session speed override

Supported active followers advertise a **Requested speed** row beside follower
status. The first implementation is GM Velocity Vector; clients use backend
capabilities and do not infer support from a follower name. Older backends and
unsupported followers show no row. No QGC Settings value is added.

The compact row uses a speedometer, minus, an acknowledged numeric readout, plus
and restore. Tap the readout for an inline exact-speed entry and Apply/Cancel;
Stop remains outside the scrollable content and reachable while editing. A
changed session/revision or panel gesture invalidates the captured edit. Pending
requests retain the acknowledged value. The Options dialog uses measured layout
dimensions and font/control sizing at the panel scale, rather than an unmeasured
visual transform, so its scrolling and Close remain aligned.

Each minus/plus activation requests one backend-advertised step. It sets a
temporary total 3D speed and replaces PixEagle's saved total and per-axis velocity
caps for that session. There is no additional QGC speed ceiling. Altitude
protection, acceleration, yaw/rate limits, Stop, circuit breaker and aircraft
firmware limits remain unchanged. Requested speed is not measured aircraft speed.

The highlighted restore icon removes the velocity override. It restores saved
limits and keeps or lowers the request; it never raises speed. Zero ramps down
translation while tracking and permitted yaw continue. **Stop** ends following.
Target recovery permits reductions and restore, and pauses increases without
renewing time/travel budgets.

Dashboard and QGC use the same authenticated `following-control` action and
authoritative following status. Requests carry absolute value (or `null` for
restore), revision and backend/runtime/session/aircraft/connection identity.
Press context is captured before activation; stale presses, panel movement,
resize and changed sessions cannot redirect an old action. Only acknowledged
values are shown. Stop can preempt a pending adjustment.

## Configuration

`GM_VELOCITY_VECTOR.REQUESTED_SPEED` is an optional saved startup request. Missing
or `null` retains ceiling-based startup within configured limits; it does not
automatically activate an override. `INITIAL_VELOCITY` remains the ramp initial
condition. `RAMP_ACCELERATION` remains its existing acceleration/deceleration
setting. The override and restore never edit those settings.

Saved limits remain in `Safety.GlobalLimits` / `Safety.FollowerOverrides`.
SafetyManager resolves the owned live velocity override at guidance, setpoint
validation and final PX4 dispatch. It holds a weak provider reference, not a
second saved speed value. Restore and session teardown remove that binding.
Altitude is relative to the backend estimator/home reference, not terrain
clearance. Installation and advanced configuration remain in Dashboard.

## Evidence and operator gate

The local validation root is
`~/.cache/pixeagle-qgc-baseline/session-speed-2026-10-22/`.
It retains passing and failed runs, raw logs, checksums, configuration and exact
driver/source receipts. Failed response-window assumptions are not deleted.
The private Desktop handoff contains selected plots, CSV and both successful
mount runs. These are synthetic target / PX4 SIH evidence, not physical flight
qualification or evidence for a new pursuit law.

The reusable commands are:

```bash
python3 custom-pixeagle/validation/run-gimbal-sih-evidence.py \
  --execute --session-controls --backend-repo /path/to/PixEagle \
  --follower gm_velocity_vector --mount VERTICAL --output /new/evidence/directory

python3 custom-pixeagle/validation/plot-session-speed-evidence.py \
  /successful/evidence/directory --output /review/plots
```

The plot exporter needs matplotlib only in the validation environment. The SIH
driver accepts explicit Python/binary/library paths; its cache defaults are
development conveniences. It starts owned, pinned Docker containers with no
external network and exposes no host ports.

After software checks, build Windows first with one manual
`pixeagle-platform.yml` dispatch. Keep the accepted prior package intact. Bench
acceptance uses the cooled Pi/camera with the circuit breaker active. Confirm
shared values in Dashboard/QGC, restore, zero, retargeting, reconnection and Stop.
Aircraft acceptance is a separate checkpoint. No public custom-QGC PR or release
is authorized here.
