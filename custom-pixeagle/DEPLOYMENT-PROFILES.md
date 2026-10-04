# PixEagle deployment profiles

This document defines the two supported installation profiles for the
PixEagle–QGroundControl integration. It keeps the ordinary operator workflow
simple while recording the explicit settings needed for the gimbal bench.

The checked-in PixEagle configuration is the single source of truth. A profile
is an approved set of changes applied through PixEagle Config Sync or the web
Dashboard. It is not a second configuration file, a QGC-local settings store,
or a collection of hidden defaults. Hardware and one-time installation values
belong in the PixEagle configuration; QGC exposes only the operational choices
needed during a flight.

## Profile A — ordinary Raspberry Pi deployment

This is the profile intended for most users. It is suitable for a Raspberry Pi
with a USB, CSI, or RTSP camera and a QGC laptop on the same approved network
path. The first repeatable demonstration uses the bundled video file; a live
camera is an explicit source change after the board has been commissioned.

| Area | Default intent |
| --- | --- |
| Video source | `VIDEO_FILE`, `resources/test4.mp4`, real-time pacing |
| Live camera alternatives | `USB_CAMERA`, `CSI_CAMERA`, or qualified RTSP OpenCV input |
| Tracking engine | PixEagle/local tracking; Classic `CSRT` at startup |
| Smart tracking | Optional installed local model; never silently replaces Classic |
| Gimbal provider/control | Disabled |
| GStreamer input/output | Disabled; Dashboard HTTP/WebSocket/WebRTC remains independent |
| Follower | `mc_velocity_position` (yaw-only constant-position behavior) |
| Altitude commands | Disabled by default; altitude safety envelope remains enabled |
| Global safety | Altitude limits, emergency stop, and violation handling enabled |
| Circuit breaker | Enabled (`FOLLOWER_CIRCUIT_BREAKER: true`) |
| Following | Requires verified vehicle association and explicit operator start |

The profile is safe to inspect and track without an aircraft. It never turns a
recorded video demo into aircraft following. For a deployed live source, change
only the source and camera-specific settings in the Dashboard, for example:

```yaml
VideoSource:
  VIDEO_SOURCE_TYPE: RTSP_OPENCV       # or USB_CAMERA / CSI_CAMERA
  RTSP_URL: rtsp://camera-host:554/stream=0
  USE_GSTREAMER: false

Tracking:
  DEFAULT_TRACKING_ALGORITHM: CSRT

GimbalTracker:
  ENABLED: false
  CONTROL_ENABLED: false

Follower:
  FOLLOWER_MODE: mc_velocity_position

FOLLOWER_CIRCUIT_BREAKER: true
```

`USE_GSTREAMER: false` is deliberate for the ordinary profile. It avoids
requiring a board-specific multimedia stack. If a CSI or USB driver on a
particular board requires GStreamer, that becomes an explicitly qualified
board profile; it does not change the factory defaults.

## Profile B — gimbal camera laboratory deployment

This is an expert, bench-only profile for the Ethernet camera and gimbal used
in the current qualification. It must be selected explicitly in PixEagle
configuration and is never inherited by a new installation.

```yaml
VideoSource:
  VIDEO_SOURCE_TYPE: RTSP_OPENCV
  RTSP_URL: rtsp://192.168.0.108:554/stream=0
  RTSP_PROTOCOL: tcp
  RTSP_LATENCY: 200
  USE_GSTREAMER: true                 # input-path acceleration for this board

Tracking:
  DEFAULT_TRACKING_ALGORITHM: Gimbal  # camera-owned Classic/Smart modes

GimbalTracker:
  ENABLED: true
  CONTROL_ENABLED: true
  PROVIDER: topotek_sip_udp
  UDP_HOST: 192.168.0.108
  UDP_PORT: 9003
  LISTEN_PORT: 9004
  MOUNT_TYPE: HORIZONTAL              # change to VERTICAL for the bench mount
  COORDINATE_SYSTEM: GIMBAL_BODY
  DISABLE_ESTIMATOR: true

Follower:
  FOLLOWER_MODE: gm_velocity_vector
  FollowerOverrides:
    GM_VELOCITY_VECTOR:
      ENABLE_ALTITUDE_CONTROL: false  # bench intent; no vertical setpoints

FOLLOWER_CIRCUIT_BREAKER: true        # retain until the SIH/ground gate passes
```

`gm_velocity_vector` is the current laboratory default. `gm_velocity_chase`
remains available when its guidance law is selected. `MOUNT_TYPE` selects the
shared geometry preset; vendor-specific signs or offsets belong in
`GEOMETRY_OVERRIDE` after a camera-only characterization. They are not guessed
from the image rotation.

Disabling altitude *command generation* in this bench profile does not remove
the global safety envelope, telemetry freshness checks, or the circuit breaker.
The current default velocity envelope is deliberately conservative. Raising a
lab profile's speed limit to 2 m/s or more requires a reviewed SIH/props-off
test and must remain an explicit override; it must never be copied into the
ordinary profile or treated as flight approval.

The camera-owned engine and PixEagle/local engine are independent choices. To
use the gimbal's firmware tracker, keep `DEFAULT_TRACKING_ALGORITHM: Gimbal`
and select Camera Classic or Camera Smart. To use PixEagle tracking while still
moving the gimbal, select the PixEagle engine, keep the provider/control enabled,
and choose an image-compatible `mc_*` follower. The provider remains the one
socket owner; no second listener or competing motor-control path is created.

## Network and port card

The following addresses come from the controlled bench manual and are a
deployment reference, not a claim that the current laptop is connected to all
of them.

| Device/service | Address or port | Use |
| --- | --- | --- |
| Raspberry Pi | `192.168.0.226/24` | PixEagle host |
| Ethernet camera | `192.168.0.108/24` | RTSP and Topotek UDP control |
| GCS workstation (manual) | `192.168.0.150/24` | QGC operator station |
| Ground datalink | `192.168.0.250/24` | Ground-side MAVLink link |
| Air datalink | `192.168.0.251/24` | Air-side MAVLink link |
| Dashboard | TCP `3040` | Browser UI |
| PixEagle API/video | TCP `5077` | Authenticated backend and native QGC API |
| MAVLink2REST | TCP `8088` | Local telemetry bridge by default |
| Legacy telemetry WebSocket | TCP `5551` | Only when enabled by the deployment |
| Gimbal provider | UDP `9003` / listen `9004` | Explicit gimbal profile only |
| Optional QGC H.264 output | UDP `5600` | Only when `ENABLE_GSTREAMER_STREAM` is enabled |

The normal launcher resolves the Dashboard port from its environment and
prints the actual value. `3040`, `5077`, `8088`, and `5551` are the shipped
defaults; a changed port must be recorded in the deployment card and supplied
to QGC rather than guessed.

The Pi may use Wi-Fi for Internet or hotspot access while Ethernet remains on
the robot router. The final board commissioning must verify route metrics and
firewall rules so camera/MAVLink control traffic stays on the robot LAN and is
not accidentally forwarded over Wi-Fi. This dual-interface behavior has not
yet been hardware-qualified.

The checked-in backend uses `API_EXPOSURE_MODE: local_only` and loopback binds
by default. That is correct for a same-host demo. A QGC laptop connecting to a
remote Pi needs an explicitly approved authenticated HTTPS/reverse-proxy or
trusted-LAN deployment; do not expose the loopback service with arbitrary port
forwarding or forwarded headers. Remote restart remains administrator-only and
is not advertised until that HTTPS path is verified.

## Credentials and first connection

There is no universal production password. The supplied image/release card or
the setup credential handoff is authoritative. Some local developer setup paths
offer `admin/admin` when the operator deliberately accepts the convenience
choice, but that value must not be assumed for a field board and should be
changed on first commissioning. Generated browser-session credentials are the
preferred handoff for a networked Pi. QGC's saved-password option uses the
platform credential store; it does not create a second PixEagle account.

On first connection QGC verifies the single unambiguous vehicle association
automatically after authentication. A duplicate, conflicting, stale, or
missing vehicle identity leaves the association pending and asks the operator
to verify it; it never guesses across multiple vehicles.

## Commissioning sequence

1. Record the Pi image/version and SHA256, camera model/firmware, router and
   datalink addresses, and the chosen profile in the deployment card.
2. Start PixEagle with the owned supervisor and confirm the Dashboard, backend,
   and telemetry bridge on their actual printed ports.
3. Sign in with the supplied/generated credential and confirm the backend
   runtime/configuration identity. Apply profile changes while tracking and
   following are inactive, then restart when the configuration service says the
   provider or source requires it.
4. In QGC enter the authenticated backend address, allow automatic single-
   vehicle verification to complete, and confirm the aircraft identity before
   enabling any aircraft command path.
5. For a normal profile, test video/tracking first and leave the circuit breaker
   enabled. For the gimbal profile, test camera movement and angle telemetry
   with props off or command-blocked SIH before any aircraft command test.
6. Keep the QGC and Dashboard logs, the effective configuration snapshot, and
   the operator observations together. A software publication success does not
   prove motor motion, visual convergence, or safe flight behavior.

## Qualification status and next hardware gate

The normal local/video, QGC integration, camera-control, gimbal SIH, and
recorded-video follower checkpoints have automated evidence. Network/load
fixtures cover the shared relay and reconnect behavior but do not qualify a
radio, Raspberry Pi scheduler, real camera, or motor response. Windows/Android
release packaging and the Pi/router/PX4 ground checkpoint remain open.

No camera or SSH action is required for the current software work. The next
operator action is needed when the Pi and camera are available: capture the
actual board inventory, install the selected profile, verify the dual-interface
routes, and run the short camera-only and command-blocked SIH handoff. Real
hardware following remains gated on those results.

The public/release boundary is recorded in
[Publication status](PUBLICATION-STATUS.md). This customized QGC build is a
private qualification artifact for now; do not publish its binaries, private
credentials, or local handoff logs as if they were an official QGC release.

## Source manual

The deployment values above were reconciled with the controlled bench manual:

`/home/alireza/Downloads/PB-SW-MAN-001_OPERATOR_v1.0.0_2026-07-25 (2).pdf`

The manual is a bench commissioning document. It requires the supplied image
and credential card, treats `192.168.144.108` as an isolated camera-recovery
address only, and does not authorize flight. Its unresolved image/package,
datalink-evidence, and camera-compatibility items remain release-card work.

Related integration details:

- [Camera scenarios](CAMERA-SCENARIOS.md)
- [QGC connection setup](README.md#connect-a-companion)
- [Slice 4b acceptance status](STATUS.md)
- [PixEagle configuration defaults](../../PixEagle-qgc-integration/configs/config_default.yaml)
- [PixEagle native camera contract](../../PixEagle-qgc-integration/docs/apis/native-camera-controls.md)
