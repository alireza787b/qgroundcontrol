# Native WebRTC network qualification — in progress

The committed upstream-port baseline is `d35cefcc96c97bf06e0f8bbb58d8f8a3781b3a6b`.
The integration branch also merges upstream `c535d1dea7f5ea24dd5a0818c2aa768ab0ddbd0d`
(receiver cleanup on exit) at `badb1441e887c753a72a2128362739ec8fd81544`.
Its 471-suite regression result remains distinct from the changes below, which
are still under verification. Native transport remains unavailable in product
capabilities. No Pi or camera was used.

## Isolated test method

Linux user/network namespaces permit real UDP/TCP shaping without touching the
host's management interface. Retain the caller's non-root UID with
`unshare -Un --map-current-user --keep-caps`; QGC's root refusal stays intact.
The namespace contains loopback and a disposable dummy ICE interface only.
Destroying the namespace removes its interfaces and qdiscs.

The development probe runs the real recorded test9 sender and QGC receiver with
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
  --video ../PixEagle/resources/test9.mp4 --output /tmp/native-link-1mbps \
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
service, network time and presentation. Per-peer encoder targets still share
the canonical aggregate budget. JPEG policy/ACK semantics remain unchanged.

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

The receiver requests a keyframe through the negotiated upstream/RTCP path once
per unresolved dimension change, avoiding per-delta-frame request storms.
This request is not proof of remote receipt or successful decoder recovery.
The new cleanup regression verifies that the custom receiver dies during the
plugin cleanup hook and cannot be recreated by a delayed initialization.

Before enabling native capability, close the remaining weak-link behavior and
exercise real automatic JPEG fallback. Add actual presentation feedback (not
just decoder/encoder counters) and reconcile aggregate budgets across native
and JPEG consumers through the existing budget owner. Do not treat separate
per-transport ceilings as a global guarantee. No additional GitHub build has
been dispatched during this checkpoint.
