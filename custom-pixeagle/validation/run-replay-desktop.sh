#!/usr/bin/env bash
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cache=${QGC_BASELINE_CACHE:-"$HOME/.cache/pixeagle-qgc-baseline"}
backend=${PIXEAGLE_WORKTREE:-"$(dirname -- "$repo")/PixEagle-qgc-integration"}
replay=${1:?Usage: run-replay-desktop.sh /absolute/private/replay-directory}
demo=${PIXEAGLE_DESKTOP_DEMO_DIR:-"$cache/local-replay-desktop"}
binary=${PIXEAGLE_SMOKE_BINARY:-"$repo/build/pixeagle-custom-release/Release/PixEagle-QGroundControl"}

[[ -x $binary ]] || { echo "Build the custom Linux Release app first." >&2; exit 1; }
[[ -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]] || { echo "Run this command from your desktop session." >&2; exit 1; }
umask 077
mkdir -p "$demo/config/QGroundControl" "$demo/cache" "$demo/data" "$demo/home"
chmod 700 "$demo"

"$backend/.venv/bin/python" "$backend/tools/native_replay_demo.py" validate --directory "$replay"
"$backend/.venv/bin/python" - "$replay" "$demo" <<'PY'
import configparser
import json
from pathlib import Path
import socket
import sys

replay, demo = map(Path, sys.argv[1:])
manifest = json.loads((replay / 'demo-manifest.json').read_text())
with socket.socket() as probe:
    probe.bind(('127.0.0.1', manifest['port']))
settings = configparser.ConfigParser(interpolation=None)
settings.optionxform = str
path = demo / 'config/QGroundControl/PixEagle-QGroundControl Daily.ini'
settings.read(path)
settings['General'] = {'SettingsVersion': '9', 'audioMuted': 'true', 'firstRunPromptIdsShown': '3'}
settings['AutoConnect'] = {key: 'false' for key in (
    'autoConnectUDP', 'autoConnectPixhawk', 'autoConnectSiKRadio',
    'autoConnectRTKGPS', 'autoConnectLibrePilot', 'autoConnectZeroConf')}
settings['AutoConnect']['nmeaSource'] = '0'
settings['PixEagle'] = {'integrationEnabled': 'false', 'videoEnabled': 'true',
                       'CompanionEndpoint': f"http://127.0.0.1:{manifest['port']}"}
settings['QGCQml'] = {'MainFlyWindowIsMap': 'false', 'IsPIPVisible': 'true'}
settings['MainWindowState'] = {'width': '1280', 'height': '800', 'visibility': '2'}
with path.open('w') as output:
    settings.write(output, space_around_delimiters=False)
PY

backend_pid=
app_pid=
cleanup() {
    [[ -z $app_pid ]] || kill "$app_pid" 2>/dev/null || true
    [[ -z $backend_pid ]] || kill "$backend_pid" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 0' INT TERM

"$backend/.venv/bin/python" "$backend/tools/native_replay_demo.py" run --directory "$replay" \
    > "$demo/pixeagle.log" 2>&1 &
backend_pid=$!
echo "$backend_pid" > "$demo/backend.pid"

"$backend/.venv/bin/python" - "$replay" <<'PY'
import json
from pathlib import Path
import sys
import time

import httpx

directory = Path(sys.argv[1])
credentials = json.loads((directory / 'credentials.json').read_text())
manifest = json.loads((directory / 'demo-manifest.json').read_text())
deadline = time.monotonic() + 30
while time.monotonic() < deadline:
    try:
        with httpx.Client(base_url=credentials['endpoint'], timeout=2, trust_env=False) as client:
            response = client.post('/api/v1/auth/login', json={
                'username': credentials['username'], 'password': credentials['password']})
            if response.status_code == 200:
                session = response.json()
                context = client.get('/api/v1/integration/context').json()
                valid = (context.get('instance_id') == manifest['instance_id']
                         and context.get('command', {}).get('connected') is False
                         and context.get('telemetry', {}).get('connected') is False)
                client.post('/api/v1/auth/logout', json={}, headers={
                    session['csrf_header_name']: session['csrf_token']})
                if valid:
                    print('Recorded-video companion ready; no aircraft connected.')
                    break
                raise SystemExit('Unexpected companion identity or aircraft connection; demo stopped.')
    except (httpx.HTTPError, ValueError):
        pass
    time.sleep(0.5)
else:
    raise SystemExit('PixEagle did not become ready; inspect the private demo pixeagle.log.')
PY

echo "Demo login: $replay/credentials.json"
echo "Logs and isolated QGC settings: $demo"
desktop_libs=$cache/desktop-libs/root/usr/lib/x86_64-linux-gnu
if [[ -f $desktop_libs/libxcb-cursor.so.0 ]]; then
    export LD_LIBRARY_PATH="$desktop_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
HOME="$demo/home" XDG_CONFIG_HOME="$demo/config" XDG_CACHE_HOME="$demo/cache" XDG_DATA_HOME="$demo/data" \
    "$binary" > "$demo/qgc.log" 2>&1 &
app_pid=$!
echo "$app_pid" > "$demo/app.pid"
wait "$app_pid"
