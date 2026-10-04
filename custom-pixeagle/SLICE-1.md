# Slice 1: authenticated connection and aircraft association

Date: 2026-09-21. Local Linux checkpoint; baseline prerequisites and remaining
repository-wide lint/CI blockers are recorded in [slice 0](BASELINE.md).

Status: slice 1 complete for the local mock-backed Linux connection gate. The
separate platform, video, flight and hardware gates below remain open.

## Result

The explicitly selected custom build has a default-off **PixEagle** settings
page. Setup is enable, select the active QGC vehicle, enter its endpoint, sign
in, then verify its aircraft association. Each vehicle owns a separate client,
cookie store, requests and status. Endpoints are the only persisted connection
preferences; credentials remain in memory. Standard QGC flight, mission,
multi-vehicle and camera functionality remains available.

The backend worktree adds authenticated `/api/v1/integration/context` and
observational `/api/v1/integration/connection` routes. Verification compares the
QGC aircraft UID with observed command and fresh telemetry UIDs, retaining 64-bit
identifiers as strings. A fresh read must confirm the discovery result before
QGC establishes a binding. Runtime/generation changes, stale replies, mismatches,
duplicate assignments, disconnects and lost identities withdraw trust.

No following, target-selection, provisioning or aircraft-control action is
exposed. The observational connection route does not invoke PixEagle's
following-start connection path. Video source/geometry remains explicitly
unverified until slice 2.

## Implementation boundaries

- `PixEagleClient`: session/CSRF contract, TLS verification, strict endpoint and
  redirect policy, request generations, bounded response size/age, polling and
  explicit aircraft verification. Cookies are isolated even between two ports
  on localhost. Disabled clients cancel requests and discard sessions.
- `PixEagleManager`: per-QGC-vehicle lifetime and endpoint preferences, active
  vehicle selection, duplicate aircraft/instance detection, and raw autopilot UID
  observation. A conflicting or subsequently zero UID remains untrusted until
  that QGC vehicle is removed; another component's UID does not alter it.
- Native Fact settings and QML use QGC controls, sizing and translations, with
  one sequential setup form and collapsed connection details. The core plugin
  continues to delegate stock behavior.
- Backend status/telemetry scopes, authenticated session/bearer access, POST
  CSRF, audit and route inventory apply to the new typed contract. Explicit
  MAVLink system/component routing replaces the relevant vehicle-1 assumptions.
  The backend retains its stable flight-owner loop and lifecycle barrier.

The backend details, exact commands and limits are in the sibling worktree's
`docs/reporting/agent-ops/codex-modernization/checkpoints/2026-09-21-qgc-native-context.md`
and `docs/apis/native-integration-context.md`.

## Validation evidence

Evidence directory:
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-1-2026-09-21/`.
Builds use the slice-0 pinned toolchain, dependency configuration, separate build
directories and recorded container image. No upstream/fork transport branch is
modified. Source/binary hashes and revision provenance are recorded in
`slice-1-manifest.json` alongside the source snapshot in the evidence directory.

- Custom Linux Debug and Release builds passed. Release boot passed.
- QGC standard `Unit|Integration`, excluding `Flaky|Network`, passed **406/406**
  CTest targets with four workers. All **three StockUI** targets remained
  enabled and passed. `custom-tests.xml`, `custom-tests.log`,
  `custom-test-runner.log` and `custom-ctest-details.log` retain the full run.
- New native Qt suites reported **67 client** and **4 manager** passes, including
  their init/cleanup cases. Coverage includes large/unknown/conflicting/zero
  UIDs, wrong endpoints, changing runtimes/generations, stale discovery
  confirmation, cookie/CSRF isolation, cancellation and late replies, vehicle
  removal, redirects, rejected TLS and secret-free preferences.
- Existing camera-free HTTP/WebSocket JPEG cases passed in the full suite.
  They remain generic unauthenticated transport coverage, not evidence for the
  future authenticated integration video path.
- After the final wording corrections, the native and StockUI selections
  passed **5/5** together; `copy-regression.*` records that focused result. No broad
  baseline sweep is needed for those copy-only changes.
- Backend focused validation passed **660 tests**, including 43 native contract
  tests. Schema validation passed at **43 sections / 606 parameters**. Route and
  candidate inventories, syntax compilation and diff checks passed. Its ignored
  `reports/qgc-slice1/` directory retains logs/JUnit.
- Source and validation-tool checks passed through the repository's locked
  pre-commit environment. Raw operator feedback is preserved verbatim rather
  than rewritten by prose formatters. Existing slice-0 lint/CI blockers remain
  separately documented; this is not a new all-green repository CI claim.

The full-suite command, from the QGC checkout root:

```bash
QGC_BASELINE_NETWORK=none custom-pixeagle/validation/run-container.sh bash -c \
  'cd build/pixeagle-custom-debug && python ../../.github/scripts/cmake_helper.py \
  ctest --build-type Debug --include-labels "Unit|Integration" \
  --exclude-labels "Flaky|Network" --jobs 4 \
  --junit-output slice-1-tests.xml --ctest-output slice-1-tests.log'
```

Stock source did not change in slice 1. Its separate slice-0 Debug/Release builds
and smoke evidence remain the stock baseline; this slice rechecks the retained
stock interface in the updated custom build.

## Interactive and operator evidence

The isolated fixture uses two mock MAVLink vehicles, two real authenticated
FastAPI route instances with mocked aircraft providers, Xvfb and software
rendering. It has no host network or hardware devices. The UI was exercised at
1440×900, 800×600 and 480×720 using separate settings. No aircraft was armed,
mission uploaded, camera accessed or production PixEagle service started.

The interaction sequence covered rejected credentials, sign-in, explicit
verification, matching full-width IDs, switching between two independent
endpoints, deliberate duplicate assignment, sign-out/address correction,
re-verification, healthy switch-back, companion loss, process restart and expired
session recovery. Restarting the app retained endpoints and required sign-in.
The Release app was also exercised separately with its compiled QML resources.

Initial captures/logs are in `ui/`; review corrections in `followup/ui/`; Release
runs in `release/ui/` and `final-ui/ui/`. Startup captures with missing-parameter
or mission-transfer dialogs are diagnostic fixture evidence, not successful
connection screenshots. The upstream mock lacks those protocol responses;
offline map warnings are also expected. No PixEagle QML runtime error remained.
The scan of 30 UI log/settings files found no fixture password or credential
headers; `ui-log-settings-check.json` records its scope. All slice-1 UI containers
were stopped after inspection.

[Raw reviews and dispositions](reviews/README.md) retain both original
interpretations and follow-up feedback. These are independent simulated operator
reviews with their prior context disclosed. They are not a human user study or
evidence of unbiased field usability. Copy changes identify the active vehicle,
conflicting assignment, correction steps, ongoing monitoring and the actual
reason verification was invalidated, without adding a dashboard of diagnostics.

## Demo and next gates

The local slice-1 demo is **connection and identity verification only**, using
the [documented fixture](README.md#isolated-local-connection-demo), or a matching
configured PixEagle backend with observable aircraft identities. Its Xvfb harness
captures evidence and does not open a host-desktop window automatically.

Slice 2 adds authenticated PixEagle video and read-only status in QGC's video
surface. Its gate is correct displayed-frame identity through drops, reconnects,
source changes and counter resets. That is the first integrated local video
demo. Slice 3 adds target operations; slice 4 following/gimbal controls; slice 5
Linux/Windows/Android release acceptance. Following remains unavailable now.

Production deployment needs a unique stable `PIXEAGLE_INSTANCE_ID` and the
correct telemetry route. The local-path fallback is not globally unique, and
MAVSDK-only telemetry cannot currently establish the required observed system ID.
Physical aircraft with indistinguishable on-wire identities cannot be proven
distinct by those identities alone. Camera geometry, real operator feedback,
touch/keyboard/high-DPI use, Windows and Android remain separate acceptance work.
SITL, hardware qualification, provisioning and deployment remain outside the
current local slice and require their agreed separate gates.
