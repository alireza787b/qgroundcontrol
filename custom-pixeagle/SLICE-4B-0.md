# Slice 4b.0 — Smart readiness and workflow contract

**Status:** Operator accepted the current workflow; proceeding to 4b.1 — 2026-09-30

The user reported that all requested tests were done and they were satisfied.
The saved September 30 session (03:04–03:06 UTC) records Smart activation,
two accepted Smart selections, three rejected clicks, and an accepted Cancel.
It used `visdrone26m` on CPU fallback. User acceptance does not establish
both-model accuracy or deterministic latency/drop qualification; those evidence
limits below remain recorded without blocking the authorized next slice.

This checkpoint keeps the accepted normal Classic/following workflow unchanged and begins the next phase with Smart evidence and terminology cleanup. It does not enable camera/gimbal control and does not require the Ethernet camera.

## Changes

- Corrected the connection guide so companion-only video and permitted targeting are described separately from aircraft following. Following requires a verified aircraft and a compatible PixEagle follower.
- Replaced the toolbar camera/video indicator with a PixEagle label and separate target/following states. Existing freshness and association guards remain authoritative.
- Added the running model/device result to Smart Options, alongside the existing model dropdown. The compact panel, stock flight-mode display, Settings and Dashboard links remain available.
- Tightened the test9 launcher readiness gate to require both installed Smart fixtures, `visdrone26m` and `visdrone9m`, rather than checking only one model.
- Confirmed the existing native inventory computes `available` from Full AI availability and verified detect/OBB support. QGC and the launcher use `available`/`unavailable_reason`; no redundant `smarttracker_supported` response field is added and compatibility is never inferred from a filename.
- Added the cleanup rule that redundant legacy configuration or UI paths may be consolidated when one authoritative owner, compatibility boundary, migration test, and removal condition are recorded.

## Automated evidence

Command:

```bash
custom-pixeagle/validation/run-sih-desktop.sh --check --video test9
```

Passed on 2026-09-28:

- isolated PixEagle and PX4 SIH startup;
- both test9 Smart model artifacts available through the authenticated model catalog;
- PX4 telemetry delivered to QGC UDP 14560;
- simulated aircraft UID resolved;
- authenticated QGC-route WebSocket JPEG delivery;
- launcher cleanup completed.

The rebuilt Release binary passed a private X11 boot smoke with the new toolbar
and Options QML. The private test9 screen showed `PixEagle`, separate `No target`
and `Stopped` states in the toolbar, and Smart Options runtime text for
`visdrone9m` after CPU fallback. QGC logs had no QML `TypeError`, `ReferenceError`,
or binding-loop error. A video-delayed notice appeared during CPU fallback.
The source loops; the earlier explanation that the clip ended was incorrect.
That initial screen alone did not establish smooth Smart playback or target selection. The later measurements below establish ongoing delivery, with CPU throughput limits.

The test9 source is a controlled replay. This check proves readiness and transport, not model accuracy, target continuity, aircraft following, or camera-owned tracking.

## Build note

The existing custom Debug build directory could not relink because its generated Qt dependency refers to `/usr/lib/x86_64-linux-gnu/libxkbcommon.so`, while this host currently provides only the versioned runtime library. The failure is environmental (`ninja: error: ... libxkbcommon.so ... missing`) and is separate from the QML change. QML lint, shellcheck, markdownlint, and typo checks passed. Debug and Release subsequently rebuilt successfully using the already documented pinned build container, which contains the development dependency. No host library replacement or symlink workaround was needed.

## User test requested next

Use the rebuilt Release binary and private test9 launcher. Run the normal
workflow twice: once with `visdrone26m`, once with `visdrone9m`. In each run
record the model shown in Options, device/fallback status, one Smart selection,
one retarget, Cancel, and any loss/recovery. Do not connect the camera yet.

The first standard Debug run was interrupted at 32/411 and remains archived as
inconclusive. The completed September 30 run passed **411/411** tests in
**301.32 seconds**, using `Unit|Integration` labels and `Flaky|Network`
exclusions. Focused PixEagle tests also passed: TargetController 90/90,
ModelClient 65/65, Manager 25/25, and VideoController 5/5. The backend review
reported 113 focused native-model, route-inventory and parameter-reload tests
passing, plus schema validation. Scoped QML/shell/Markdown/typo checks passed;
this is not a repository-wide lint or cross-platform qualification claim.

## Live Smart evidence and limitations

The Release app ran on a private Xvfb display, with the isolated SIH aircraft on
the ground. No following or takeoff was commanded in this checkpoint. Full AI
and both installed models were available. The NVIDIA driver was unavailable,
so this session used CPU; the earlier CUDA session is separate evidence.

| Active model | WebSocket JPEG sample | Largest inter-frame gap | Native evidence |
| --- | --- | --- | --- |
| `visdrone9m` | 48 frames / 20.45 seconds, approximately 2.3 fps | 0.553 seconds | Smart active; Options showed CPU fallback |
| `visdrone26m` | 111 frames / 20.04 seconds, approximately 5.5 fps | 0.385 seconds | Selected through QGC dropdown; backend audit confirmed model-switch success; Options showed running on CPU |

These are delivery measurements with QGC also connected, not display latency,
drop counts, sustained 30 fps, or target-accuracy results. They do not explain
the initial delayed-video notice. A separate fixed-frame CPU inference check
found detections with both models, but does not prove native selection.
Two automated native clicks on `visdrone9m` returned no matching object; the
panel reported a failed target action and did not claim tracking. Successful
selection/retarget, loss/recovery and Cancel remain an operator test, as requested
by the user. A deterministic Smart selection-latency/drop report also remains
open; the existing Classic target-path fixture does not establish Smart results.

The independent [AI-simulated screenshot review](reviews/slice-4b-0-ui-review.md)
recommended retaining the accepted layout and prioritizing live behavior. Small
toolbar text and the generic no-object failure wording remain usability followups.
No human/vendor expert endorsement is claimed.

Evidence is under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4b-2026-09-30/`:
`tests.xml`, `tests.log`, `tests-driver.log`, `smart9-pacing.json`,
`smart26-pacing.json`, `inference-benchmark.json`, and `runtime-evidence/`.
Private screenshots are in the September 29 `ui/` directory, including
`10-options.png` and `11-model-switch.png`. The demo containers and dashboard
were stopped after evidence capture; the launcher prepares the next user run.
The [checkpoint manifest](slice-4b-0-manifest.json) records source and artifact hashes.

No configuration keys were migrated in 4b.0. The
[camera scenario guide](CAMERA-SCENARIOS.md) distinguishes current settings from
the planned provider/control separation. Continue with 4b.1 settings/apply/OSD
and 4b.2 mock camera controls after the Smart checkpoint; physical Ethernet
camera work remains 4b.3.
