#!/usr/bin/env python3
"""Release an idle, verified previous camera bench before a fresh handoff."""

import argparse
import json
import os
import re
import select
import signal
import subprocess
from pathlib import Path

import requests


def listening_pid(port):
    result = subprocess.run(
        ["ss", "-H", "-ltnp", "sport", "=", f":{port}"],
        capture_output=True,
        text=True,
        check=True,
    )
    owners = {int(match) for match in re.findall(r"pid=(\d+)", result.stdout)}
    if not result.stdout.strip():
        return None
    if len(owners) != 1:
        raise RuntimeError("The backend port has an unknown or shared owner; close it manually.")
    return owners.pop()


def camera_bench_is_idle(runtime, port):
    manifest = json.loads((runtime / "bench-manifest.json").read_text())
    credentials = json.loads((runtime / "credentials.json").read_text())
    if manifest["port"] != port or credentials["endpoint"] != f"http://127.0.0.1:{port}":
        raise RuntimeError("Previous bench identity or endpoint is inconsistent.")

    session = requests.Session()
    session.trust_env = False
    try:
        response = session.post(
            credentials["endpoint"] + "/api/v1/auth/login",
            json={key: credentials[key] for key in ("username", "password")},
            timeout=3,
        )
        response.raise_for_status()
        auth = response.json()

        def get(path):
            result = session.get(credentials["endpoint"] + path, timeout=3)
            result.raise_for_status()
            return result.json()

        context = get("/api/v1/integration/context")
        target = get("/api/v1/integration/target-state")
        camera = get("/api/v1/gimbal/control")
        manual = camera.get("manual", {})
        manual_idle = (
            manual.get("state") == "idle" and manual.get("gesture_id") is None
        ) or manual.get("state") in {"stopped", "expired"}
        idle = (
            context.get("instance_id") == manifest["instance_id"]
            and context.get("command", {}).get("connected") is False
            and context.get("telemetry", {}).get("connected") is False
            and target.get("tracking_active") is False
            and target.get("following_active") is False
            and camera.get("motion_active") is False
            and camera.get("following_active") is False
            and camera.get("tracking_state") == "disabled"
            and manual_idle
        )
        session.post(
            credentials["endpoint"] + "/api/v1/auth/logout",
            json={},
            headers={auth["csrf_header_name"]: auth["csrf_token"]},
            timeout=3,
        )
        return idle
    finally:
        session.close()


def retire_previous(port, previous_runtimes):
    pid = listening_pid(port)
    if pid is None:
        return

    owner = Path(os.readlink(f"/proc/{pid}/cwd")).resolve()
    allowed = {path.resolve() for path in previous_runtimes}
    command = (Path(f"/proc/{pid}/cmdline").read_bytes()).split(b"\0")
    if owner not in allowed or b"src/main.py" not in command:
        raise RuntimeError(
            "A different backend owns the port; it will not be stopped automatically."
        )

    with os.fdopen(os.pidfd_open(pid), "rb", closefd=True) as process:
        if not camera_bench_is_idle(owner, port):
            raise RuntimeError(
                "The previous camera bench is active; stop tracking and close it manually."
            )
        if Path(os.readlink(f"/proc/{pid}/cwd")).resolve() != owner:
            raise RuntimeError("Previous backend ownership changed during the check.")
        signal.pidfd_send_signal(process.fileno(), signal.SIGTERM)
        poller = select.poll()
        poller.register(process.fileno(), select.POLLIN)
        if not poller.poll(5000):
            raise RuntimeError("Previous backend did not stop; it will not be force-killed.")
    print(f"Retired idle camera bench {owner.name}; starting the fresh bench.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--previous-runtime", type=Path, action="append", required=True)
    args = parser.parse_args()
    try:
        retire_previous(args.port, args.previous_runtime)
    except (OSError, ValueError, KeyError, requests.RequestException, RuntimeError) as exc:
        parser.exit(1, f"Cannot retire previous camera bench: {exc}\n")


if __name__ == "__main__":
    main()
