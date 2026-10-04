# PixEagle–QGC status — 2026-10-04

Latest [v21 operator review and next-slice plan](SLICE-4B-V21-ACCEPTANCE.md):
operator accepted the recorded workflow; all 38 audited target starts succeeded
and all 277 recorded publications succeeded. Chase retargeting restored authority
and produced simulated yaw response. Prediction-only startup refusal and target-
uncertainty handoffs remain explicitly recorded. The selection repair is accepted;
full 4b.4 and release qualification remain open. Next is remaining 4b.4d shared-link/load, application suspension and
normal recovery/altitude overlays. Camera hardware is not needed for that software work.

The 4b.4d software gate is now renewed: 245 recovery/controller tests passed and
the full backend CI unit sweep passed 3,463 tests,
and the deterministic link envelope again met dispatch, expiry, heartbeat,
reorder and obsolete-motion limits. A failed Stop has an explicit guarded
cleanup retry; failed cleanup still blocks target admission. Radio throughput, QGC transport contention, suspension, process-death and physical
motor behavior remain open. On 2026-10-04, the focused link harness passed 68
tests, including a short sustained production-fixture run with 60 authenticated
control renewals and 60 JPEG WebSocket frames sharing one 128 KiB/s loopback budget.
The relay reconnect test also passed without reviving the old stream. Dashboard
VideoStream tests passed 25/25, and the QGC native video controller now treats
hidden application state as suspended.
Single-vehicle sign-in now automatically starts the normal identity verification
request when command and telemetry identities match, even before the backend
reports its association flag; multi-vehicle and conflict guards remain explicit.
This does not qualify QGC desktop UI, radio, Pi, camera or motor behavior.
The fresh shutdown-retry and link evidence is recorded in the backend checkpoint
at docs/reporting/agent-ops/codex-modernization/checkpoints/2026-10-03-qgc-4b4d-shutdown-retry.md
and the local manifest at ~/.cache/pixeagle-qgc-baseline/slice-4b4d-2026-10-04/link-v3-sustained/manifest.json.

Slices 0–3b, normal following in 4a, and camera-v5/v6 tracking/control in 4b.3
have local operator acceptance. The subsequent gimbal repairs and both followers
on synthetic horizontal/vertical mounts have software/SIH evidence; final new
physical camera acceptance remains open. The operator is away from the camera,
so that test is postponed until return. No hardware connection is needed now.

The [shared restart and recorded-video checkpoint](SLICE-4B-RESTART.md) now passes:
normal Linux/SIH use one backend supervisor; QGC Backend settings offer guarded
restart and report canonical pending changes; verified HTTPS administrators can
use the explicit remote policy. A fresh copied-source restart returned ready in
7.161 seconds, preserved sidecars/logs and resumed no operations.

The [deployment profile card](DEPLOYMENT-PROFILES.md) now records the ordinary
Raspberry Pi path separately from the explicit Ethernet-gimbal laboratory path.
It reconciles the checked-in defaults, the `192.168.0.x` bench addresses and
service ports with the older PB-SW-MAN manual, and documents why credentials,
remote exposure, dual-interface routing and GStreamer remain deployment gates.
The ordinary profile stays video-file/CSRT/local PixEagle with no gimbal and
`mc_velocity_position`; no camera or SSH action is required until the Pi and
camera are available for the final hardware inventory.
The [publication-status note](PUBLICATION-STATUS.md) records the public
Apache-licensed PixEagle project separately from this unreleased customized QGC
qualification branch.

The earlier restart checkpoint passed **3,419 backend Unit** (41 optional dependency
skips), **195 Integration**, **490 Dashboard** and **414/414 QGC** tests. Relevant
schema, API/security, supervisor/profile and lint/build checks pass. Recorded
CSRT acquisition/retarget/Stop and local Smart detection/selection/Stop pass.
Smart currently uses CPU fallback; this is not GPU performance qualification.
Fresh Position, Chase and Distance SIH runs add **596 successful publications**,
zero failures, independent simulated response and confirmed Hold. Test overlays
exercise altitude guidance; factory Position remains yaw-only. No gains changed.

The [camera-free qualification record](SLICE-4B-4D.md) retains the earlier five
normal multicopter and 64 link/load software checks. Sustained MC attitude-rate
still exceeds the configured envelope; fixed-wing startup is proven separately
from following. Both remain release-qualification blockers. Opt-in ground-speed
fallback stays in PixEagle Dashboard configuration, off by default.

Next: retain the physical camera/failure retest for the user's return, finish
the remaining radio/Pi/process-loss evidence, then slice 5 portable harness
and Linux/Windows/Android release/PR gates and slice 6 onboard ground
acceptance.
The older Linux prototype package does not include the new restart change.
PixEagle main and release artifacts are not updated. Tested changes are being
committed in reviewable groups on both integration branches; remaining source
edits are uncommitted and nothing is published. Backend commits `e083f71` and
`a1724a8` record sustained-link qualification and release boundaries. QGC
commits `9d9e5f275`, `04ba98b5b`, `dd917e5ef`, `6ca249720`, and `2f4599db8`
record the native overlay, validation harness, qualification evidence, package
identity, and deployment/publication documentation. The renewed
focused gate passed 142 backend tests, an additional 226-test recovery/transport
suite, four QGC client suites and the Linux package-identity test. Real-flight qualification
remains later scope. The optional [recorded-video SIH handoff](OPERATOR-RECORDED-SIH.md)
is prepared but stopped.

The [recorded-tracker-to-SIH checkpoint](SLICE-4B-RECORDED-SIH.md) adds one
advanced Dashboard replay allowance, off by default and restricted to the owned
isolated SIH launcher. Dashboard Follower Test retains preview-only behavior.
Fresh actual CSRT selection drove 168 successful publications, zero failures,
independent −36.99° simulated yaw response and confirmed Hold. The five-minute
[operator v20 handoff](OPERATOR-RECORDED-SIH.md) is prepared with commands blocked.
This corrects v17's misleading recorded-follow instructions; earlier synthetic
follower evidence was separate. Factory defaults and gains remain unchanged.

Renewed gates: 3,452 backend Unit, 269 Integration/route/config checks, 491
Dashboard and 414/414 QGC Unit/Integration tests. Exact attempts and hardware
limitations are recorded in that checkpoint. Physical camera and real-flight acceptance remain
open.

The sections below retain historical local workflow evidence.

V21 repairs the v20 target-selection failure after completed following Stop.
Only clean teardown releases the stopping flag; failed/pending shutdown stays
blocked and handoff history is retained. Renewed backend gates: 3,460 Unit,
269 Integration/route/config and 48 API/docs/test-hygiene checks. Actual recorded
CSRT selections after switching to Chase and Visual Centering succeeded, with
263 successful publications and confirmed Hold after each Stop. Use the fresh
[v21 handoff](OPERATOR-RECORDED-SIH.md); physical acceptance remains separate.
See [the checkpoint](SLICE-4B-RECORDED-SIH.md) for historical evidence; the
failed-shutdown retry repair is recorded in the current 4b.4d checkpoint. Defaults and gains are unchanged.

## Latest operator fixes

The second slice-4a desktop report exposed a missing aircraft-specific demo
endpoint, dark placeholder contrast, a Docker-forwarded WebSocket Origin
rejection, and unpaced image-sequence replay. These are fixed and documented in
[the slice-4a checkpoint](SLICE-4A.md). The compact panel now keeps Classic/Smart visible, summarizes fresh
tracker/follower states, and uses QGC hold confirmation for Start with immediate
Stop. The gear opens a separate configuration dialog. Both video panels can be
dragged; connection status and Dashboard access are in the standard toolbar
drawer. Initial Smart-model selection remains available in Tracking options. Use the updated [operator steps](OPERATOR-TEST-4A.md), including
Sign in → Verify vehicle → Open Fly View. Verification is also available
directly in Fly View; its unavailable tracking panel stays hidden until the
connection and video are ready. The scripted `--video target-path` fixture
adds repeatable Classic-target motion with a frame-by-frame reference; test9
remains the Smart detection clip. The latest target-path acceptance did not exercise Smart.

## Current addition

- Dashboard opens the system browser from PixEagle settings, Tracking options,
  or its toolbar drawer.
- Default address: backend host and protocol, port 3040, root path. This is a
  fallback convention, not frontend discovery. The inspected backend has no
  authoritative dashboard-URL contract; CORS origins are not that contract.
- Advanced users can save a complete dashboard URL, including a different host,
  port or proxy path, under Web dashboard. Use default removes it.
- Preferences follow the full backend endpoint, including port/prefix, and the
  active vehicle/companion. Browser navigation shares no native session secrets.
- Connection help opens the setup guide and documentation. About PixEagle uses
  QGC's standard dialog with the exact published copyright, source and license.
  The matching native-integration backend is still required; public main
  documentation does not promise this unmerged QGC branch is already part of
  stock QGC.
- Web dashboard and Connection details use QGC SectionHeader disclosures.
  Connection help and About PixEagle are footer actions. The whole-page review
  shortened repeated labels, removed duplicate aircraft guidance and grouped
  Video and targeting. Only real on/off preferences retain checkboxes.
- PixEagle remains off by default. Help is available without enabling it.

## Accepted session and evidence

The saved desktop session ran September 24, 04:38–04:47 UTC; the user's acceptance
arrived September 26. Its audit records four accepted Classic starts, thirteen
accepted Smart clicks, five Smart-mode changes and six Cancels. Two no-object
and one ambiguous-object Smart refusals were followed by accepted selections.
VisDrone26m ran on CUDA. No backend ERROR/CRITICAL or QML binding error appeared.
Following stayed inactive and PX4 disconnected. No live monitoring occurred
while the coding agent was idle.

The logs also retain tracker losses, a source/configured-resolution warning and
OSD budget degradation. The accepted workflow is not continuous tracker-accuracy
qualification. These logs do not establish that the user switched models or used
all six Classic trackers; earlier automated model/tracker checks remain distinct.
See [the raw session review](reviews/slice-3b-accepted-session-raw.md).

## Plan position

| Stage | Status | Remaining gate |
| --- | --- | --- |
| 0 — reproducible workspace and baseline | Implemented, recorded stock/custom Linux builds and pinned dependencies | Retain recorded stock/backend baseline caveats; combined release qualification is still later |
| 1 — optional connection and association | Implemented and tested; default off, authentication, per-vehicle and companion-only sessions | Deployment-specific connectivity and identity qualification |
| 2 — video and read-only status | Implemented and accepted locally; authenticated JPEG, frame provenance, map/video, PiP/fullscreen | Other selectable transports require equivalent displayed-frame evidence for targeting |
| 3 / 3a — Classic targeting and UX | Implemented and accepted locally; advertised tracker choices, selection/retarget/Cancel | Physical external-camera target qualification remains open |
| 3b — Smart models and simplified controls | Implemented; local workflow accepted; dashboard/help feedback addressed | Hardware target gate is separate; no claim that user covered every model/algorithm |
| 4a — normal following | Native profile/readiness, Start/Stop, pending cancellation, QGC per-aircraft Stop and isolated SIH command publication passed | Accepted target-path operator run; Smart recheck remains separate; SIH is not field qualification |
| 4b.0 — Smart readiness and workflow contract | Operator accepted; Debug/Release and 411/411 Linux tests passed; both installed models exercised on CPU | Sustained performance and deterministic accuracy qualification remain separate |
| 4b.1–4b.2 — settings and camera software | Software/mock gate passed; Debug/Release and 413/413 Linux tests, live OSD and joystick checks recorded | Physical movement, source alignment and camera-owned tracking remain unqualified |
| 4b.3 — Ethernet/RTSP bench | Operator accepted camera-v5/v6 | Retain hardware evidence; new control changes require targeted regression |
| 4b.4 — camera following | Repairs and both gimbal follower/mount software/SIH gates recorded; shared restart and recorded/local qualification passed | Fresh physical camera acceptance deferred; horizontal physical installation untested |
| 4b.4d — network/load | 68 focused software tests passed; shared-relay and suspension evidence recorded | Radio throughput, actual Pi load, camera/process-loss motor behavior |
| 6 — onboard ground test | Pending release/SIH gates | Pi/router/PX4/camera inventory and CB-active hardware acceptance |
| 5 — release and upstream preparation | Not complete | Linux regression, Windows/Android builds and touch/input acceptance, onboarding/troubleshooting, extension docs, coherent reviewable commits and PRs |

Slice 4a/4b are sequencing labels within the existing slice 4, not new scope.
The [slice 4a checkpoint](SLICE-4A.md) records implementation, Linux tests,
the isolated SIH run and the raw private screenshot review.
The user confirmed Ethernet and RTSP for the next camera checkpoint. Recover the
existing provider/configuration and original gimbal notes before requesting
hardware connection. The recovered provider is **Topotek SIP UDP**: Ethernet
UDP control/telemetry and matching RTSP input. Earlier dashboard hardware
acceptance was partial. Later QGC camera-v5/v6 acceptance and the vertical
mount observations are recorded separately; they do not qualify every camera
or arbitrary installation geometry. Keep that separate from local
AI Smart tracking. See [the recovered notes](reviews/next-gimbal-notes.md).
No new vendor implementation is needed. Camera RTSP can feed PixEagle while
QGC keeps its authenticated, frame-associated video path; direct RTSP display
is not automatically qualified for native target selection.

Following remains owned by PixEagle. QGC's flight controls and multi-vehicle
behavior stay available. Stop tracking, Stop camera and Stop following remain
distinct. The pinned PX4 SIH stack completed the normal-mode Start/Stop,
target-loss, pending-cancel and armed/takeoff/follow/land checks. PX4 entered
Offboard and PixEagle published follower commands at 20 Hz in simulation. The
ground Start guard now requires fresh matching armed and in-air telemetry. The
user accepted the tested normal workflow; no real aircraft qualification is claimed.

## Explicit later scope

Native recording/storage, detailed OSD styling, detailed config editing, model
upload/delete/trust, account administration and runtime diagnostics remain
tracked in [operator coverage](OPERATOR-COVERAGE.md). Redetect, tracker restart and
segmentation remain deliberately deferred. OSD enablement and guarded apply of saved settings are now included in 4b.1; guarded supervised system restart is now included; unsupervised restart remains unavailable. The browser shortcut provides access
to existing dashboard functions without implying native feature parity.

## Validation of the shortcut change

Results and exact source/evidence hashes are recorded in the
[links checkpoint manifest](slice-3b-links-manifest.json). Final debug and release
builds succeeded. The final custom Unit/Integration run passed **411/411** tests
in 266.45 seconds, with the standard Flaky/Network exclusions. An earlier
eight-worker run hit a QGCSignalTransitionTest process-exit timeout after all
four Qt assertions passed; its isolated recheck and the final four-worker suite
passed. The failed run remains archived.

The shortcut pass adds URL-policy, persistence, endpoint isolation and
active-vehicle switching checks, plus private UI/browser handoff checks. No
shared desktop capture or input automation was used. Product and authored-doc
lint passed; two preserved raw-review artifacts retain lint findings: a bare
example URL and a typo false positive inside an audit run identifier. These are
recorded separately rather than changing the raw feedback or claiming a clean
repository-wide lint run.

The previous stock 405/406 run with a passing unchanged isolated recheck, and the
older pinned-backend hygiene failure, remain recorded baseline caveats. There
are no backend source changes in this shortcut pass. Do not claim all platforms,
a fresh stock full suite or hardware qualification from these Linux results.

## Handoff decision

The [slice 4a SIH operator run](OPERATOR-TEST-4A.md) is prepared for the user's
desktop. Its fixed-image and test9 `--check` runs passed backend login,
dashboard availability and QGC UDP delivery; both cleaned up their private
Docker network and services. The private screenshot review and SIH command
probe are separate evidence. The coding agent did not open the shared desktop
app or a real camera.

The user's first desktop attempt found a hard-to-read backend address and an
address-in-use error after interrupting the launcher twice. The address field
and stale-run cleanup were corrected and verified privately; the initial logs
and [raw feedback](reviews/slice-4a-user-feedback-raw.md) were preserved.
The later target-path retry was accepted; this history is retained for provenance.

Ready for the Ethernet camera bench: the [4b.1–4b.2 checkpoint](SLICE-4B-1-2.md) records software validation
and the boundary before physical camera testing. The user accepted the 4b.0
Smart handoff; earlier CPU performance measurements and accuracy limitations
remain in that checkpoint. The [camera scenarios](CAMERA-SCENARIOS.md) explain
RTSP with local tracking, camera-owned tracking, and an external camera control
application. No configuration key migration is required. Provider settings now
carry their actual system-restart tier in the authoritative schema.
