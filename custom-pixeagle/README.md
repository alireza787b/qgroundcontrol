# PixEagle custom build

This overlay adds optional native PixEagle connection setup to QGC. The plugin
inherits QGC's standard UI, firmware factories, mission tools, multi-vehicle
support, and flight controls. PixEagle is off by default; while off it starts no
companion connections or polling and adds no Fly View controls or reserved space.

Slice 1 provides sign-in and verified aircraft association. Slice 2 adds
authenticated video and read-only status, including a companion-only connection
without PX4. Slice 3 adds guarded target selection, retargeting, tracker choice,
and tracking-only Cancel. Slice 4a adds follower choices and guarded Start/Stop
controls, with recorded PX4 SIH validation and physical-flight qualification still open. See the
[slice-4a checkpoint](SLICE-4A.md) for its evidence and remaining gate, and the
[slice-3 checkpoint](SLICE-3.md) for validation progress and the tracker demo gate;
the [slice-2 checkpoint](SLICE-2.md) preserves the completed video baseline.
See the [operator coverage plan](OPERATOR-COVERAGE.md) for the explicit Smart
model, external-camera, gimbal movement, following and administration boundaries.
The [current status](STATUS.md) records accepted workflows and remaining gates.
The [publication status](PUBLICATION-STATUS.md) explains that this customized
QGC branch is not a public release; the public PixEagle project and its Apache
2.0 notice remain separate.
The [shared restart and recorded-video checkpoint](SLICE-4B-RESTART.md) records
the latest camera-free validation; an optional
[recorded SIH session](OPERATOR-RECORDED-SIH.md) is prepared. Final camera hardware
acceptance is postponed until the operator returns.
The [feedback checkpoint](SLICE-3B-FEEDBACK.md) preserves the
accepted **test4 demo**, direct targeting and simplified Classic/Smart controls.
The [slice-3b checkpoint](SLICE-3B.md) preserves earlier model/runtime validation.
The approved [next-phase plan](NEXT-PHASE-PLAN.md), [camera scenarios](CAMERA-SCENARIOS.md),
and [4b.0 Smart operator test](OPERATOR-TEST-4B-0.md) define the current
local-versus-camera-owned workflow boundary. The Ethernet camera is not needed
for 4b.0. The [settings and camera checkpoint](SLICE-4B-1-2.md) records 4b.1–4b.2 implementation and validation before the 4b.3 Ethernet/RTSP bench.

The overlay is selected explicitly; a normal configure continues to build stock
QGC. Use separate build directories because custom definitions and branding are
configure-time choices. QGC's normal custom-build behavior disables its upstream
stable-version update check.

## Linux development

Use `python3 tools/setup/install_python.py dev --python /usr/bin/python3` on
Ubuntu 24.04 to install the locked tools, then activate `tools/.venv`.
Install Qt 6.11.1 with the modules in
`.github/build-config.json` **plus `qttasktree`**, needed by Qt 6.11.1's QML asset
downloader. Set `QT_ROOT_DIR` to its `gcc_64` directory. Install QGC's Linux system
dependencies using its setup guide, or use an isolated build container.
The recorded Linux environment includes `libopengl-dev`, needed for CMake's EGL
target discovery. See [the pinned container recipe](validation/Dockerfile).

For the recorded local container, prefix the CMake commands below with
`custom-pixeagle/validation/run-container.sh`. The wrapper mounts this checkout,
the baseline cache, and `uv`; it uses the exact image ID from the checkpoint.
Override `QGC_BASELINE_CACHE` or `QGC_BASELINE_IMAGE` for another installation.
The recorded image can be restored with `docker load -i container-image.tar.gz`.
To build a fresh image from the pinned direct package versions:

```bash
docker build -t pixeagle-qgc-baseline:local custom-pixeagle/validation
export QGC_BASELINE_IMAGE=pixeagle-qgc-baseline:local
```

Compare its packages against `validation/container-packages.tsv`; distribution
repositories may change transitive packages or retire pinned versions. The
recorded image archive preserves the exact validated environment.

```bash
cmake --preset Linux-debug -B build/pixeagle-stock-debug \
  -C custom-pixeagle/cmake/BaselineDependencies.cmake
cmake --build build/pixeagle-stock-debug --parallel 10

cmake --preset Linux-debug -B build/pixeagle-custom-debug \
  -C custom-pixeagle/cmake/BaselineDependencies.cmake \
  -DQGC_CUSTOM_DIR=custom-pixeagle
cmake --build build/pixeagle-custom-debug --parallel 10
```

The custom build fetches QtKeychain 0.16.0 at
`aa6da344e1a20b9194e12bace3665caeea6b6304` for the system credential
store. The Linux build uses the Secret Service/KWallet D-Bus backend; an
unavailable store never falls back to plaintext settings. The dependency is
linked statically into the custom executable and is absent from stock QGC.

For release binaries use the `Linux` preset and separate directories:

```bash
cmake --preset Linux -B build/pixeagle-stock-release \
  -C custom-pixeagle/cmake/BaselineDependencies.cmake
cmake --build build/pixeagle-stock-release --parallel 10

cmake --preset Linux -B build/pixeagle-custom-release \
  -C custom-pixeagle/cmake/BaselineDependencies.cmake \
  -DQGC_CUSTOM_DIR=custom-pixeagle
cmake --build build/pixeagle-custom-release --parallel 10
```

The custom executable is named `PixEagle-QGroundControl`. Recorded release boot
checks used isolated settings and no external network:

```bash
QGC_BASELINE_NETWORK=none custom-pixeagle/validation/run-container.sh \
  timeout 60s xvfb-run -a build/pixeagle-stock-release/Release/QGroundControl \
  --simple-boot-test
QGC_BASELINE_NETWORK=none custom-pixeagle/validation/run-container.sh \
  timeout 60s xvfb-run -a build/pixeagle-custom-release/Release/PixEagle-QGroundControl \
  --simple-boot-test
```

The upstream comparison uses the untouched worktree `build/upstream-source` at
`6f1f4368de08fe093291c63ac7f012c797aaf3c4`. From that worktree, configure with
`--preset Linux-debug -B build/baseline-debug` and load
`-C ../../custom-pixeagle/cmake/BaselineDependencies.cmake`. Set `QGC_PYTHON_ENV`
to the integration checkout's `tools/.venv` for that configure only; unset it
before running CTest so the Python-venv contract test can control its environment.

For each Debug build run the CI test helper from the build directory:

```bash
python /absolute/path/to/source/.github/scripts/cmake_helper.py ctest \
  --build-type Debug --include-labels 'Unit|Integration' \
  --exclude-labels 'Flaky|Network' --jobs 4 \
  --junit-output baseline-tests.xml --ctest-output baseline-tests.log
```

Keep `StockUI` tests enabled: this overlay preserves the standard interface.
The helper uses Xvfb when no display is available. Use isolated `XDG_CONFIG_HOME`,
`XDG_CACHE_HOME`, and `XDG_DATA_HOME` directories for application smoke tests.
Container example for stock Debug tests, run from the checkout root:

```bash
custom-pixeagle/validation/run-container.sh bash -c \
  'cd build/pixeagle-stock-debug && python ../../.github/scripts/cmake_helper.py \
  ctest --build-type Debug --include-labels "Unit|Integration" \
  --exclude-labels "Flaky|Network" --jobs 4 \
  --junit-output baseline-tests.xml --ctest-output baseline-tests.log'
```

## Dependency boundary

`BaselineDependencies.cmake` applies to stock and custom builds. It disables
automatic dependency refresh and pins the ArduPilot parameter repository, whose
upstream declaration otherwise follows `main`. Other dependencies use upstream's
fixed commit or version declarations. Keep the resolved dependency inventory and
tool/package versions with build evidence.

The integration branch imports the existing generic HTTP/WebSocket JPEG stack.
Those generic transports remain unauthenticated. Slice 2 adds a separate
authenticated integration path and display-frame provenance; the original
transport PR branches stay unchanged.

## Connect a companion

Use **Application Settings → PixEagle → Enable PixEagle**. With an aircraft in
QGC, select its active vehicle, enter the companion address, sign in, then select
**Verify vehicle**. Selecting another vehicle on this page also changes QGC's
active vehicle. Without an aircraft, **Companion only** allows sign-in, video,
status and permitted tracking actions. Its session never transfers to an
aircraft that connects later.
Aircraft association remains unavailable in companion-only mode.

The companion must run the matching integration backend worktree. Use HTTPS
with a trusted certificate; HTTP is accepted only on loopback for local
development. An address may include a reverse-proxy path prefix. Redirects are
rejected: enter the final address. The backend must require a real authenticated
session, and the account must have status and telemetry read access, plus
`media:read` for video. The legacy `local_compat` profile is unsupported.
Remote WebSocket deployments must allow the endpoint's HTTPS origin through
PixEagle's existing Origin policy. Loopback native connections omit Origin and
remain subject to PixEagle's loopback peer/Host checks.

Backend preferences are saved by aircraft UID or the separate companion-only
preference. Custom dashboard addresses are saved by the full backend address,
including its port and API prefix. **Remember sign-in on this device** is on
by default. After a successful sign-in, QGC saves the account and password in
the operating system's credential store, keyed by the full backend address;
there is no plaintext-settings fallback. If the store is unavailable, QGC shows
that sign-in will be needed next time. The checkbox can remove the saved entry.
Sign out suppresses automatic sign-in until the operator signs in again.
Session cookies and CSRF tokens remain in memory. Each vehicle has a separate session, including when two local
companions use different ports on the same host. Sign out before changing a
signed-in companion's address.

Verification compares QGC's aircraft UID with the companion's observed command
and fresh telemetry identities. Missing, conflicting, stale, or duplicate
identity keeps verification unavailable. A changed runtime or connection requires
explicit verification again. Merely switching between healthy verified vehicles
does not invalidate their associations. The **Connection details** disclosure
contains the identities for diagnosis.

The backend needs a unique `PIXEAGLE_INSTANCE_ID` and the correct
`MAVLINK_SYSTEM_ID` / `MAVLINK_COMPONENT_ID` configuration. Its native integration
contract documents the current MAVSDK-only telemetry limitation and routing
migration. Verification establishes identity, not flight readiness or permission
to start following. The separate following status supplies profile readiness;
Start requires a verified aircraft, fresh target and PixEagle's Offboard preflight.
Stop uses the captured follow session or pending start, even if video is lost.

**Show video in Fly View** selects the companion's video; turn it off to
return to QGC's configured camera. PiP, map/video switching and double-click
fullscreen use QGC's existing layout. The compact status and its details name the connection and show
fresh, delayed, waiting or unavailable video. Tracker/following status is polled
separately; it is not a coordinate overlay synchronized to the displayed frame.
Recording and photo capture are unavailable on this custom source. By default, tap the video to select or replace a target; Classic also accepts a
drawn box. Turn off **Tap video to select targets** to require **Select target**
or **Retarget** before each gesture. **Cancel tracking** stops the current track.
Existing stock camera preferences remain intact.

## Dashboard and help

**Dashboard** in PixEagle settings or Fly View → Options opens your system browser.
The default URL uses the backend's host and protocol, port **3040**, and `/`.
For example, a local backend at `http://127.0.0.1:5077` opens
`http://127.0.0.1:3040/`. This is a documented default, not service discovery.
The backend currently publishes no authoritative frontend URL; CORS origins and
reverse DNS cannot reliably identify its dashboard route.

For a custom port, hostname, tunnel or reverse proxy, expand **Web dashboard**,
enter the complete browser URL and **Save**. **Use default** clears that override.
For example, backend `https://companion.example/pixeagle-api` may use dashboard
`https://companion.example/pixeagle/`. The override belongs to that complete
backend address and follows the selected vehicle/companion. Changing the backend
port or prefix selects a separate preference. Browser navigation does not send
QGC's password, session cookie or CSRF token; sign in separately in your browser.
Only plain HTTP/HTTPS URLs without embedded credentials, query or fragment are
saved. This browser shortcut does not change QGC's API authentication/TLS policy.

**Connection help**, also available while integration is off, opens a short
setup guide and the [PixEagle documentation](https://github.com/alireza787b/PixEagle/blob/main/docs/README.md).
**About PixEagle** opens a standard QGC dialog with the project's published
copyright notice, [source code](https://github.com/alireza787b/PixEagle) and
[license](https://github.com/alireza787b/PixEagle/blob/main/LICENSE). These links
open only on request; there is no dashboard probing or port scan. Web dashboard
and Connection details use native disclosure headings. Checkboxes represent
only enabled/disabled preferences.

## Isolated local connection demo

`validation/run-connection-smoke.sh` launches the custom Debug app on a private
Xvfb display with two mock MAVLink vehicles and two local companions. The
companions use the production authentication and integration routes with a mocked
aircraft provider. No camera, SITL, aircraft, host network, or deployment is used.
It requires the recorded container/SDK/tools, the built Debug app, the backend
worktree and its prepared `.venv`, and the baseline's extracted Xvfb UI tools.

```bash
custom-pixeagle/validation/run-connection-smoke.sh
# Inspect the private display with the baseline's screenshot/xdotool tooling.
docker stop pixeagle-slice1-ui
```

Set `PIXEAGLE_SMOKE_BINARY` to the absolute path of the Release executable to
exercise that build. `PIXEAGLE_SMOKE_EVIDENCE` selects a separate evidence and
isolated-settings directory for each run.

For camera-free video with no aircraft, set `PIXEAGLE_SMOKE_NO_AIRCRAFT=1` and
use `http://127.0.0.1:8093`. The same disposable fixture credentials apply.
`ui/media-3.json` controls numbered synthetic frames; the backend's
`docs/apis/native-frame-provenance.md` documents freeze, drop, source, epoch,
resolution and variant changes. Normal two-vehicle runs use `media-1.json` and
`media-2.json`.

A separately prepared real Core replay can be selected with
`PIXEAGLE_SMOKE_REPLAY_DIR=/absolute/private/replay-directory`. Prepare it using
the backend's `tools/native_replay_demo.py`; it runs actual PixEagle capture and
APIs with bundled `test4.mp4`, a private viewer account and inhibited aircraft
connections. This path is distinct from the synthetic fixture. Its credentials
are in the replay directory's private `credentials.json`.

Inside that fixture only, Vehicle 1 uses `http://127.0.0.1:8091` and Vehicle 2
uses `http://127.0.0.1:8092`. Both use the disposable account `operator` with
password `fixture-only`. These are test credentials, not credentials for an
installed PixEagle service. The fixture creates temporary authentication/audit
storage and leaves the user's PixEagle configuration untouched.

This harness provides captured UI evidence; it does not open a host desktop
window. The first connection demo is ready after slice 1's gates. The first
integrated PixEagle video demo is gated on slice 2; target interaction and aircraft
following have separate later gates.

## Desktop recorded-video demo without PX4

After building custom Release and preparing a private Core replay as above,
run this from a Linux desktop terminal:

```bash
custom-pixeagle/validation/run-replay-desktop.sh /absolute/private/replay-directory
```

The launcher starts the actual Core app, verifies its identity and disconnected
aircraft state, then opens QGC with isolated settings under
`~/.cache/pixeagle-qgc-baseline/local-replay-desktop`. It disables automatic
aircraft connections in those demo settings. Closing QGC stops the backend
started by this launcher. It does not install a service or change either main
checkout. Keep the terminal open for the demo.

Open **Application Settings → PixEagle**, enable the integration and use the
backend endpoint, username and password from the replay directory's private
`credentials.json`, then choose **Open Fly View**. With Remember sign-in enabled,
the password can be saved only in the system credential store. The top toolbar's **Disconnected** message refers to aircraft; video and
permitted tracking can work without PX4. Viewer accounts remain read only.
Swap map/video with the lower-left inset; use QGC's fullscreen controls as usual.
Turn off **Show video in Fly View** to restore QGC's configured camera.

Choose **Classic** and an advertised tracker, or **Smart** and an installed model.
Tap a target (a detected object in Smart), then tap another to replace it.
Classic also accepts a drawn box. **Cancel tracking** stops tracking. Under
**Options**, disable **Tap video to select targets** to require a separate
**Select target / Retarget** action before each selection. Unavailable tracker
reasons are in Settings → Connection details. Smart requires the Full AI
runtime and compatible installed models; external-camera mode requires hardware.

The accepted Full AI replay uses `test4.mp4`, both installed VisDrone models and
all six Classic tracker assets. Test4 includes recorded tracking graphics that
are separate from native QGC target state. The earlier Core-only soccer replay
is useful for Classic tracking, but does not qualify vehicle/person detection.
Following and aircraft movement were outside this accepted replay demo. Report mode/model,
action, approximate time and observations so logs can be correlated.

Logs are `qgc.log` and `pixeagle.log` in the isolated demo directory. The first
host launch exposed a missing `libxcb-cursor.so.0`, although the build container
had it. This system uses locally extracted Ubuntu 24.04 package
`libxcb-cursor0=0.1.4-1build1`; its package SHA-256 is
`137cf52479b5a9d8c5926d70d311af04be41941a32ee777340d704cc458c06d8`.
The launcher automatically uses this cache when present. To reproduce the
unprivileged extraction on this distribution:

```bash
mkdir -p ~/.cache/pixeagle-qgc-baseline/desktop-libs
cd ~/.cache/pixeagle-qgc-baseline/desktop-libs
apt-get download libxcb-cursor0=0.1.4-1build1
dpkg-deb -x libxcb-cursor0_0.1.4-1build1_amd64.deb root
```

This supplements the normal Qt/GStreamer runtime prerequisites; other systems
should install their QGC Linux dependencies. Blank maps in the isolated captures
are expected because those containers have no network. Map downloads on the host
also depend on provider/network availability and are separate from local video.

See [the baseline checkpoint](BASELINE.md) for original revisions, validation results,
commands, and outstanding gates. Linux evidence does not qualify Windows,
Android, SITL, or aircraft hardware.
