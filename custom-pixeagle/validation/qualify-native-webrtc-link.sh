#!/usr/bin/env bash
# Real sender/QGC traffic in a disposable network namespace; never shape the host link.
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: qualify-native-webrtc-link.sh --backend DIR --qgc-build DIR --video FILE --output DIR
       [--python FILE] [--ctest FILE] [--mbps 0.5|1|2|5|10] [--loss-percent N]
       [--delay-ms N] [--jitter-ms N] [--duration-ms N] [--metadata-delay-ms N]

Use the backend interpreter with the optional GStreamer runtime. Existing
GST_PLUGIN_PATH/GI_TYPELIB_PATH are inherited. Requires Linux user/network
namespaces, iproute2 and a built native-enabled QGC integration test. No sudo,
Pi, camera, aircraft, deployment credentials or external network is used.
The rate limits aggregate media and signaling in both directions. Qdisc
counters include congestion drops in addition to configured random loss.
The output directory must be new so failed evidence cannot be overwritten.
USAGE
}

backend='' build='' video='' output='' python='' ctest_bin=''
mbps=1 loss=0.5 delay=40 jitter=5 duration=10000 metadata_delay=0
while (($#)); do
    case "$1" in
        --backend) backend=${2:?}; shift 2 ;;
        --qgc-build) build=${2:?}; shift 2 ;;
        --video) video=${2:?}; shift 2 ;;
        --output) output=${2:?}; shift 2 ;;
        --python) python=${2:?}; shift 2 ;;
        --ctest) ctest_bin=${2:?}; shift 2 ;;
        --mbps) mbps=${2:?}; shift 2 ;;
        --loss-percent) loss=${2:?}; shift 2 ;;
        --delay-ms) delay=${2:?}; shift 2 ;;
        --jitter-ms) jitter=${2:?}; shift 2 ;;
        --duration-ms) duration=${2:?}; shift 2 ;;
        --metadata-delay-ms) metadata_delay=${2:?}; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
[[ -d $backend && -f $build/CTestTestfile.cmake && -f $video && -n $output ]] || { usage >&2; exit 2; }
qgc_repo=$(realpath "$(dirname "$0")/../..")
backend=$(realpath "$backend")
build=$(realpath "$build")
video=$(realpath "$video")
output=$(realpath -m "$output")
python=${python:-$backend/.venv/bin/python}
ctest_bin=${ctest_bin:-$(command -v ctest)}
[[ -x $python && -x $ctest_bin ]] || { echo 'Python or CTest not executable' >&2; exit 2; }
python=$(realpath -s "$python")
ctest_bin=$(realpath -s "$ctest_bin")
[[ $(id -u) != 0 ]] || { echo 'Run as your normal user; QGC must not run as root.' >&2; exit 2; }
for tool in unshare ip tc; do command -v "$tool" >/dev/null; done
"$python" -c 'import cv2, gi'
"$python" - "$mbps" "$loss" "$delay" "$jitter" "$duration" "$metadata_delay" <<'PY'
import math, sys
rate, loss, delay, jitter, duration, metadata_delay = map(float, sys.argv[1:])
if not (all(map(math.isfinite, (rate, loss, delay, jitter, duration, metadata_delay)))
        and rate in (0.5, 1, 2, 5, 10) and 0 <= loss <= 20
        and 0 <= jitter <= delay <= 1000 and 0 <= duration <= 120000
        and metadata_delay.is_integer() and 0 <= metadata_delay <= 250):
    raise SystemExit('Invalid shaping/test limits')
PY

if [[ -z ${PIXEAGLE_LINK_PARENT_NETNS:-} ]]; then
    [[ ! -e $output ]] || { echo 'Choose a new output directory.' >&2; exit 2; }
    export PIXEAGLE_LINK_PARENT_NETNS
    PIXEAGLE_LINK_PARENT_NETNS=$(readlink /proc/self/ns/net)
    exec unshare -Un --map-current-user --keep-caps bash "$0" \
        --backend "$backend" --qgc-build "$build" --video "$video" --output "$output" \
        --python "$python" --ctest "$ctest_bin" --mbps "$mbps" --loss-percent "$loss" \
        --delay-ms "$delay" --jitter-ms "$jitter" --duration-ms "$duration" --metadata-delay-ms "$metadata_delay"
fi
[[ $(readlink /proc/self/ns/net) != "$PIXEAGLE_LINK_PARENT_NETNS" ]] || exit 2
mkdir "$output"
"$python" - <<'CHECK_NS'
import json, subprocess
links = json.loads(subprocess.check_output(['ip', '-j', 'link', 'show']))
if len(links) != 1 or links[0]['ifname'] != 'lo':
    raise SystemExit('Refusing to shape a namespace with existing network interfaces')
CHECK_NS
ip link set lo up
ip link set lo mtu 1500
ip link add native-test type dummy
ip addr add 10.203.0.1/24 dev native-test
ip link set native-test up
limit=$("$python" -c 'import math,sys; print(max(8,math.ceil(float(sys.argv[1])*1e6*.15/8/1200)))' "$mbps")
tc qdisc add dev lo root netem delay "${delay}ms" "${jitter}ms" loss "${loss}%" rate "${mbps}mbit" limit "$limit"
"$python" - "$output" "$backend" "$build" "$video" "$mbps" "$loss" "$delay" "$jitter" "$duration" "$qgc_repo" "$metadata_delay" <<'PY'
import datetime, hashlib, json, pathlib, subprocess, sys
out, backend, build, video, rate, loss, delay, jitter, duration, qgc, metadata_delay = sys.argv[1:]
def revision(path):
    return subprocess.check_output(['git', '-C', path, 'rev-parse', 'HEAD'], text=True).strip()
def dirty(path):
    return bool(subprocess.check_output(['git', '-C', path, 'status', '--porcelain'], text=True))
digest = hashlib.sha256()
with open(video, "rb") as source:
    for block in iter(lambda: source.read(1024 * 1024), b""):
        digest.update(block)
record = dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              backend_revision=revision(backend), backend_dirty=dirty(backend),
              qgc_revision=revision(qgc), qgc_dirty=dirty(qgc),
              qgc_build=build, video=video, video_sha256=digest.hexdigest(),
              aggregate_rate_mbps=float(rate), configured_loss_percent=float(loss),
              delay_ms=float(delay), jitter_ms=float(jitter), minimum_codec_duration_ms=float(duration), injected_metadata_delay_ms=int(metadata_delay))
pathlib.Path(out, 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')
PY
export PYTHONPATH="$backend/src${PYTHONPATH:+:$PYTHONPATH}"
cd "$backend"
"$python" -c 'import logging,runpy; logging.basicConfig(level=logging.INFO); runpy.run_path("tools/qualify_gstreamer_native_server.py",run_name="__main__")' \
    --port 8197 --video "$video" --metadata-delay-ms "$metadata_delay" > "$output/sender.log" 2>&1 &
fixture_pid=$!
cleanup() {
    tc -s qdisc show dev lo > "$output/qdisc.txt"
    kill -TERM "$fixture_pid" 2>/dev/null || true
    wait "$fixture_pid" 2>/dev/null || true
}
trap cleanup EXIT
"$python" - <<'PY'
import time, urllib.request
end = time.monotonic() + 10
while True:
    try:
        with urllib.request.urlopen('http://127.0.0.1:8197/qualification/context', timeout=2) as response:
            assert response.status == 200
        break
    except Exception:
        if time.monotonic() > end:
            raise
        time.sleep(.1)
PY
export QGC_WEBRTC_TEST_SIGNALING_URL=ws://127.0.0.1:8197/ws/webrtc_signaling
export QGC_WEBRTC_FAILURE_IMAGE="$output/failure"
export QGC_WEBRTC_TEST_DURATION_MS=$duration
"$ctest_bin" --test-dir "$build" -R '^QGCWebRtcVideoSourceTest$' -V \
    --output-junit "$output/qgc.xml" > "$output/qgc.log" 2>&1
cat "$output/qgc.log"
