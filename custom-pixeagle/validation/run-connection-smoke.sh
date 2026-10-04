#!/usr/bin/env bash
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cache=${QGC_BASELINE_CACHE:-"$HOME/.cache/pixeagle-qgc-baseline"}
backend=${PIXEAGLE_WORKTREE:-"$(dirname -- "$repo")/PixEagle-qgc-integration"}
evidence=${PIXEAGLE_SMOKE_EVIDENCE:-"$cache/slice-1-2026-09-21"}
container_image=${QGC_BASELINE_IMAGE:-sha256:ecfaf2358be090447ff9948397075ce76cefff3a152fbfab006a2d26495ea14b}
container_name=${PIXEAGLE_SMOKE_CONTAINER:-pixeagle-slice1-ui}

if [[ ${1:-} != --inside ]]; then
    mkdir -p "$evidence"
    exec docker run -d --rm --name "$container_name" \
        --user "$(id -u):$(id -g)" --shm-size 2g --network none \
        --mount "type=bind,src=$repo,dst=$repo" \
        --mount "type=bind,src=$backend,dst=$backend" \
        --mount "type=bind,src=$cache,dst=$cache" \
        -e QGC_BASELINE_CACHE="$cache" -e PIXEAGLE_WORKTREE="$backend" \
        -e PIXEAGLE_SMOKE_EVIDENCE="$evidence" \
        -e PIXEAGLE_SMOKE_NO_AIRCRAFT="${PIXEAGLE_SMOKE_NO_AIRCRAFT:-0}" \
        -e PIXEAGLE_SMOKE_REPLAY_DIR="${PIXEAGLE_SMOKE_REPLAY_DIR:-}" \
        -e PIXEAGLE_SMOKE_BINARY="${PIXEAGLE_SMOKE_BINARY:-$repo/build/pixeagle-custom-debug/Debug/PixEagle-QGroundControl}" \
        -w "$repo" "$container_image" bash "$0" --inside
fi

ui=$evidence/ui
export HOME=$ui/home
export XDG_CONFIG_HOME=$ui/config
export XDG_CACHE_HOME=$ui/cache
export XDG_DATA_HOME=$ui/data
export XDG_RUNTIME_DIR=$ui/runtime
export DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1
export PATH=$repo/tools/.venv/bin:$cache/ui-tools/root/usr/bin:/usr/bin:/bin
export LD_LIBRARY_PATH=$cache/ui-tools/root/usr/lib/x86_64-linux-gnu
export LANG=C.UTF-8
mkdir -p "$HOME" "$XDG_CONFIG_HOME/QGroundControl" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

settings="$XDG_CONFIG_HOME/QGroundControl/PixEagle-QGroundControl Daily.ini"
if [[ ! -e $settings ]]; then
    cat > "$settings" <<'INI'
[General]
SettingsVersion=9
audioMuted=true

[MainWindowState]
x=0
y=0
width=1440
height=900
visibility=2
INI
fi

children=()
cleanup() {
    if ((${#children[@]})); then
        kill "${children[@]}" 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap 'exit 0' TERM INT

Xvfb :99 -screen 0 1440x900x24 -ac > "$ui/xvfb.log" 2>&1 &
children+=("$!")
for ((attempt = 0; attempt < 50; attempt++)); do
    if xdotool getdisplaygeometry >/dev/null 2>&1; then break; fi
    sleep 0.1
done

if [[ -n ${PIXEAGLE_SMOKE_REPLAY_DIR:-} ]]; then
    (
        cd "$backend"
        exec .venv/bin/python tools/native_replay_demo.py run --directory "$PIXEAGLE_SMOKE_REPLAY_DIR"
    ) > "$ui/real-pixeagle.log" 2>&1 &
    children+=("$!")
elif [[ ${PIXEAGLE_SMOKE_NO_AIRCRAFT:-0} == 1 ]]; then
    (
        cd "$backend"
        exec .venv/bin/python tools/native_integration_fixture.py \
            --port 8093 --instance-id fixture-no-aircraft --no-aircraft \
            --media-control-file "$ui/media-3.json"
    ) > "$ui/companion-3.log" 2>&1 &
    children+=("$!")
else
for id in 1 2; do
    (
        cd "$backend"
        exec .venv/bin/python tools/native_integration_fixture.py \
            --port "809$id" --system-id "$id" --uid "1844674407370955100$id" \
            --instance-id "fixture-$id" --media-control-file "$ui/media-$id.json"
    ) > "$ui/companion-$id.log" 2>&1 &
    children+=("$!")
    python "$repo/custom-pixeagle/validation/mock_vehicle.py" \
        --system-id "$id" --uid "1844674407370955100$id" > "$ui/mock-$id.log" 2>&1 &
    children+=("$!")
done
fi

"$PIXEAGLE_SMOKE_BINARY" > "$ui/app.log" 2>&1 &
app_pid=$!
children+=("$app_pid")
echo "$app_pid" > "$ui/app.pid"
wait "$app_pid"
