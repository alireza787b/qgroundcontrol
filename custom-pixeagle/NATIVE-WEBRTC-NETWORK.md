# Native WebRTC network qualification — in progress

The committed upstream-port baseline is `d35cefcc96c97bf06e0f8bbb58d8f8a3781b3a6b`.
The integration branch also merges upstream `c535d1dea7f5ea24dd5a0818c2aa768ab0ddbd0d`
(receiver cleanup on exit) at `badb1441e887c753a72a2128362739ec8fd81544`.
Its 471-suite regression result remains distinct from the changes below, which
are still under verification. Native transport remains unavailable in product
capabilities. The initial probes used no Pi or camera; later isolated Pi replay
results are recorded below.

## Isolated test method

Linux user/network namespaces permit real UDP/TCP shaping without touching the
host's management interface. Retain the caller's non-root UID with
`unshare -Un --map-current-user --keep-caps`; QGC's root refusal stays intact.
The namespace contains loopback and a disposable dummy ICE interface only.
Destroying the namespace removes its interfaces and qdiscs.

The development probe runs the recorded test11 sender and QGC receiver with
40 ms delay, 5 ms jitter and 0.5% configured random loss. The loopback rate is an
aggregate shaping budget for media and signaling in both directions. Queue
capacity approximates 150 ms at that rate, with an eight-packet minimum. The
retained qdisc counters include additional congestion drops; the configured
random loss is not the total observed loss.

The parameterized repository launcher is
`custom-pixeagle/validation/qualify-native-webrtc-link.sh`. It requires a new
output directory, validates shaping limits and records both repository
revisions, dirty flags, the video checksum, sender/QGC logs and qdisc counters.
The interpreter path preserves venv symlinks. It never installs dependencies,
reads deployment credentials or changes the host interface. For example:

```bash
bash custom-pixeagle/validation/qualify-native-webrtc-link.sh \
  --backend ../PixEagle --qgc-build build/native-webrtc-port \
  --video ../PixEagle/resources/test11.mp4 --output /tmp/native-link-1mbps \
  --ctest tools/.venv/bin/ctest --mbps 1
```

Use `--metadata-delay-ms 50` to test intentionally late metadata. These are
fixture controls, not deployment settings. Existing optional runtime paths are
inherited; normal installed runtimes need no cache-specific paths. Historical
failed probes remain under
`~/.cache/pixeagle-qgc-baseline/native-webrtc-2026-10-19/`.

## Findings and changes under test

- A 1 Mbps run produced a displayed barcode/context mismatch after packet loss.
  A captured damaged image is retained. Pixel corruption/concealment must be
  distinguished from an incorrect RTP association; the gate cannot ignore it.
- A test gap allowed a mismatch arriving during final selection verification to
  escape the earlier assertion. Check again after receiver Stop, and retain
  expected/decoded IDs and an optional diagnostic image on mismatch.
- Native parsebin now reuses the stock codec/parameter-set configuration through
  one shared helper. Native depayloaders request and wait for keyframes when
  those properties are supported. Native decoders discard corrupted frames,
  request synchronization when supported and disable libav corrupt output.
  Explicitly corrupted decoded buffers cannot receive selection provenance.
- Stock transport recovery policy remains unchanged. The generic GStreamer,
  receiver and receiver-component suites passed after the refactor.

The relevant GStreamer controls are documented in
[rtph264depay](https://gstreamer.freedesktop.org/documentation/rtp/rtph264depay.html)
and [avdec_h264](https://gstreamer.freedesktop.org/documentation/libav/avdec_h264.html).

After these guards, one short 10 Mbps and one short 2 Mbps recorded QGC probe
passed for both codecs with exact displayed IDs and selection geometry. These
are preliminary probes, not sustained operating-envelope acceptance. At 1 Mbps,
H.264 needed about 17 seconds to acquire sufficient verified frames and failed
the unchanged 15-second presentation test; VP8 passed that probe. QoS warnings
in earlier attempts are retained, not suppressed. No timeout was extended.

## Next work

1. Retain measured estimator/encoder rates and frame/drop/freshness observations
   throughout congestion, not just successful connection state.
2. Apply the existing automatic profile and delivery-geometry facilities to
   native FPS/resolution adaptation while GCC owns transport bitrate control.
   Reuse the canonical policy; do not add another congestion algorithm or user
   setup switch.
3. Test constrained rates, outages, metadata reordering, multiple clients and
   load; require useful fresh frames and accurate selection or automatic JPEG
   fallback. Keep all failed attempts in the evidence.
4. Renew native/default-off, backend and complete QGC gates after the repair;
   then qualify clean Windows/Linux/Android packages and request hardware.

## Adaptive-workload checkpoint

The sender now reuses `AdaptiveQualityEngine` for startup cadence/scale,
profile bounds and hysteresis, with GCC's bitrate target and measured
submission-to-payloader time as inputs. GCC remains the network estimator and
pacer. Native diagnostics distinguish this pipeline duration from encoder-only
service, network time and presentation. Per-peer encoder ceilings and JPEG
pacing now use the same canonical payload budget. JPEG ACK semantics remain
unchanged. The transitional aiortc browser sender remains outside that
allocation; aggregate browser/native parity is still an open rollout gate.

QGC and the sender explicitly negotiate NACK/PLI feedback; setting the receiver
transceiver property alone did not establish negotiated packet recovery.
The actual-media tests now require confirmed generic NACK negotiation.

The constrained-link probe revealed two separate late-frame cases:

- A decoder may conceal at its previous dimensions after losing a resize
  keyframe. A dimension mismatch cannot receive selection provenance.
- At 1 Mbps, VP8 diagnostics recorded 206 missing metadata joins, 53 dimension
  refusals and no missing RTP mappings. The prior implementation consumed the
  exact RTP identity before the reliable metadata arrived. A decoded frame may
  now wait up to 100 ms for its exact context, before presentation. Stop wakes
  that wait immediately. Expired, consumed, wrong-epoch and missing metadata
  remain invalid; an already presented frame is never relabelled.

RTP delivered-selection retention now covers the canonical age window within
the existing frame/byte bounds. JPEG keeps its ACK-paced three-frame limit.
This fixes avoidable token eviction, without extending selection authority.

The test now uses the same low-latency sink setting as the product controller,
checks metadata dimensions after scaling, keeps checking identity through Stop,
and observes a sustained interval. Earlier clock-synchronized probe failures
remain in the evidence. The numbered fixture advertises the existing delivery
scaling capability rather than weakening the item's size validation.

Backend gates passed 3,727 unit tests and 196 integration tests. Separate runs
with the optional GI runtime passed actual encrypted H.264/VP8 delivery and
negotiated congestion/NACK feedback. The native-disabled source files passed
syntax checks. These checks do not qualify the constrained-link operating
envelope; the sustained matrix still has weak-link failures. The renewed QGC Unit/Integration gate
passed all **471 suites** with `Flaky|Network` exclusions; changed-file lint passed.
Native product capability stays disabled until those gates pass.

## Planned operator sequence

After the local checkpoint passes, use the Pi with **recorded video first**:
verify Core/JPEG without the optional runtime, then the GStreamer provider with
the same source, identities and controls. `resources/test11.mp4` provides about
290 seconds of 720p footage, avoiding the 9.4-second test9 loop during an
operator session. Confirm its suitability visually before the handoff.

Request the cooled Pi only when that setup is ready. Camera acceptance follows
recorded acceptance; Pixhawk connectivity follows separately. Windows builds
and operator tests follow that software/Pi checkpoint. No camera profile,
service, flight gains or hardware settings have changed in this slice.

The latest sustained matrix passed both codecs at 2/5/10 Mbps. At 0.5 Mbps,
startup/acquisition failed; at 1 Mbps, VP8 still failed freshness/selection after
resizing. No timeout or selection-age limit was extended to make these pass.
The missing-metadata fix reduced 1 Mbps VP8 join refusals from hundreds to
roughly one per short run; geometry recovery remains a separate limitation.
A 50 ms intentionally delayed-metadata probe passed both codecs at 10 Mbps.
These are short controlled runs, not sustained Pi or field qualification.

The current receiver requests a keyframe through the negotiated upstream/RTCP
path once per interrupted prediction-reference generation. A verified decoded
dimension mismatch fails with `decoded_geometry` and invokes the controller's
JPEG fallback. Neither a request nor its transmission proves decoder recovery.
The new cleanup regression verifies that the custom receiver dies during the
plugin cleanup hook and cannot be recreated by a delayed initialization.

Before enabling native capability, close the remaining weak-link behavior and
exercise real automatic JPEG fallback. Do not treat configured payload ceilings
as a wire-rate or measured-capacity guarantee. No additional GitHub build has
been dispatched during this checkpoint.

## Presentation and access-unit repair

An additional 20-second-per-codec test using the longer test11 recording
exposed an H.264 barcode mismatch under loss at 10 Mbps. The retained image
contains concealed horizontal slices. GStreamer 1.24's H.264 depayloader defaults
to NAL output, where `wait-for-keyframe` is ineffective. Native parsebin now
negotiates complete AVC access units before the shared stock parser hooks run.
A query regression verifies AU negotiation; stock receivers keep their policy.
The renewed 10 Mbps test passed both codecs without mismatched contexts.
The 1 Mbps test still exposes VP8 resize-recovery/selection failures, and H.264
has multi-second verified-frame gaps. Neither rate is a released operating claim.

QGC now sends optional presentation observations on the existing authenticated
frame-challenge message. Only distinct Qt surface swaps count as presentations;
decode/queue admission is separate. Immutable receipts preserve identity across
cache eviction. The sender requires fresh healthy receiver evidence before
raising its workload, and sustained slow-renderer evidence reduces cadence.
No new route, setting, credential store or selection authority is introduced.

Native admission and JPEG pacing share the existing payload-budget owner.
Encoder reservations survive timed-out teardown and leave a JPEG share; earlier
JPEG reservations drain before a native sender begins. This bounds configured
payload targets, not protocol overhead or measured link capacity. The temporary
aiortc browser provider remains outside that accounting until its migration.

## Authenticated controller fallback checkpoint

The existing loopback-only `tools/native_integration_fixture.py` now accepts
`--native-webrtc normal|reject|missing_metadata|wrong_dimensions` for qualification only.
Its default remains disabled; the product capability remains disabled too.
The fixture has no aircraft command path. It uses the production session auth,
capability response, signaling manager, sender, publisher and JPEG route.
Native qualification modes retain real analysis/selection geometry.

Run the fixture from the backend repository with its optional runtime loaded:

```bash
PYTHONPATH=src .venv/bin/python tools/native_integration_fixture.py \
  --port 8198 --no-aircraft --instance-id native-controller-check \
  --native-webrtc missing_metadata
```

From the QGC repository, using its complete native runtime:

```bash
QGC_PIXEAGLE_WEBRTC_FIXTURE_URL=http://127.0.0.1:8198 \
QGC_PIXEAGLE_EXPECT_FALLBACK=1 \
tools/.venv/bin/ctest --test-dir build/native-webrtc-port \
  -R '^PixEagleVideoControllerTest$' -V
```

Use `normal` with `QGC_PIXEAGLE_EXPECT_FALLBACK=0` to verify native playback.
For `reject`, also set `QGC_PIXEAGLE_EXPECT_FAILURE_STAGE=remote_media`; the
intentional source/bus warnings must be captured and verified, not hidden.
Stop only this owned fixture with Ctrl-C afterwards. No hardware is involved.

All three cases passed. QGC selected WebRTC automatically; refusal recovered to
fresh selectable JPEG in about two seconds. Missing metadata recovered through
the unchanged presentation watchdog in about twelve seconds. These durations
are local test totals, not field latency guarantees. The test checks unchanged
media identity, actual swap observations, fresh geometry and explicit Stop with
no retry. Initial fixture omissions (selection geometry and scoped signal
cleanup) failed and were corrected; their logs remain in the evidence.

The renewed full QGC gate passed 471 suites in 210 seconds; backend unit plus
route-inventory passed 3,793 tests with seven optional-runtime skips. The full
backend integration gate passed 196 tests. The subsequent focused streaming,
media-health, route and configuration run passed 422 tests with seven skips;
36 actual encrypted-media/presentation tests passed with the optional runtime.
Schema remains current at 43 groups / 612 parameters.
Dashboard regression passed 64 suites / 493 tests, production build and lint.

Next extend fallback/restart/revocation tests across constrained links and close
VP8 resize recovery. Finish browser/native aggregate parity and clean platform
runtime probes before ordinary native capability is enabled. Request the Pi
for recorded-video qualification only after that local checkpoint is ready.

## Packet-loss integrity and decoder fallback

An additional weak-link probe retained a VP8 image with a damaged barcode:
its pixels read 741 while the exact RTP metadata identified 749. The receiver
was using the Linux VA-API decoder, which did not flag all concealed output.
The barcode comparison remains unchanged. A passing metadata association alone
cannot establish intact pixels after prediction-reference loss.

Native reception now observes packet continuity before depayloading. An
incomplete access unit, source change or discontinuity invalidates dependent
selection provenance until a complete clean sync frame is observed. A keyframe
with a missing fragment cannot reestablish the reference. The guard also checks
the generation after waiting for metadata, so concurrent loss cannot authorize
an obsolete reference. It neither retransmits packets nor replaces GStreamer's
congestion control. Unit fixtures cover sequence wrap, missing fragments/frames,
late and duplicate packets, corruption, source changes and clean recovery.

The standard upstream keyframe request uses the parsed source pad's
`gst_pad_send_event` direction. An earlier test used the wrong direction; its
warnings remain in the logs. If verified metadata dimensions differ from the
decoded image, QGC reports `decoded_geometry` and switches automatically to
JPEG. Diagnostics retain expected/decoded sizes and unsafe-reference counts.

The sender/receiver use the framework's default 200 ms jitter latency. Encoder
rate-control buffers follow the pinned GStreamer webrtcsink recipe: x264 VBV
120 ms and VP8 CBR buffers 100/120/150 ms, with 15-bit VP8 picture IDs. These
are media defaults, not longer selection or watchdog deadlines. The H.264
zero-latency aggregation experiment was reverted because it introduced
ambiguous association; the failed actual-media run remains evidence.

Fresh guarded probes passed:

- `reference-recovery-10mbps-loss/`: both codecs, 20 seconds each, test11,
  40 ms delay / 5 ms jitter / 0.5% configured random loss. H.264 verified 246
  presented frames with a largest 962 ms gap; VP8 verified 261 with a 476 ms
  gap. No barcode mismatch occurred. These are short development observations.
- `reference-controller-normal-2mbps/`: authenticated automatic native playback.
- `reference-geometry-fallback-1mbps/`: deliberately wrong metadata dimensions
  caused the named decode failure and recovered to fresh selectable JPEG.
- `reference-metadata-fallback-1mbps/`: connected native transport without exact
  metadata recovered to JPEG; total test runtime 13.4 seconds.
- `reference-metadata-fallback-05mbps/`: startup fallback passed in 10.4 seconds.
  Native never connected; this is not evidence of metadata-loss recovery.
- `reference-final-qgc.log`: all 471 Unit/Integration suites passed with the
  standard `Flaky|Network` exclusions, 210 seconds. Changed-file lint and
  native-disabled source factory/receiver syntax checks also passed.

VA-API VP8 still emits sticky-event ordering warnings. Keep them visible for
platform qualification. Native operation at 0.5/1 Mbps remains unqualified;
fallback success is separate evidence from sustained native quality. A later
2 Mbps run also failed strict VP8 identity; the short passes above do not
supersede that failure.

The same isolated harness supports controller failure tests without a video:

```bash
bash custom-pixeagle/validation/qualify-native-webrtc-link.sh \
  --backend ../PixEagle --qgc-build build/native-webrtc-port \
  --controller-mode wrong_dimensions --output /tmp/native-fallback-1mbps \
  --ctest tools/.venv/bin/ctest --mbps 1
```

Controller fixtures use synthetic pixels and production auth/media routes;
their manifest distinguishes them from recorded-video evidence. No aircraft
commands are available. Product native capability remains disabled.

## Decoder-input continuity repair

The subsequent `cadence-recorded-2mbps/` run failed VP8 identity (879 versus
887), despite the RTP reference guard. A diagnostic copied plaintext encoded
access units before transport: independent software decoding recovered all
83 barcodes correctly, while QGC's received image failed (707 versus 715).
Forcing a software receiver decoder also failed (644 versus 676). These
observations do not justify attributing the defect only to VA-API or relaxing
the barcode check.

Inspection found that the display branch's bounded leaky queue can discard
complete encoded reference frames **after** the RTP/parser guard. Verification
now runs immediately before decoding, after that queue. Access-unit ordinals
detect an internal queue drop even with continuous RTP sequence numbers.
Dependent units are refused until a complete clean sync unit; they cannot
pollute the decoder's reference state. The queue remains bounded and stock
receiver policy is unchanged. An independent unit fixture drops a complete
reference at this boundary and requires clean recovery.

`--controller-mode recovery` additionally exercises the real manager's bound
client, source replacement, brief cached capture, explicit sign-out and new
session, followed by Stop. Its synthetic controls use an atomic local file;
there is no deployment control API. The corrected lifecycle fixture passed in
`controller-recovery-v3-10mbps/` (8.2 seconds). It waits for logout completion
before requesting a new session, just as the operator UI's busy state requires.
Source replacement may retire by status refresh or the sanitized `remote_media`
error; only those expected lifecycle warnings are allowed in this test.

The fresh `decoder-input-link-10mbps/` probe passed both codecs for 20 seconds
each without a barcode mismatch. H.264's largest verified gap was 2,320 ms and
VP8's 871 ms. The 2 Mbps run passed VP8 identity (213 frames / 892 ms largest
gap) but failed H.264's ending freshness check. Keep the overall 2 Mbps result
failed: a successful association cannot be described as uninterrupted video.
No freshness or watchdog limit was extended.

`decoder-input-fallback-1mbps/` passed the deliberately wrong-dimension failure
and automatic selectable-JPEG recovery through the actual manager-bound client.
Focused GStreamer/receiver/component suites and native-disabled DecodingBranch
syntax passed. The full QGC regression renewal is recorded below.

The matching backend measures completed associated access units over one-second
windows. Its optional `associated_encoded_fps` is distinct from the requested
policy FPS and actual Qt presentations. Stale, idle and changed-profile samples
expire. This avoids lowering detail for frames slow analysis never produced,
while retaining the requested-cadence pipeline-overload guard. Backend revision
`902a59b` passes 3,798 unit/route tests (seven optional skips), 196 integration
tests and 81 focused streaming tests with the optional runtime; schema and
changed-file lint pass. This does not close platform or Pi performance gates.

Final renewal for the decoder-input repair passed all **471 QGC suites** with
standard exclusions in 209 seconds (`decoder-input-final-qgc.log`). The native
build, changed-file lint and native-disabled decoder-branch syntax pass.
The harness rejects fractional durations and caps its per-codec observation
at 40 seconds to fit the existing 120-second integration-test budget. This
execution limit changes no media freshness, Stop or recovery deadline.

The next hardware checkpoint uses the cooled Pi and a long recorded file,
with the camera and Pixhawk off. Back up and preserve its custom profile;
compare Core/JPEG and optional GStreamer in an isolated test deployment.
This is development qualification, not enabling native capability in ordinary
installations. Low-bandwidth freshness, browser/provider parity and clean
Windows/Android runtime qualification remain open. No release build, public
QGC PR or Pi deployment was performed by these local tests.

## Pi cooling and additional local tests — 2026-10-19

The Pi's isolated Core replay presented 720p but failed sustained freshness
during runtime-installation traffic. Its optional build was cleanly cancelled
when cooling became unavailable; its venv transaction rolled back, the Cargo
cache remains, and all 41 backed-up configuration files match the installed
profile. OpenCV 4.13.0 and installed source revision 664171e00 are preserved.
Heavy Pi qualification waits for a fan. Its normal service and owned replay
are stopped; no compiler remains. Camera and Pixhawk are not needed for the
next recorded-video comparison. Private resume instructions and evidence are
under `~/.cache/pixeagle-qgc-baseline/native-webrtc-2026-10-19/pi/`.

The full-controller source/freeze/sign-out recovery fixture passed at 1 Mbps.
At 0.5 Mbps it obtained fresh JPEG after native fallback, failing its expected
native-transport assertion; later recovery steps did not run. Neither fixture
establishes sustained camera quality or Pi performance.

A candidate change applied receive-side reference discontinuities in decoder
order, allowing an earlier intact queued access unit despite later packet loss.
It passed a deterministic queued-frame check and a 2 Mbps recorded run, but
failed the stronger 10 Mbps recorded pixel-identity test: H.264 barcode 103 was
presented with context 106. **The candidate was discarded; the conservative
reference guard is retained unchanged.** These experimental results do not
qualify native selection or establish that receive-side lookahead alone was
the cause. The failed logs and candidate-independent reproduction are retained
in `ordered-decoder-*` and `reference-lookahead-proof/` under the private
2026-10-19 evidence root.

The shaping helper now identifies deliberate packet loss to the test. Only
the specific expired-NACK warning is allowed in that case and remains in logs.
Other unexpected warnings, wrong pixels and unsafe selections still fail.
Native product capability remains disabled; exact pixel provenance under loss,
weak-link freshness, Pi performance and clean platform runtimes remain open.

## Isolated Pi replay comparison after setup — 2026-10-19

At the operator's request, work resumed without waiting for a fan. The standard
optional-runtime reconciliation succeeded in a separate candidate environment
under a temporary systemd CPU quota of 35%. The quota constrained compilation
only and is no longer active. Installed OpenCV, the normal source revision
`664171e00` and all 41 backed-up configuration files remain unchanged.
The candidate source is PixEagle `9f90e28`; the tested QGC binary is `333f9773d`.

The actual production replay worker served looping recorded test11, with camera
and MAVLink disabled, circuit breaker enabled and command preview active. Core
used the installed interpreter without GI; native used the candidate interpreter
and explicit process-local experimental admission. The real QGC controller
signed in and required fresh, selectable Qt presentations for 20 seconds:

| Transport | Presented FPS | Largest presentation gap | Largest frame-age bound | Peak Pi temperature |
| --- | ---: | ---: | ---: | ---: |
| Core / JPEG | 6.37 | 363 ms | 455 ms | 61.15°C |
| Native H.264 | 12.41 | 336 ms | 771 ms | 65.0°C |

These are unshaped short tests on shared Wi-Fi. API, signaling and JPEG traversed
an SSH loopback forward; native RTP used direct ICE connectivity. They are not
equal-path bandwidth benchmarks, true camera-to-screen latency measurements or
independent pixel-identity tests. Native diagnostics recorded active GCC/TWCC,
960×540 delivery and adaptation from 12 to 15 requested FPS, with no metadata
drops in the retained run. Measured presentation FPS is reported separately.

Historical thermal flags remain `0xe0000`; current throttling bits were clear.
Both workers stopped cleanly. A subsequent isolated operator replay has a
75°C cutoff and a maximum ten-minute lifetime; it opens recorded video and CSRT
in Linux QGC without enabling aircraft commands. The normal field service stays
stopped during these tests. No saved field profile is replaced.

Evidence is retained under the private `pi/` directory: `core-idle-controller.log`,
`native-idle-controller.log`, each replay's `thermal-run.json`, and
`qgc-desktop/native-qgc.log`. All 471 QGC Unit/Integration suites passed with
standard exclusions in the preceding final renewal (`post-pi-final-qgc.log`).
The earlier installation-load failure and rejected reference-order experiment
remain failed evidence. Short Pi success does not close provenance under loss,
weak-link freshness, browser parity, sustained performance or platform gates.

The guarded operator replay was intentionally capped at 75°C and ten minutes.
It reached 75.45°C after 131.37 seconds and shut down cleanly; QGC had already
shown the video and PixEagle panel before the cutoff. This confirms the operator
path and the thermal guard, not sustained Pi capacity. A fan or equivalent
cooling is required before longer replay, constrained-link tests or physical
qualification. The normal `pixeagle.service`, saved field profile and installed
source were not changed.

## Cooled Pi continuation and operator quality report — 2026-10-19

The Pi returned with cooling, camera and Pixhawk connected. Its normal service
had restarted with the saved flight-command circuit breaker disabled. That
service was stopped before isolated media tests; no saved flight or network
configuration was changed and no aircraft command was sent. The camera answered
ping and a separate TCP RTSP probe received/parsed 60 H.264 access units in
2.33 seconds without errors. This proves receive/parse connectivity, not camera
presentation quality or motor behavior. The MAVLink router opened its UART;
aircraft identity and command delivery were not qualified in this checkpoint.

The cooled recorded replay completed its ten-minute guard: 601.76 seconds
including graceful shutdown, peak 60.6°C, and no throttling. All 41 saved
configuration files still match the earlier backup. Cooling is no longer the
blocker for these short media tests. The original product installation and
native-capability gate remain unchanged.

The operator reported acceptable motion but progressive pixelation. Preserve
that raw feedback separately from automated results. Across a 119.7-second
media-health observation, capture was never stale (115 samples, maximum source
age 573 ms), but 111 samples reported 320×180 native delivery. The encoder target
ranged from 64 kbps to 1.187 Mbps. Receiver feedback included renderer/age
pressure; a healthy source therefore does not establish acceptable delivery.
The strict production-controller checks failed freshness/selectability, and
remain failed evidence despite the earlier short passes.

At the recording's loop boundary, the sender reported `source_changed`. QGC
retired that transport, recovered through JPEG, and the refreshed identity
allowed a new native peer. This is distinct from a camera RTSP reconnect or
backend restart. It does not excuse the earlier loss of detail.

Detailed `rtpgccbwe` logs show delay-overuse decisions reducing the target rate.
Temporarily disabling Pi Wi-Fi power saving did not remove these decisions or
the strict controller failure. The original power-save setting was restored;
no persistent network workaround was applied. Missing padding support in this
runtime is an investigation lead, not an established cause or a reason to
override congestion control.

Two independent local comparisons narrow the next investigation:

- Ubuntu sender/QGC receiver, recorded test11, no shaping: H.264 and VP8 both
  passed exact presented IDs for 20 seconds each, held 960×540, and produced no
  estimator decreases. Largest verified-frame gaps were 92/102 ms. This fixture
  does not include the full production capture/tracker workload.
- Pi-local H.264, synthetic 1280×720 input with GCC, 20-second observation:
  exact decoded mappings passed at 960×540 with no estimator decreases. It
  exercises ICE/DTLS/SRTP and decoding, not QGC presentation, recorded detail or
  network capacity.

Next compare the actual Pi recorded sender over a wired path while retaining
Wi-Fi management. Wi-Fi remains a required operating target; the comparison
isolates path jitter from production workload and receiver behavior. Do not
force a high bitrate, relax freshness/provenance gates, deploy the candidate to
the field service or start platform release builds on these results.

Private evidence: `pi/native-cooled-recorded-v2/`,
`pi/cooled-controller.log`, `pi/cooled-gcc-ready-controller.log`,
`pi/cooled-wifi-off-controller.log`, `pi/native-cooled-gcc-v3/`,
`pi/native-cooled-wifi-v4/`, `pi/pi-loopback-media.*`, and
`local-comparison-v1/receiver.log`. An initial diagnostic test ran before API
readiness, and a local setup attempt lacked the optional GI runtime; neither
is media-quality evidence. All failed setup and media runs remain retained.

## Repaired cadence, real datalink and camera continuation — 2026-10-19

Backend `15ae6d2` removes raw-cap renegotiation on adaptive FPS-only changes;
dimensions still renegotiate and PTS/duration retain actual cadence. The
regression failed before the repair. Renewed backend gates: 328 streaming
tests (seven optional skips), 74 route/config tests, 25 optional-runtime peer
tests, schema and syntax checks. Existing BLE001/PIE807 lint findings were
reproduced from baseline; they were not hidden by modifying production logic.
QGC implementation/binary is unchanged from the earlier tested receiver.

The Windows GCS bridges its two Ethernet interfaces. Ubuntu temporarily adds
192.168.0.203/24 on its Ethernet interface while retaining Wi-Fi management;
the Pi is 192.168.0.226 on the datalink. This is a path through a radio/datalink,
not a direct Pi-to-laptop cable or loss-free laboratory wire. An initial ICE
run still chose Wi-Fi despite the backend address; interface counters caught
that. A private ICE-binding experiment crashed before media and is rejected,
never installed in product code.

Later tests used the normal sender with an automatically removed, process-test
UDP path exclusion to establish actual Ethernet media delivery. TCP SSH/API
management stayed on Wi-Fi. No persistent network workaround was deployed.

| Test | Result and scope |
|---|---|
| Recorded datalink, repaired sender, 20 s | Passed: 13.51 presented FPS, largest gap 263 ms, maximum conservative frame-age bound 631 ms |
| Recorded datalink, visible 119 s observation | No stale source samples; dimensions moved between 640×360/960×540 and reached 1280×720. Operator: “yes it loosk perfect smoothand quality good.” This does not establish uninterrupted 720p/20 FPS. |
| Recorded Wi-Fi, repaired sender, 20 s | Passed: 12.11 presented FPS, largest gap 336 ms, maximum age bound 631 ms. A longer GUI observation was incomplete; do not call it sustained Wi-Fi acceptance. |
| Camera native TCP decode, 30 s | 1920×1080, 29.88 decoded FPS, maximum sample gap 176 ms, no reported decode errors, peak 52.9°C. No QGC/encoder evidence in this baseline. |
| Camera Wi-Fi, QGC 20 s | Passed: 11.85 presented FPS, largest gap 418 ms, maximum age bound 706 ms |
| Same camera Wi-Fi, visible 120 s | Sustained quality failed: mostly 320×180, target reached 64 kbps, source remained fresh; Pi peaked 73.8°C before requested shutdown. Short freshness success does not establish picture quality. |
| Camera datalink, QGC 20 s | Passed: 13.42 presented FPS, largest gap 302 ms, maximum age bound 651 ms. Longer visible run was stopped by the 75.45°C guard; sustained acceptance remains open. |

The first camera datalink attempt caught a source identity change during initial
capture startup, before its fixed-identity observation began. Its failed log is
retained. A repeat after source readiness passed; no test assertion or product
freshness guard was relaxed. A requested 120-second controller observation was
also refused by the fixture's documented 20-second bound; this is invalid test
setup, not a measured media failure. Failed GUI polling at guarded shutdown is
likewise not a completed long-duration result.

Backend `73e6057` documents actual user parameters, internal adaptation
thresholds, startup, cooldown, the current per-peer bitrate allocation and
limits of the shared budget. No tuning/default value changed. Wi-Fi is not
established as a universal Raspberry Pi defect; disabling power saving alone
did not solve the earlier failure. Backend `f384607` extends the existing
private harness with explicit RTSP capture-only import. Its 119 harness/API/
config tests and changed-file Ruff checks passed. The camera bench disables
MAVLink, camera-owned tracking/control and aircraft commands; it cannot qualify
those physical behaviors. It preserves the installed field profile separately.

Evidence lives under `native-webrtc-2026-10-19/pi/`, notably
`native-datalink-cadence-v8/`, `native-wifi-cadence-v9/`,
`camera-source-baseline/`, `native-camera-cadence-v10/`,
`native-camera-datalink-v11/`, and `qgc-desktop/`. Private files may contain
credentials; do not publish the cache or manifests.

Keep native capability disabled. Sustained Wi-Fi quality, packet-loss pixel
identity, lowest-bandwidth fallback, thermal operating range, browser/provider
parity and platform runtime packages remain open. The installed Pi service,
saved geometry/gains/safety policy and main checkout are not updated by these
isolated candidates. No release/Actions build was triggered from this evidence.

### Live-camera operator review, v12

After the operator improved cooling, a fresh isolated datalink session started
at 41.9°C. The operator accepted the picture as clear/smooth after several
initial waiting interruptions. The logs contain one receiver start and one
native peer, so repeated stream reconnects are not established as their cause.
Startup presentation needs its own timing evidence; do not label the cause
resolved or extend freshness checks to suppress the symptom.

The retained 119.12-second media-health window has 112 native samples, no stale
source, and delivery rising from 960×540 to 1280×720. The last receiver feedback
reports 18.41 presented FPS without pressure and no metadata drops. Raw user
feedback is separate in `pi/native-camera-review-v12/operator-feedback-raw.md`.
The full workload still hit 75.45°C after 203.47 seconds and the guard stopped
it cleanly. Its test filter was removed and verified. QGC waiting after that
deliberate stop is expected; the early startup interruptions remain a separate
open issue. Visual acceptance does not close thermal, impaired-link, provenance
or platform qualification. No implementation/default change accompanied this
review.

### October 20 applied-profile retest

Backend candidate `8d16dad` retains canonical quality limits in the inhibited
RTSP harness; factory behavior is unchanged. The installed Pi profile was saved
through `ConfigService` with backup: maximum delivery15 FPS, startup10 FPS,
explicit8000 kbps payload allocation, preprocessing/blur/CLAHE disabled. Its
vertical mount, camera engine, flight gains and field safety choices were
preserved. Only video settings were applied; native source upgrade remains in
the separate qualification worktree, not the installed service.

With actual datalink media, v15 passed the20-second controller check at12.04
presented FPS during ramp, largest gap115 ms and maximum conservative age589 ms.
The connected GUI119.78-second sample contains115 observations, no stale source,
delivery reaching1280×720, final14 presented FPS and no metadata drops or receiver
pressure. Pi58.95°C/no throttling at handoff. It is not yet a finished soak.
Earlier v14 completed10 minutes but had a viewer only for its short controller
test; retain it as a thermal runtime observation, not sustained media acceptance.

Core Pi with optional Python GI unavailable also passed QGC JPEG playback and
selection20 seconds (5.04 FPS); this is fallback compatibility, not WebRTC
performance parity. Backend122 harness/API/config tests and109 focused factory/
adaptation tests pass; no QGC implementation or binary changed. Saved laptop
NetworkManager profile now retains192.168.0.203/24, keeping Wi-Fi default routes.
The private Desktop/PixEagle-video-test-2026-10-20 handoff contains the applied
config, rollback, diff and results. Native/product/platform and startup gates
remain open; no new release or Actions run follows from these short results.
