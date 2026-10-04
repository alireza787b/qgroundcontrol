# PixEagle camera and gimbal scenarios

**Status:** Camera controls/tracking were operator accepted; follower repairs
have automated SIH evidence and await a fresh physical camera retest. Camera-free
normal-follower and network/load checks can proceed independently. A working stream or accepted command does not prove axis direction,
tracking quality, mount calibration, or aircraft response.

Video source, target engine, camera control owner, and aircraft follower are
separate choices. Local Classic/Smart remains the ordinary workflow. A camera
mounted on a gimbal does not require selecting the Gimbal target engine.

Fresh defaults use recorded test4 video, local CSRT, disabled camera provider
and controls, the yaw-only `mc_velocity_position` follower, altitude safety
enabled and PixEagle aircraft commands blocked. Changing an explicit camera
profile is a Dashboard configuration decision; it does not change these
ordinary defaults or add a QGC installation-settings store.

The two supported installation profiles, network card, credential policy, and
the Raspberry Pi/gimbal commissioning boundary are recorded in
[Deployment profiles](DEPLOYMENT-PROFILES.md). Keep that document as the
profile-level reference; this page describes the camera/tracker behavior.

## Common QGC connection setup

These camera changes assume the matching authenticated integration backend.
Enable PixEagle explicitly in QGC and use its backend API address, normally
port 5077; the prepared SIH profile uses 8096. The web Dashboard normally uses
port 3040. The input RTSP URL belongs in PixEagle video configuration, not the
QGC companion-address field. Native QGC requires the browser-session profile;
ordinary `local_compat` is unsupported. The checked-in backend is loopback-only
by default; remote endpoints require the approved authenticated HTTPS/trusted-
LAN deployment described in [Deployment profiles](DEPLOYMENT-PROFILES.md).
See [connection setup](README.md#connect-a-companion).

Tracking/video can run without an aircraft. Aircraft following additionally
requires verified association and the existing readiness/command-block checks.
No camera/engine configuration automatically starts aircraft following.

## Choose the workflow

| Goal | Target engine | Camera control | Follower |
| --- | --- | --- | --- |
| PixEagle tracks objects from RTSP | Local Classic or local Smart | Optional PixEagle controls, or camera application | Backend-advertised compatible image follower |
| Camera firmware tracks objects and supplies angles | Camera/Gimbal | Optional PixEagle camera selection, or camera application | Compatible `gm_velocity_chase` or `gm_velocity_vector` |
| Camera application moves the gimbal; PixEagle processes video | Local Classic or local Smart | `CONTROL_ENABLED: false` | Compatible image follower, subject to camera geometry qualification |

The backend now owns one camera provider independently of the selected tracker.
The Gimbal tracker consumes that provider's samples; manual controls use the
same provider and transport. Switching to a local tracker no longer requires
constructing a second listener to keep manual camera controls available.
Unsupported controls remain absent.

## RTSP plus PixEagle local tracking

Configure the source and keep a local tracker:

```yaml
VideoSource:
  VIDEO_SOURCE_TYPE: RTSP_OPENCV       # or a qualified RTSP_STREAM pipeline
  RTSP_URL: rtsp://CAMERA_HOST:554/STREAM_PATH

Tracking:
  DEFAULT_TRACKING_ALGORITHM: CSRT

# Strict RTSP-only operation: no camera provider/control sockets.
GimbalTracker:
  ENABLED: false
  CONTROL_ENABLED: false

Follower:
  FOLLOWER_MODE: mc_velocity_position

FOLLOWER_CIRCUIT_BREAKER: true
```

Select another Classic tracker from the advertised catalog, or select local
Smart and an installed compatible model in QGC. Local Smart executes the
PixEagle model; its reported device and fallback describe that execution.
Camera firmware Smart is a different capability and uses no local model choice.

Use **Tracking runs on: PixEagle** in Advanced settings to apply/save the local
engine. Choose a compatible normal follower explicitly; switching engines does
not silently replace it. Restart the backend for provider/video configuration
changes while following/tracking are inactive. The authoritative backend
[camera-workflows guide](/home/alireza/PixEagle-qgc-integration/docs/trackers/06-integration/camera-workflows.md)
explains optional controls, external ownership and mounting boundaries.

To let PixEagle/QGC also move the currently supported Ethernet camera, configure:

```yaml
GimbalTracker:
  ENABLED: true
  CONTROL_ENABLED: true
  PROVIDER: topotek_sip_udp
  UDP_HOST: CAMERA_HOST
  UDP_PORT: 9003
  LISTEN_PORT: 9004
  CONNECTION_TIMEOUT: 5.0
  TRACKING_STATUS_TIMEOUT: 2.0
```

Keep `Tracking.DEFAULT_TRACKING_ALGORITHM: CSRT` (or the chosen local tracker).
The existing `GimbalTracker` group remains the sole provider configuration;
this slice does not rename keys or create a second camera configuration store.
Changing provider/transport settings requires a backend restart. Native settings
reports that requirement rather than pretending a tracker restart recreated the
camera transport. `CONTROL_ENABLED: false` remains the shipped default.

When controls are enabled explicitly, the provider starts independently of the
local tracker. Fresh defaults disable both provider and control; an explicit
installation enables only the capabilities it needs.

Choose a follower compatible with the aircraft and target output, such as an
advertised `mc_*` profile for a multirotor. `gm_*` requires gimbal-angle input,
not a normal image bounding box. Moving a camera changes the image-to-aircraft
geometry; an image follower is not qualified for arbitrary gimbal motion by
this software checkpoint. Stop aircraft following before camera movement.

## Camera-owned tracking

Use this when the camera's firmware owns detection/acquisition and reports
fresh angle and target-lock samples:

```yaml
Tracking:
  DEFAULT_TRACKING_ALGORITHM: Gimbal

GimbalTracker:
  ENABLED: true
  CONTROL_ENABLED: true               # false when the camera application owns control
  PROVIDER: topotek_sip_udp
  UDP_HOST: CAMERA_HOST
  UDP_PORT: 9003
  LISTEN_PORT: 9004
  CONNECTION_TIMEOUT: 5.0
  TRACKING_STATUS_TIMEOUT: 2.0
  MOUNT_TYPE: VERTICAL                # HORIZONTAL for a horizontal installation
  COORDINATE_SYSTEM: GIMBAL_BODY
  DISABLE_ESTIMATOR: true

VideoSource:
  VIDEO_SOURCE_TYPE: RTSP_OPENCV
  RTSP_URL: rtsp://CAMERA_HOST:554/STREAM_PATH

Follower:
  FOLLOWER_MODE: gm_velocity_chase     # choose a compatible profile after qualification
```

The provider advertises camera selection modes and whether each accepts points
or rectangles. The current Topotek adapter advertises **Camera Classic** and
**Camera Smart**. These are camera firmware modes, separate from PixEagle's
local Classic algorithms and installed Smart models.

Native selection requires the displayed retained frame, matching source and
orientation. The current Topotek source guard still requires the RTSP and
control host to match. An unrelated QGC RTSP view cannot be used as qualified
PixEagle selection imagery. New providers must implement their own source
association rather than inheriting guessed IP or coordinate rules.

For camera-application control, set `CONTROL_ENABLED: false` and select Gimbal
through the saved backend configuration or dashboard. Monitoring remains
available to the Gimbal tracker; PixEagle does not send manual control commands.
Native camera selection/mode buttons are unavailable in this read-only setup.
Camera application traffic is outside PixEagle's client lease arbitration.

Following requires fresh angles **and** a fresh tracking-active report.
`gm_velocity_chase` and `gm_velocity_vector` consume `GIMBAL_ANGLES`. Mount
orientation, native axis signs, coordinate frame and target retention require
the physical bench checkpoint. The accepted vertical installation has recorded
operator direction observations; horizontal physical acceptance and arbitrary
mounts are not implied. `MOUNT_TYPE` chooses the shared geometry preset; leave
`GEOMETRY_OVERRIDE` fields at AUTO/zero unless provider characterization
requires an explicit expert correction. Image rotation is a separate setting.

## Camera application plus local PixEagle tracking

Use the RTSP/local configuration above and leave `CONTROL_ENABLED: false`.
The camera application owns movement; PixEagle processes incoming frames.
Do not select Gimbal solely to expose controls, start a second listener, or let
two applications issue competing motor commands. If the camera application owns
tracking as well and PixEagle should consume its angles, use the camera-owned
scenario instead.

## Operational follower choices

Set `Follower.FOLLOWER_MODE` in backend configuration, or select an advertised
compatible follower in operational Options. Engine selection, target acquisition
and follower selection never start aircraft following automatically.

| Follower | Required target output | Intended behavior / current gate |
| --- | --- | --- |
| `mc_velocity_position` | Local image position | Holds horizontal position and rotates toward target; ordinary default |
| `mc_velocity_chase` | Local image position | Forward chase with compatible guidance; synthetic SIH passed |
| `mc_velocity_ground` | Local image position | Downward/oblique-view following; synthetic SIH passed |
| `mc_velocity_distance` | Local image position | Distance/position guidance; synthetic SIH passed, physical distance calibration separate |
| `mc_attitude_rate` | Local image position | Direct body-rate control; sustained attitude-envelope release blocker |
| `gm_velocity_chase` | Camera angles plus fresh target state | Camera-angle chase; both synthetic mount presets passed |
| `gm_velocity_vector` | Camera angles plus fresh target state | Vector guidance; fresh default coordinated turn, sideslip remains advanced |
| `fw_attitude_rate` | Compatible local image position and qualified speed | Fixed-wing live qualification/speed acquisition remain blocked |

Ordinary altitude safety stays enabled with
`Safety.GlobalLimits.ALTITUDE_SAFETY_ENABLED: true`. Optional altitude guidance
uses `Follower.General.ENABLE_ALTITUDE_CONTROL`, or a sparse
`Follower.FollowerOverrides.<UPPERCASE_FOLLOWER>.ENABLE_ALTITUDE_CONTROL` value.
The ordinary default is false; both GM overrides are true. For example, enabling
only Position climb/descent uses the existing override path:

```yaml
Follower:
  FollowerOverrides:
    MC_VELOCITY_POSITION:
      ENABLE_ALTITUDE_CONTROL: true
```

Merge this into the current configuration; do not replace sibling overrides.
Changing altitude guidance never disables altitude safety. Keep
`FOLLOWER_CIRCUIT_BREAKER: true` for setup/ground tests; native unblocking requires
explicit confirmation and does not start following. Command Preview is a distinct
Dashboard test execution mode, not actual aircraft following.

## Optional fixed-wing ground-speed fallback

This rare setup option belongs in PixEagle's web Dashboard configuration under
**Fixed-Wing Attitude Rate**. It adds no QGC setting or operational switch:

```yaml
FW_ATTITUDE_RATE:
  ALLOW_GROUND_SPEED_FALLBACK: false
```

Enable it explicitly to use fresh aircraft ground velocity when airspeed is
unavailable. Available valid airspeed keeps priority; a low airspeed reading
cannot be bypassed by the fallback. Cached/missing ground velocity remains
unavailable. Runtime telemetry labels the selected speed/source as a proxy and
leaves measured airspeed unavailable. Config save/sync preserves the operator's
choice; the schema reports the existing follower-restart application tier.

The proxy uses the existing fixed-wing guidance assumptions and speed limits.
It does not measure aerodynamic stall margin. Default remains false; gains and
ordinary camera/follower defaults are unchanged. Full fixed-wing live SIH and
airframe qualification remain open; enabling it does not remove the existing
live-profile qualification gate.

PX4 separately supports [validated synthetic airspeed](https://docs.px4.io/v1.17/en/sensor/airspeed)
from ground velocity corrected by estimated wind. In PX4 v1.17,
[VFR_HUD](https://github.com/PX4/PX4-Autopilot/blob/v1.17.0/src/modules/mavlink/streams/VFR_HUD.hpp)
reports unavailable airspeed for non-sensor sources, so choosing synthetic
speed alone does not make it available through the current HUD path. Typed
measured/synthetic acquisition is a separate remaining task. The
[sensorless VTOL guide](https://docs.px4.io/main/en/config_vtol/vtol_without_airspeed_sensor)
addresses position-controlled flight; it does not qualify this raw-rate follower.

## Control and restart behavior

QGC uses its native thumb-pad visual in a movable, nonmodal camera panel.
Drag and hold for pan/tilt; displacement selects speed and the dominant axis.
Manual input uses a renewable gesture, with one latest intent and a camera
executor independent of local AI and capture. The client renews every 100 ms;
the backend expires input after 350 ms. The first update after Begin permits
movement. Normal holds have no arbitrary five-second cutoff. The existing
provider socket remains the sole transport owner.

At takeover, the camera reports tracking disabled before movement starts.
Tracking does not resume automatically. Zoom remains adjacent; supported roll
controls are always below the pad. The disclosure shows observed camera angles
with freshness and coordinate frame; zoom telemetry is shown only when provided.
Release, neutral, focus loss or closing the panel stops the gesture. Target
selection remains available while the panel is open and idle. An active manual
gesture takes ownership and disables camera tracking once; release ends movement
but does not automatically restart tracking. Camera/source/owner changes,
expired input and failed transmission require a fresh gesture.

Stop retains the original camera identity, works without fresh imagery, and
invalidates previously queued native movement guards. It never transfers a
release to a replacement camera. Dashboard and QGC share backend arbitration;
interleaved clients cannot acquire one active movement hold. Dashboard and QGC
use the same renewable manual interface when advertised. Only older providers
retain finite steps and their defensive hold limit.

Camera status, target state, following state and aircraft flight mode remain
separate. A successful command means its software path ran; physical movement
or target acquisition still requires fresh observed telemetry/video. UDP Stop
is best effort. Network loss or process death requires device-side protection.

No aircraft commands are part of this camera bench checkpoint. At 4b.3, record the exact
camera model/firmware, Ethernet topology, authenticated RTSP shape, mounting
orientation and safe bench limits before checking each axis and camera tracker.
Windows/Android and release qualification remain slice 5.

## References

- [Native camera contract](../../PixEagle-qgc-integration/docs/apis/native-camera-controls.md)
- [Gimbal tracker reference](../../PixEagle-qgc-integration/docs/trackers/02-reference/gimbal-tracker.md)
- [External provider guide](../../PixEagle-qgc-integration/docs/trackers/05-development/external-gimbal-providers.md)
- [Tracker schemas](../../PixEagle-qgc-integration/configs/tracker_schemas.yaml)
- [Configuration defaults](../../PixEagle-qgc-integration/configs/config_default.yaml)
- [Deployment profiles](DEPLOYMENT-PROFILES.md)
