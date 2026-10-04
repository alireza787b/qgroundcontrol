#!/usr/bin/env bash
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cache=${QGC_BASELINE_CACHE:-"$HOME/.cache/pixeagle-qgc-baseline"}
container_image=${QGC_BASELINE_IMAGE:-sha256:ecfaf2358be090447ff9948397075ce76cefff3a152fbfab006a2d26495ea14b}
uv_binary=$(command -v uv)

exec docker run --rm --user "$(id -u):$(id -g)" --shm-size 2g \
    --network "${QGC_BASELINE_NETWORK:-bridge}" \
    --mount "type=bind,src=$repo,dst=$repo" \
    --mount "type=bind,src=$cache,dst=$cache" \
    --mount "type=bind,src=$uv_binary,dst=/usr/local/bin/uv,readonly" \
    -e PATH="$repo/tools/.venv/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    -e QT_ROOT_DIR="$cache/Qt/6.11.1/gcc_64" \
    -e CCACHE_DIR="$repo/build/upstream-source/.ccache" \
    -e CPM_SOURCE_CACHE="$repo/.cache/CPM" \
    -e XDG_CACHE_HOME="$cache/container-cache" \
    -e XDG_CONFIG_HOME="$cache/container-config" \
    -e XDG_DATA_HOME="$cache/container-data" \
    -e QT_QPA_PLATFORM=offscreen \
    -e LIBGL_ALWAYS_SOFTWARE=1 \
    -w "$repo" "$container_image" "$@"
