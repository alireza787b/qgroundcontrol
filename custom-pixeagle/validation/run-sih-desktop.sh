#!/usr/bin/env bash
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cache=${QGC_BASELINE_CACHE:-"$HOME/.cache/pixeagle-qgc-baseline"}
base=$cache/slice-4-2026-09-26
stack_script=${PIXEAGLE_SIH_STACK_SCRIPT:-"$cache/slice-4-2026-09-26/sih_live_stack.sh"}
demo=${PIXEAGLE_SIH_DEMO_DIR:-"$cache/slice-4-operator-desktop"}
dashboard=/home/alireza/PixEagle-qgc-integration/dashboard
backend=$(dirname -- "$dashboard")
dashboard_build=$base/dashboard-build
binary=${PIXEAGLE_SMOKE_BINARY:-"$repo/build/pixeagle-custom-release/Release/PixEagle-QGroundControl"}
image=sha256:ecfaf2358be090447ff9948397075ce76cefff3a152fbfab006a2d26495ea14b
px4_image=px4io/px4-sitl@sha256:fd6d93dc2705482aeb64ea26fdf16185d8a511010fdc53e26305f10d91855865
px4_name=pixeagle-slice4-px4-operator
stack_name=pixeagle-slice4-stack-operator
network_name=pixeagle-slice4-operator
owner_label=pixeagle.slice4.operator
check_only=false
video=fiducial
while [[ $# -gt 0 ]]; do
    case $1 in
        --check) check_only=true; shift ;;
        --video)
            [[ $# -ge 2 && ( $2 == test9 || $2 == target-path ) ]] || {
                echo "Choose --video test9 or --video target-path." >&2; exit 2;
            }
            video=$2
            shift 2 ;;
        *) echo "Usage: $0 [--check] [--video test9|target-path]" >&2; exit 2 ;;
    esac
done
runtime=$base/sih-live-v1
if [[ $video == test9 ]]; then runtime=$base/sih-test9-v1; fi
if [[ $video == target-path ]]; then runtime=$base/sih-target-path-v1; fi
prepared_runtime=$runtime
if [[ $video == target-path ]]; then prepared_runtime=$base/sih-live-v1; fi

[[ -x $binary ]] || { echo "Build the custom Linux Release app first." >&2; exit 1; }
[[ -f $dashboard_build/index.html && -x $dashboard/node_modules/.bin/serve ]] || {
    echo "The prepared local dashboard build is missing." >&2
    exit 1
}
[[ -f $prepared_runtime/credentials.json && -f $prepared_runtime/fiducial.jpg && -f $stack_script ]] || {
    echo "The prepared private SIH runtime or stack script is missing." >&2
    exit 1
}
if ! $check_only && [[ -z ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
    echo "Run this command from your desktop session." >&2
    exit 1
fi
if $check_only; then
    [[ -f $runtime/configs/config.yaml ]] || {
        echo "The prepared SIH config is missing: $runtime/configs/config.yaml" >&2
        exit 1
    }
    [[ -r $stack_script ]] || {
        echo "The SIH stack script is not readable: $stack_script" >&2
        exit 1
    }
    docker image inspect "$px4_image" "$image" >/dev/null || {
        echo "One or more pinned SIH images are missing locally." >&2
        exit 1
    }
    echo "SIH preflight passed. No processes, containers, settings, or demo files were changed."
    echo "This preflight does not verify backend startup, video, PX4 delivery, or QGC behavior."
    exit 0
fi
umask 077
mkdir -p "$demo/config/QGroundControl" "$demo/cache" "$demo/data" "$demo/home"
chmod 700 "$demo"
dashboard_pid_file=$demo/dashboard.pid
app_pid_file=$demo/app.pid
exec 9> "$demo/launcher.lock"
if ! flock -n 9; then
    echo "The SIH demo is already running. Close its QGC window before starting another run." >&2
    exit 1
fi

if [[ $video == target-path ]]; then
    fixture_python=$cache/slice-3b-2026-09-22/runtime/full-ai-venv/bin/python
    [[ -x $fixture_python ]] || { echo "The prepared Full AI Python environment is missing." >&2; exit 1; }
    "$fixture_python" "$repo/custom-pixeagle/validation/generate-target-path.py" \
        --output "$runtime/target-path-frames"
    "$fixture_python" - "$prepared_runtime" "$runtime" <<'PYPREPARE'
import json
import shutil
import sys
from pathlib import Path

import yaml

source, runtime = (Path(value) for value in sys.argv[1:])
for name in ('src', 'resources', 'models'):
    destination = runtime / name
    if destination.is_symlink() and destination.resolve() == (source / name).resolve():
        continue
    if destination.exists() or destination.is_symlink():
        raise SystemExit(f'The target-path runtime has an unexpected {name} entry.')
    destination.symlink_to(source / name, target_is_directory=True)
if not (runtime / 'configs').exists():
    shutil.copytree(source / 'configs', runtime / 'configs', ignore=shutil.ignore_patterns('secrets'))
shutil.copyfile(source / 'credentials.json', runtime / 'credentials.json')
(runtime / 'credentials.json').chmod(0o600)
(runtime / 'logs').mkdir(exist_ok=True)
config_path = runtime / 'configs/config.yaml'
config = yaml.safe_load(config_path.read_text())
truth_path = runtime / 'target-path-frames/ground-truth.json'
truth = json.loads(truth_path.read_text())
video = next(value for value in config.values() if isinstance(value, dict) and 'CUSTOM_PIPELINE' in value)
video.update({
    'VIDEO_SOURCE_TYPE': 'CUSTOM_GSTREAMER', 'USE_GSTREAMER': True,
    'PIPELINE_MODE': 'REALTIME', 'CAPTURE_FPS': truth['fps'], 'DEFAULT_FPS': truth['fps'],
    'CAPTURE_WIDTH': truth['width'], 'CAPTURE_HEIGHT': truth['height'],
    'FRAME_ROTATION_DEG': 0, 'FRAME_FLIP_MODE': 'none',
    'CUSTOM_PIPELINE': (
        f'multifilesrc location="{runtime}/target-path-frames/frame-%05d.png" '
        f'start-index=0 stop-index={truth["frame_count"] - 1} loop=true '
        f'caps=image/png,framerate={truth["fps"]}/1 ! pngdec ! videorate ! '
        'videoconvert ! video/x-raw,format=BGR ! appsink max-buffers=1 drop=true sync=true'
    ),
})
config_path.write_text(yaml.safe_dump(config, sort_keys=False))
(runtime / 'fixture-runtime.json').write_text(json.dumps({
    'fixture_id': truth['fixture_id'], 'prepared_source_runtime': str(source),
    'ground_truth': str(truth_path), 'simulator_only': True,
    'source_clock_paced': True, 'drop_old_frames': True,
    'limitations': truth['limitations'],
}, indent=2) + '\n')
print('Target path: 30-second loop; center, right, left, up, down, center.')
print(f'Ground truth: {truth_path}')
PYPREPARE
fi

if [[ -f $dashboard_pid_file ]]; then
    stale_dashboard_pid=$(cat "$dashboard_pid_file")
    if [[ $stale_dashboard_pid =~ ^[0-9]+$ && -r /proc/$stale_dashboard_pid/cmdline ]]; then
        stale_command=$(tr '\0' ' ' < "/proc/$stale_dashboard_pid/cmdline")
        if [[ $stale_command == *"$dashboard/node_modules/.bin/serve -s $dashboard_build"* ]]; then
            kill -TERM -- "-$stale_dashboard_pid" 2>/dev/null || true
            for ((attempt=0;attempt<50;attempt++)); do
                if ! kill -0 "$stale_dashboard_pid" 2>/dev/null; then break; fi
                sleep 0.1
            done
        fi
    fi
    rm -f "$dashboard_pid_file"
fi
if [[ -f $app_pid_file ]]; then
    mapfile -t stale_app < "$app_pid_file"
    if [[ ${stale_app[0]:-} =~ ^[0-9]+$ && -r /proc/${stale_app[0]}/cmdline && -n ${stale_app[1]:-} ]]; then
        stale_command=$(tr '\0' ' ' < "/proc/${stale_app[0]}/cmdline")
        if [[ $stale_command == "${stale_app[1]} "* ]]; then
            kill -TERM "${stale_app[0]}" 2>/dev/null || true
        fi
    fi
    rm -f "$app_pid_file"
fi

for container in "$stack_name" "$px4_name"; do
    if docker container inspect "$container" >/dev/null 2>&1; then
        label=$(docker container inspect "$container" --format '{{index .Config.Labels "pixeagle.qgc-demo"}}')
        if [[ $label != "$owner_label" ]]; then
            echo "Container $container exists but was not created by this launcher; stop it manually." >&2
            exit 1
        fi
        docker rm --force "$container" >/dev/null
    fi
done
if docker network inspect "$network_name" >/dev/null 2>&1; then
    label=$(docker network inspect "$network_name" --format '{{index .Labels "pixeagle.qgc-demo"}}')
    if [[ $label != "$owner_label" ]]; then
        echo "Network $network_name exists but was not created by this launcher; remove it manually." >&2
        exit 1
    fi
    docker network rm "$network_name" >/dev/null
fi

archive=$demo/history/$(date -u +%Y%m%dT%H%M%SZ)-$$
for previous_log in \
    "$demo/qgc.log" "$demo/dashboard.log" \
    "$runtime/logs/pixeagle.log" "$runtime/logs/router.log" \
    "$runtime/logs/mavlink2rest.log" "$runtime/logs/mavsdk.log" \
    "$runtime/logs/security-audit.jsonl"; do
    if [[ -f $previous_log ]]; then
        mkdir -p "$archive"
        mv "$previous_log" "$archive/"
    fi
done

python3 - <<'PY'
import errno
import socket

for kind, ports in ((socket.SOCK_STREAM, (8094, 3040)),
                    (socket.SOCK_DGRAM, (14560,))):
    for port in ports:
        with socket.socket(socket.AF_INET, kind) as probe:
            try:
                probe.bind(('127.0.0.1', port))
            except OSError as exc:
                if exc.errno == errno.EADDRINUSE:
                    raise SystemExit(
                        f'Cannot start the SIH demo: localhost port {port} is already in use. '
                        'Close the other service or QGC session and try again.'
                    ) from None
                raise
PY

# Refresh the stopped demo's source and definitions together, preserving its data.
python3 "$backend/tools/refresh_native_demo_sources.py" "$prepared_runtime"
if [[ $runtime != "$prepared_runtime" ]]; then
    for definition in config_default.yaml config_schema.yaml config_retirements.yaml tracker_schemas.yaml follower_commands.yaml; do
        cp -- "$prepared_runtime/configs/$definition" "$runtime/configs/$definition"
    done
fi

# Image sequences need timestamps as well as a clock-synchronized sink.
python3 - "$runtime" <<'PYREPLAY'
import sys
from pathlib import Path

import yaml

config_path = Path(sys.argv[1]) / 'configs/config.yaml'
config = yaml.safe_load(config_path.read_text())
section = next(value for value in config.values()
               if isinstance(value, dict) and 'CUSTOM_PIPELINE' in value)
pipeline = section['CUSTOM_PIPELINE']
if not pipeline.startswith('multifilesrc ') or 'appsink' not in pipeline:
    raise SystemExit('The SIH fixture requires its prepared image-sequence source.')
if 'videorate' not in pipeline:
    pipeline = pipeline.replace('! jpegdec !', '! jpegdec ! videorate !')
    pipeline = pipeline.replace('! pngdec !', '! pngdec ! videorate !')
section['CUSTOM_PIPELINE'] = pipeline.replace('sync=false', 'sync=true')
security = next(value for value in config.values()
                if isinstance(value, dict) and 'API_SECURITY_AUDIT_LOG_PATH' in value)
security['API_SECURITY_AUDIT_LOG_PATH'] = str(Path(sys.argv[1]) / 'logs/security-audit.jsonl')
config_path.write_text(yaml.safe_dump(config, sort_keys=False))
PYREPLAY

px4_started=false
stack_started=false
network_started=false
app_pid=
dashboard_pid=
cleanup() {
    trap - EXIT
    trap '' INT TERM
    if [[ -n $app_pid ]]; then
        kill "$app_pid" 2>/dev/null || true
        for ((attempt=0;attempt<50;attempt++)); do
            if ! kill -0 "$app_pid" 2>/dev/null; then break; fi
            sleep 0.1
        done
        if kill -0 "$app_pid" 2>/dev/null; then kill -KILL "$app_pid" 2>/dev/null || true; fi
        wait "$app_pid" 2>/dev/null || true
    fi
    rm -f "$app_pid_file"
    if [[ -n $dashboard_pid ]]; then
        kill -- "-$dashboard_pid" 2>/dev/null || true
        wait "$dashboard_pid" 2>/dev/null || true
    fi
    rm -f "$dashboard_pid_file"
    if $stack_started; then docker stop --timeout 5 "$stack_name" >/dev/null 2>&1 || true; fi
    if $px4_started; then docker stop --timeout 5 "$px4_name" >/dev/null 2>&1 || true; fi
    if $network_started; then docker network rm "$network_name" >/dev/null 2>&1 || true; fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

docker network create --driver bridge --label "pixeagle.qgc-demo=$owner_label" \
    "$network_name" > "$demo/network-id.txt"
network_started=true
gateway=$(docker network inspect "$network_name" --format '{{(index .IPAM.Config 0).Gateway}}')
[[ -n $gateway ]] || { echo "Docker bridge gateway is unavailable." >&2; exit 1; }
docker run -d --rm --name "$px4_name" --label "pixeagle.qgc-demo=$owner_label" \
    --network "$network_name" \
    -p 127.0.0.1:8094:8094/tcp \
    -e PX4_SIM_MODEL=sihsim_quadx "$px4_image" > "$demo/px4-container-id.txt"
px4_started=true
docker run -d --rm --name "$stack_name" --label "pixeagle.qgc-demo=$owner_label" \
    --user "$(id -u):$(id -g)" \
    --network "container:$px4_name" \
    -e LD_LIBRARY_PATH=/mnt/hostlibs \
    -e PIXEAGLE_SIH_RUNTIME="$runtime" \
    -e PIXEAGLE_QGC_UDP_HOST="$gateway" \
    -v "$cache:$cache" \
    -v /home/alireza/PixEagle/bin:/home/alireza/PixEagle/bin:ro \
    -v /home/alireza/PixEagle/.venv/lib:/home/alireza/PixEagle/.venv/lib:ro \
    -v /usr/lib/x86_64-linux-gnu:/mnt/hostlibs:ro \
    "$image" bash "$stack_script" > "$demo/stack-container-id.txt"
stack_started=true

python3 - "$runtime" <<'PY'
import json
import socket
import sys
import time
from pathlib import Path

import requests

runtime = Path(sys.argv[1])
credentials = json.loads((runtime / 'credentials.json').read_text())
deadline = time.monotonic() + 75
while time.monotonic() < deadline:
    try:
        with requests.Session() as client:
            result = client.post(credentials['endpoint'] + '/api/v1/auth/login', json={
                'username': credentials['username'], 'password': credentials['password'],
            }, timeout=3)
            if result.status_code == 200:
                context = client.get(credentials['endpoint'] + '/api/v1/integration/context', timeout=3).json()
                if context.get('instance_id') == 'pixeagle-sih-slice4-private':
                    if runtime.name == 'sih-test9-v1':
                        inventory = client.get(
                            credentials['endpoint'] + '/api/v1/integration/models',
                            timeout=8,
                        ).json()
                        models = {row.get('model_id'): row for row in inventory.get('models', [])}
                        missing = [model_id for model_id in ('visdrone26m', 'visdrone9m')
                                   if not models.get(model_id, {}).get('available')]
                        if missing:
                            raise SystemExit(
                                'The test9 Smart model catalog is incomplete: '
                                + ', '.join(missing)
                                + '. Inspect the runtime model catalog.'
                            )
                    print('Isolated PixEagle and PX4 SIH ready. Sign in from the private credentials file.')
                    break
    except (requests.RequestException, ValueError):
        pass
    time.sleep(0.5)
else:
    raise SystemExit('The isolated PixEagle backend did not become ready; inspect the runtime logs.')

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as telemetry:
    telemetry.bind(('0.0.0.0', 14560))
    telemetry.settimeout(12)
    try:
        packet, _ = telemetry.recvfrom(2048)
    except socket.timeout as exc:
        raise SystemExit('No isolated PX4 telemetry reached QGC UDP 14560.') from exc
    if not packet or packet[0] not in (0xfd, 0xfe):
        raise SystemExit('The QGC UDP input did not receive a MAVLink packet.')
    print('Isolated PX4 telemetry reached QGC UDP 14560.')
PY

docker exec -i "$stack_name" \
    /home/alireza/.cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/runtime/full-ai-venv/bin/python - "$runtime" <<'PY'
import json
import sys
import time
from pathlib import Path

import requests

router = 'http://127.0.0.1:8088'
message = requests.get(router + '/v1/helper/mavlink?name=COMMAND_LONG', timeout=5).json()
message['message'].update(
    param1=148.0, command={'type': 'MAV_CMD_REQUEST_MESSAGE'},
    target_system=1, target_component=1,
)
requests.post(router + '/v1/mavlink', json=message, timeout=5).raise_for_status()
runtime = Path(sys.argv[1])
credentials = json.loads((runtime / 'credentials.json').read_text())
with requests.Session() as client:
    client.post(credentials['endpoint'] + '/api/v1/auth/login', json={
        'username': credentials['username'], 'password': credentials['password'],
    }, timeout=5).raise_for_status()
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        context = client.get(credentials['endpoint'] + '/api/v1/integration/context', timeout=5).json()
        uid = context.get('telemetry', {}).get('autopilot_uid')
        if isinstance(uid, str) and uid.isdigit() and int(uid) > 0:
            (runtime / 'demo-aircraft.json').write_text(json.dumps({'uid': uid}))
            print('Resolved simulated PX4 aircraft UID for QGC settings.')
            break
        time.sleep(0.25)
    else:
        raise SystemExit('The simulated aircraft UID was not received.')
PY

setsid "$dashboard/node_modules/.bin/serve" -s "$dashboard_build" \
    -l tcp://127.0.0.1:3040 > "$demo/dashboard.log" 2>&1 9>&- &
dashboard_pid=$!
printf '%s\n' "$dashboard_pid" > "$dashboard_pid_file"
for ((attempt=0;attempt<50;attempt++)); do
    if curl --silent --fail --output /dev/null http://127.0.0.1:3040/; then break; fi
    sleep 0.1
done
curl --silent --fail --output /dev/null http://127.0.0.1:3040/ || {
    echo "The local dashboard did not become ready." >&2
    exit 1
}

# Probe the same host-to-container WebSocket route used by the desktop app.
"$cache/slice-3b-2026-09-22/runtime/full-ai-venv/bin/python" - "$runtime" <<'PYMEDIA'
import json
import sys
from pathlib import Path

import requests
from websockets.sync.client import connect

runtime = Path(sys.argv[1])
credentials = json.loads((runtime / 'credentials.json').read_text())
endpoint = credentials['endpoint']
with requests.Session() as client:
    client.post(endpoint + '/api/v1/auth/login', json={
        'username': credentials['username'], 'password': credentials['password'],
    }, timeout=5).raise_for_status()
    cookie = '; '.join(f'{entry.name}={entry.value}' for entry in client.cookies)
    with connect(endpoint.replace('http://', 'ws://') + '/ws/video_feed',
                 origin=endpoint, additional_headers={'Cookie': cookie},
                 open_timeout=5, proxy=None) as stream:
        for _ in range(10):
            packet = stream.recv(timeout=5)
            if isinstance(packet, bytes) and packet.startswith(b'\xff\xd8'):
                print('Authenticated QGC-route WebSocket JPEG delivery passed.')
                break
        else:
            raise SystemExit('The authenticated video stream delivered no JPEG frame.')
PYMEDIA

python3 - "$demo" "$runtime" <<'PY'
import configparser
import json
import sys
from pathlib import Path

demo = Path(sys.argv[1])
runtime = Path(sys.argv[2])
uid = json.loads((runtime / 'demo-aircraft.json').read_text())['uid']
settings = configparser.ConfigParser(interpolation=None)
settings.optionxform = str
settings['General'] = {'SettingsVersion': '9', 'audioMuted': 'true', 'firstRunPromptIdsShown': '3'}
settings['AutoConnect'] = {
    'autoConnectUDP': 'true', 'udpListenPort': '14560',
    'autoConnectPixhawk': 'false', 'autoConnectSiKRadio': 'false',
    'autoConnectRTKGPS': 'false', 'autoConnectLibrePilot': 'false',
    'autoConnectZeroConf': 'false', 'nmeaSource': '0',
}
settings['PixEagle'] = {
    'integrationEnabled': 'true', 'videoEnabled': 'true',
    'CompanionEndpoint': 'http://127.0.0.1:8094',
    f'Endpoints\\{uid}': 'http://127.0.0.1:8094',
}
settings['QGCQml'] = {'MainFlyWindowIsMap': 'false', 'IsPIPVisible': 'true'}
settings['MainWindowState'] = {'width': '1440', 'height': '900', 'visibility': '2'}
with (demo / 'config/QGroundControl/PixEagle-QGroundControl Daily.ini').open('w') as output:
    settings.write(output, space_around_delimiters=False)
PY

echo "Credentials: $runtime/credentials.json"
echo "QGC and backend logs: $demo and $runtime/logs"
echo "Close QGC or press Ctrl-C once to stop the SIH demo. Wait for the shell prompt before restarting."
desktop_libs=$cache/desktop-libs/root/usr/lib/x86_64-linux-gnu
if [[ -f $desktop_libs/libxcb-cursor.so.0 ]]; then
    export LD_LIBRARY_PATH="$desktop_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
XDG_CONFIG_HOME="$demo/config" XDG_CACHE_HOME="$demo/cache" \
    XDG_DATA_HOME="$demo/data" "$binary" > "$demo/qgc.log" 2>&1 9>&- &
app_pid=$!
printf '%s\n%s\n' "$app_pid" "$binary" > "$app_pid_file"
wait "$app_pid"
app_pid=
