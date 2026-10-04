# Release preparation and remaining qualification

Updated 2026-10-04. Tested changes are being committed in separate reviewable
groups; remaining integration edits are uncommitted and nothing is published. The
camera is unavailable for the fresh physical retest; camera-free software
qualification and release preparation proceed without recording acceptance.
The ordinary Pi and explicit gimbal-lab deployment choices are in
[DEPLOYMENT-PROFILES.md](DEPLOYMENT-PROFILES.md).
The public PixEagle versus private customized-QGC boundary is in
[PUBLICATION-STATUS.md](PUBLICATION-STATUS.md).

## Required gates

| Gate | Position |
| --- | --- |
| 0–3b optional connection/video/local tracking | Local workflow accepted; retain stock-QGC/default-off regression |
| 4a normal following | Five multicopter dispatch/response/Stop cases passed; attitude-envelope and fixed-wing blockers remain |
| 4b.3 camera control and camera-owned Classic/Smart | Operator accepted before latest guidance repair |
| 4b.4 geometry/control/SIH | Four gimbal follower/mount synthetic cases passed; fresh camera retest and log review pending |
| 4b.4d network/load | 68 focused software checks passed, including shared-relay/reconnect and suspension evidence; radio, Pi load and camera network/process-failure motor behavior pending |
| 5 Linux release | Release build, isolated boot, native package identity CTest, and DEB generation passed. The package declares `libxcb-cursor0`; the host still does not have that runtime installed, so local package installation/launch remains a separate prerequisite |
| 5 Windows x64 | Installer build, bundled-runtime launch, secure credential store, upgrade/removal/coexistence and operator checks pending |
| 5 Android | Target SDK build, install/launch, touch/PiP/suspension/disconnect and secure credential store checks pending |
| 5 source publication | Classify existing edits, coherent tested commits, backend/QGC compatibility/rollback docs and reviewed PRs pending |
| 6 onboard ground | Pi/OS/accelerator, router/PX4/camera firmware inventory, deployment and command-blocked acceptance pending |

The accepted layout stays fixed during qualification. New tests are evidence
for the stated simulated/software conditions, not physical mount calibration,
tracker accuracy, optimal gains or real-flight approval.

## Platform provenance and package isolation

Use the pinned `.github/build-config.json`, developer `tools/uv.lock`,
`QGC_UPDATE_TRACKED_DEPS=OFF` and the dependency preload. Select
`QGC_CUSTOM_DIR=custom-pixeagle` explicitly on every custom platform. Stock
CI presets otherwise build stock QGC; successful stock Windows/Android CI
cannot qualify the overlay.

The independent identities are:

- Application/settings: `PixEagle-QGroundControl`, organization `PixEagle`.
- Package/mobile identifier: `io.github.alireza787b.pixeagle.qgroundcontrol`.
- Linux native package and launcher must remain distinct from stock
  `qgroundcontrol` / `QGroundControl` and its `/opt/QGroundControl` runtime.
- Windows NSIS install directory, uninstall registry and shortcuts follow the
  custom project identity; actual install/upgrade/uninstall testing is required.

Linux currently uses Qt 6.11.1 and available GStreamer 1.24.2. Windows/Android
use their own pinned build-config dependency versions; do not silently claim
the Linux codec/runtime test covers them. Installed local tooling has only
Linux Qt. A native Windows build runner/MSVC/NSIS and Android Qt/SDK/NDK/runtime
are prerequisites for their artifacts. Do not label a Linux binary a Windows
executable or claim a checksum for an artifact that has not been built.

The existing SIH runners accept explicit backend repository, Python runtime,
SDK/router executable and library paths, and record hashes/packages/images.
Their current convenience defaults refer to this private baseline cache;
portable dependency provisioning remains a slice-5 gate. Immutable test
snapshots must capture dirty source hashes, not only unchanged git HEAD.

## Known debt before publication

Broader upstream/imported lint blockers remain in [BASELINE.md](BASELINE.md).
The expanded [camera-free qualification](SLICE-4B-4D.md) found two normal-mode
release issues: actual multicopter attitude pitch can exceed the advertised
angle envelope under sustained reversal despite the new guard. The fixed-wing
launch fixture is now repaired, but typed airspeed acquisition and actual sensorless following remain unqualified.
The operator-requested opt-in ground-speed proxy is now in PixEagle backend
configuration and Dashboard, false by default; it retains the live-profile gate. No gains or default limits were changed to mask them.
Build-aware custom QML lint has an unresolved plugin type warning despite
passing source-only checks. The installed MAVSDK lacks the existing telemetry
manager's `velocity_body` API; qualification uses the maintained MAVLink2REST
path. A supported SDK-only path needs repair/qualification before it can be
advertised. No model/segmentation/recording administration expansion is implied.

Native package identity is now parameterized: custom DEB/RPM uses
`pixeagle-qgroundcontrol`, runtime `/opt/PixEagle-QGroundControl` and launcher
`/usr/bin/PixEagle-QGroundControl`. Stock identity stays unchanged. The pure
CMake CTest passes stock/custom metadata, staged payload, coexistence and invalid
identity checks. Git tags were fetched without moving source HEAD, resolving
the missing version prerequisite to `v5.2.0-dev-21-g2e6190baf`.

The real DEB attempt then failed dependency inspection because this host lacks
`libxcb-cursor.so.0`, required by the bundled Qt xcb plugins. The canonical setup
lists `libxcb-cursor-dev`; install that dependency before retrying
`cmake --build build/pixeagle-custom-release --target qgc-package`. Local sudo
requires an operator password, so no system installation was attempted. Do not
disable dependency inspection or claim a complete installer from the staged
runtime. Logs: `slice-4b4-2026-10-02/linux-deb-package-tagged.log` in the private
baseline cache. Windows and Android artifacts have not been built.

Preserve unrelated local edits and the separate clean PixEagle `main` checkout.
Keep generic QGC fixes (for example native package parameterization) distinct
from PixEagle-specific UI/client/overlay changes. Do not merge into main or
publish artifacts while acceptance/platform gates remain unresolved.

## Operator actions later

When the camera is available, run the [fresh camera retest](OPERATOR-RETEST-4B-4.md)
for both gimbal followers, invalid-edge preservation, retarget/recovery,
altitude guidance and Stop. A subsequent physical network/process-failure
check must record the camera's actual motor behavior; transmitted UDP Stop
alone does not prove stopping.

After simulated/physical acceptance, inventory the onboard Pi, OS, accelerator,
camera firmware, PX4 firmware, router/datalink topology and credential/trust
deployment. The onboard ground checkpoint keeps PixEagle aircraft commands
blocked and uses Dashboard Follower Test for intent generation. It verifies
zero aircraft command dispatch, sustained load/temperature/memory and recovery.
Release publication and any real-flight qualification remain explicit later
checkpoints.

## Latest rebuilt QGC regression

Debug and Release builds pass with the pinned Qt 6.11.1 toolchain and the
locked Ninja executable. The fresh Unit/Integration run with standard
Flaky/Network exclusions recorded **414/414 passes** in 305.05 seconds,
including the previously isolated `CMake.QGCTestMultiConfig` fixture. The
full log is retained at
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/qgc-final-path-corrected.log`.

The Release package was generated with CPack using the extracted pinned
`libxcb-cursor0` library as a private dependency-search path and an explicit
`libxcb-cursor0` package dependency. Its checksum is recorded beside
`build/pixeagle-custom-release/pixeagle-qgroundcontrol_5.2.0-31.g78286f417_amd64.deb`;
the package is evidence of a reproducible artifact, not proof that this host
can install it without that dependency.
Focused PixEagle/stock-UI/package checks passed 9/9 before the rebuild. All 14
mocked SIH launcher/startup boundary tests pass. Broader lint and platform
acceptance limitations above remain open.

## Commit and regression checkpoint — October 4

Backend commit `e083f71` records the sustained shared-link qualification;
`a1724a8` records project-use and release boundaries. QGC commits `9d9e5f275`
(native overlay/video path), `04ba98b5b` (SIH validation harness),
`dd917e5ef` (qualification evidence), `6ca249720` (package identity), and
`2f4599db8` (deployment/publication status) record the tested integration
groups. These commits stay on their integration branches; neither main nor
public artifacts are updated. Remaining source edits need separate reviewed
commits.

The renewed backend restart/defaults/replay/engine/altitude/fixed-wing boundary
gate passed **142 tests**; the production shared-link fixture passed separately.
The focused recovery/transport suite passed **226 tests**, covering ownership,
SIH validation contracts, supervision, streaming lifecycle, WebSocket reconnects,
and video integration. Four QGC client/connection/video suites and
`CMake.NativePackageIdentity` passed.
Build configuration validation passed. These focused checks supplement the full
suite results above and do not replace hardware or platform qualification.

The Linux package modules and new identity fixture pass the locked CMake
format/lint hooks. The existing `cmake/tests/CMakeLists.txt` still has baseline
formatting differences and two missing function docstrings; the added test
registration introduces neither issue. No whole-file reformat was applied.
