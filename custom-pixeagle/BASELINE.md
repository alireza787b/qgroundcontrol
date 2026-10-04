# Native PixEagle integration: slice 0 baseline

Date: 2026-09-21. Branch: `feature/pixeagle-native-integration`.

Status: slice 0 workspace, scaffold, and Linux baseline evidence complete, with
the existing lint/guardrail blockers listed below. This is not a green
repository-wide CI or release-qualification claim.

## Revisions and scope

| Input | Revision |
| --- | --- |
| QGC upstream | `6f1f4368de08fe093291c63ac7f012c797aaf3c4` |
| PixEagle | `989d9662173b364de03b307f4208e9d0ca96451f` |
| HTTP JPEG, PR #14730 | `7100b92dfadf5c20ccdabbc22f65bb651e2d9690` |
| WebSocket JPEG, PR #14731 | `1f5432b38bf761b2bde921bfa64a610c3819dadc` |
| ArduPilot parameter definitions | `6a8165531148318d9d5975e36d1a8b5323f0030a` |

`origin` is `alireza787b/qgroundcontrol`; `upstream` is
`mavlink/qgroundcontrol`. The integration branch starts from upstream, not the
older fork `master`. The HTTP commit is an ancestor of the WebSocket commit.
Supporting PRs #14727, #14728, and #14729 are already merged upstream; the two
transport PRs remain open. Their remote branches are not modified.
The conflict-free integration merge is
`2e6190bafc4f4796e29ae24cbd1d27f73b96d07d`, with the upstream and WebSocket pins
as its two parents.

The untouched upstream checkout is the detached worktree `build/upstream-source`.
PixEagle has a separate `/home/alireza/PixEagle-qgc-integration` worktree on
`feature/qgc-native-integration`. Its original checkout remains clean on `main`.

Only a minimal core plugin and build identity are added here. It inherits stock
QGC behavior, including both firmware factories and multi-vehicle UI. No
PixEagle connection/authentication/settings/API or flight-command path is added.
The runtime enable switch and operational controls belong to later slices.

## Toolchain and reproducibility

The host is Ubuntu 24.04 x86_64 with GCC 13.3.0 and GStreamer 1.24.2. The
repository's locked tooling installed CMake 3.31.10 and Ninja
1.13.0.git.kitware.jobserver-pipe-1 into `tools/.venv`.

Qt 6.11.1 is installed at
`/home/alireza/.cache/pixeagle-qgc-baseline/Qt/6.11.1/gcc_64`.
All configured modules are installed. An initial configure exposed the additional
`qttasktree` dependency of Qt's QML asset downloader; that module was installed
without changing the SDK version or upstream source.

Native configuration also needs system XKB/Vulkan/OpenGL development packages. Since
host package installation requires a sudo password, validation uses an isolated
Ubuntu build container. It mounts only this repository, the baseline cache, and
the `uv` executable; no host devices or host network are exposed. Build processes
run as UID/GID 1000. Qt and the locked tooling are reused inside the container.

The container base is upstream's Ubuntu 24.04 digest
`sha256:786a8b558f7be160c6c8c4a54f9a57274f3b4fb1491cf65146521ae77ff1dc54`.
The built image is `pixeagle-qgc-baseline:2026-09-21-opengl`; its exact ID, Dockerfile,
installed package versions, invocation script, and logs are retained as evidence.
The base digest and package inventory identify this run; apt packages in a fresh
image build can change, so use the recorded image ID to reproduce its environment.
An exported `container-image.tar.gz` and its SHA-256 are retained locally. The
[container recipe](validation/Dockerfile) pins direct package versions; the
[full package inventory](validation/container-packages.tsv) records transitive
packages too. Installed Qt files have a SHA-256 inventory in the evidence directory.
Rebuilding the pinned recipe as `pixeagle-qgc-baseline:2026-09-21-pinned`
succeeded and reproduced the same complete package inventory (empty diff).
`pinned-container-build.log`, `pinned-container-packages.tsv`, and the manifest
record that verification. Builds/tests used the original recorded image ID.

The first container build reached the final link but lacked `eglGetDisplay`.
Installing `libopengl-dev` allowed CMake to discover `OpenGL::EGL`, which upstream
already links. Reconfiguration and the unchanged upstream Debug build then passed.
No source patch or ad hoc linker flag was needed.

Every baseline configure loads `cmake/BaselineDependencies.cmake`: automatic
dependency refresh is disabled and the moving ArduPilot `main` dependency is
pinned. Upstream version/commit declarations remain otherwise intact. The
resolved source inventory, including dependency submodule revisions, belongs
with this run's evidence. See [build commands](README.md#linux-development).

## Evidence and status

Host evidence directory:
`/home/alireza/.cache/pixeagle-qgc-baseline/2026-09-21/`.

The unchanged upstream Debug build passed. Its standard `Unit|Integration`
selection, excluding `Flaky|Network`, ran 405 CTest targets: 403 passed initially.
Both remaining targets passed focused reruns:

- `GPSReceiverSettingsTest`: the disconnected-page visibility check exceeded its
  one-second timeout under parallel load; its focused rerun passed.
- `CMake.PythonVenvStaleCache`: inherited `QGC_PYTHON_ENV` repopulated the cache
  the test expects to be empty. Its rerun passed after removing that build-only
  override from the test environment. The validation wrapper leaves it unset.

Logs: `upstream-debug-retry.log`, `upstream-tests.log`,
`upstream-gps-retry.log`, and `upstream-python-retry.log`; matching test XML is
retained. The upstream worktree remained unchanged.

Upstream's full `pre-commit run --all-files` sweep initially found 14 failing hooks.
The Python type-check hook passed after installing the audit checkout's own
locked tooling environment; the first run lacked its expected `tools/.venv`.
The 13 remaining hooks cover file modes/whitespace, formatting, Markdown/spelling, QML/CMake
lint, and existing assertion/wait/null-safety checks. It ran in a separate
`build/lint-source` checkout because some hooks modify files. The full report
and `upstream-lint-summary.json` identify the baseline failures; no bulk cleanup
was imported. Checks scoped to the new overlay and validation files pass.
The successful Python recheck is `pyright-environment-retry.log`; it is not
counted as a remaining source blocker.

A lint pass over the imported transport changes plus the overlay found four
failing hooks: C++ formatting in the pinned transport files, CMake formatting,
two existing overlong CMake comments, and an existing `Q_ASSERT(user_data)` in
`GstVideoReceiver.cc`. `combined-changed-lint.log` records these separately from
the passing overlay checks. The original transport branches remain unchanged.

Both combined Debug builds passed. With `Unit|Integration`, `Flaky|Network`
excluded, and four workers per configuration:

| Configuration | CTest result | Follow-up |
| --- | --- | --- |
| Combined stock | 404/405 initially passed | GPS settings timeout passed its isolated rerun |
| Minimal custom | 404/404 passed | All three `StockUI` targets included and passed |

The custom count differs because upstream registers
`CMake.ApplicationAutogenGraph` only for stock builds. Both firmware plugin
factories remain enabled. Stock also passed all three `StockUI` targets.

All 13 HTTP/WebSocket JPEG cases passed in each build, with no skipped JPEG
cases. They cover delivery, appsrc queue limits, frame limits/malformed images,
trusted and rejected TLS, failed handshakes, remote disconnect, immediate stop,
and unsafe URL rejection. Logs: `stock-jpeg-tests.log`, `custom-jpeg-tests.log`,
and the full `stock-ctest-details.log` / `custom-ctest-details.log`.
The CTest reports and JUnit are `stock-tests.*` / `custom-tests.*`; the focused
stock rerun is `stock-gps-retry.*`. This is evidence for the combined branch,
independent of the original PR CI results.

Both Linux release builds and `--simple-boot-test` passed. Boot commands are in
the [build guide](README.md#linux-development). Interactive smoke checks used
Xvfb at 1440x900, Mesa software rendering, separate per-build home/XDG settings,
two instances of upstream's `tools/simulation/mock_vehicle.py` (IDs 1 and 2),
and the pinned PixEagle `tools/qgc_media_test_source.py` on loopback port 8095.
The UI containers had no external network or hardware devices.

| Release smoke check | Stock | Custom |
| --- | --- | --- |
| Fly View and native flight-mode controls | Passed | Passed |
| Active vehicle selection, ID 2 to ID 1 | Passed | Passed |
| Offline map coordinates and vehicle/mission overlays | Passed | Passed |
| HTTP JPEG and WebSocket JPEG rendering | Passed | Passed |
| Map/video PiP swaps and fullscreen entry/exit | Passed | Passed |
| Create and save a takeoff-plus-waypoint plan | Passed | Passed |

The saved plans contain MAVLink mission commands 22 and 16; no mission was
uploaded. `release-smoke-results.json`, `ui-stock/`, and `ui-custom/` retain
binary hashes, screenshots, application/source logs, isolated settings, and
saved plans. `start-ui.sh`, `launch-ui.sh`, and `ui-exec.sh` retain the local
smoke harness. All test containers were stopped after inspection.

These checks have explicit limits: external map tiles were unavailable with
networking disabled; the existing four-parameter mock lacks complete parameter,
mission, geofence, and rally responses and produced notices that were dismissed;
speech dispatcher was absent. This validates view interaction and synthetic
video rendering, not complete aircraft protocol or physical GPU/audio behavior.

Backend evidence is already recorded in the PixEagle worktree's
`docs/reporting/agent-ops/codex-modernization/checkpoints/2026-09-21-qgc-native-baseline.md`:

- Focused backend: **268 tests passed**, schema check passed (604 parameters).
- Dashboard: **471 tests passed**, production build passed.
- Existing dashboard lint: **four failures** in test files, reproduced locally.
- Existing backend CI guardrails: **152 passed, one failed**; a textual hygiene
  rule flags fake-send lambdas containing `or True` in gimbal tests.

Those pre-existing failures are baseline blockers, recorded separately from the
integration. No backend or dashboard operational source was changed to obtain
these results.

## Acceptance boundary

The baseline gate has recorded build/test evidence and identified the remaining
upstream/imported lint and backend guardrail failures. Keep `StockUI` tests
enabled in future custom builds. Windows and Android remain required, untested
release targets. No SITL, HIL, provisioning,
deployment, aircraft or full camera/gimbal qualification is claimed. Vertical
mounting remains unqualified as stated in the original handoff.

The next slice adds authenticated per-instance client state and verified vehicle
association; following controls remain disabled there.
