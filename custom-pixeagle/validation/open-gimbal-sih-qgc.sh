#!/usr/bin/env bash
# Open the custom QGC build against a separately running, isolated gimbal SIH stack.
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
binary=${PIXEAGLE_QGC_BINARY:-"$repo/build/pixeagle-custom-release/Release/PixEagle-QGroundControl"}
check_only=false
if [[ $# == 2 && $1 == --check ]]; then
    check_only=true
    runtime=$(realpath -- "$2")
elif [[ $# == 1 ]]; then
    runtime=$(realpath -- "$1")
else
    echo "Usage: $0 [--check] PRIVATE_SIH_RUNTIME" >&2
    exit 2
fi
[[ -x $binary ]] || {
    echo "QGC binary is unavailable: $binary" >&2
    exit 1
}
[[ -f $runtime/profile-manifest.json && -f $runtime/logs/stack-probe-result.json ]] || {
    echo "SIH startup probe did not complete; inspect $runtime/logs and rerun the stack with --hold." >&2
    exit 1
}
if ! $check_only && [[ -z ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
    echo "Launch from the operator desktop session." >&2
    exit 1
fi

desktop=$runtime/qgc-desktop
"${PIXEAGLE_QGC_HOST_PYTHON:-python3}" - "$runtime" "$desktop" "$check_only" <<'PY'
import configparser
import json
import sys
from pathlib import Path

import requests

runtime, desktop = (Path(value) for value in sys.argv[1:3])
check_only = sys.argv[3] == "true"
manifest = json.loads((runtime / "profile-manifest.json").read_text(encoding="utf-8"))
probe = json.loads((runtime / "logs/stack-probe-result.json").read_text(encoding="utf-8"))
credentials = json.loads((runtime / "credentials.json").read_text(encoding="utf-8"))
if manifest.get("kind") != "gimbal-sih-preflight" or probe.get("instance_id") != manifest.get("instance_id"):
    raise SystemExit("The private SIH profile and startup probe do not match")
uid = probe.get("simulated_autopilot_uid")
if not isinstance(uid, str) or not uid.isdigit() or int(uid) <= 0:
    raise SystemExit("The simulated aircraft UID is unavailable")
endpoint = credentials["endpoint"]
with requests.Session() as client:
    login = client.post(endpoint + "/api/v1/auth/login", json={
        "username": credentials["username"], "password": credentials["password"],
    }, timeout=3)
    login.raise_for_status()
    context = client.get(endpoint + "/api/v1/integration/context", timeout=3).json()
    safety = client.get(endpoint + "/api/v1/integration/safety", timeout=3).json()
    if (context.get("instance_id") != manifest["instance_id"]
            or context.get("telemetry", {}).get("autopilot_uid") != uid
            or safety.get("active") is not True):
        raise SystemExit("The current backend identity, simulated aircraft or safety state changed")

if check_only:
    print("Read-only QGC handoff preflight passed; no settings or application changed.")
    raise SystemExit(0)

for directory in (desktop / "config/PixEagle", desktop / "cache", desktop / "data"):
    directory.mkdir(parents=True, exist_ok=True)
desktop.chmod(0o700)

settings = configparser.ConfigParser(interpolation=None)
settings.optionxform = str
settings["General"] = {"SettingsVersion": "9", "audioMuted": "true", "firstRunPromptIdsShown": "3"}
settings["AutoConnect"] = {
    "autoConnectUDP": "true", "udpListenPort": "14560",
    "autoConnectPixhawk": "false", "autoConnectSiKRadio": "false",
    "autoConnectRTKGPS": "false", "autoConnectLibrePilot": "false",
    "autoConnectZeroConf": "false", "nmeaSource": "0",
}
settings["PixEagle"] = {
    "integrationEnabled": "true", "videoEnabled": "true",
    "CompanionEndpoint": endpoint,
    f"Endpoints\\{uid}": endpoint,
}
settings["QGCQml"] = {"MainFlyWindowIsMap": "false", "IsPIPVisible": "true"}
settings["MainWindowState"] = {"width": "1440", "height": "900", "visibility": "2"}
path = desktop / "config/PixEagle/PixEagle-QGroundControl Daily.ini"
with path.open("w", encoding="utf-8") as output:
    settings.write(output, space_around_delimiters=False)
path.chmod(0o600)
print("Verified the live simulated aircraft and active flight-command block.")
PY
if $check_only; then exit 0; fi
sha256sum "$binary" > "$desktop/qgc-binary-sha256.txt"

cache=${QGC_BASELINE_CACHE:-"$HOME/.cache/pixeagle-qgc-baseline"}
desktop_libs=$cache/desktop-libs/root/usr/lib/x86_64-linux-gnu
if [[ -f $desktop_libs/libxcb-cursor.so.0 ]]; then
    export LD_LIBRARY_PATH="$desktop_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
echo "QGC log: $desktop/qgc.log"
echo "Sign in with the private SIH login at $runtime/credentials.json"
XDG_CONFIG_HOME="$desktop/config" XDG_CACHE_HOME="$desktop/cache" \
    XDG_DATA_HOME="$desktop/data" "$binary" > "$desktop/qgc.log" 2>&1
