#!/usr/bin/env python3
"""Qualify production gimbal guidance in an isolated, camera-free PX4 SIH namespace.

This checks synthetic observation -> continuity -> shaping -> publisher -> PX4
response. It does not qualify native camera selection, physical mounts, or video
closed-loop convergence. No host port is exposed and the PX4 namespace has no
network interface other than loopback.
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import itertools
import json
import math
import os
import shlex
import shutil
import subprocess
import threading
import time
import uuid
from collections import deque
from datetime import datetime
from pathlib import Path
from types import SimpleNamespace

PX4_IMAGE = "px4io/px4-sitl@sha256:fd6d93dc2705482aeb64ea26fdf16185d8a511010fdc53e26305f10d91855865"
WORKER_IMAGE = "sha256:ecfaf2358be090447ff9948397075ce76cefff3a152fbfab006a2d26495ea14b"
LABEL = "org.pixeagle.qgc.gimbal_evidence"


def command(*args: str, timeout: float = 30) -> str:
    return subprocess.check_output(args, text=True, timeout=timeout).strip()


def write_json(path: Path, data: object) -> None:
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def host(args: argparse.Namespace, *, driver_path: Path | None = None, configure=None) -> None:
    if not args.execute:
        raise SystemExit("Use --execute only for this isolated simulated-aircraft qualification.")
    repo = args.backend_repo.resolve()
    runtime = args.output.resolve()
    if runtime.exists():
        raise SystemExit("Use a new output directory; existing evidence is never overwritten.")
    for path in (
        repo / "src/classes/app_controller.py",
        args.python,
        args.bin_dir / "mavsdk_server_bin",
        args.bin_dir / "mavlink2rest",
    ):
        if not path.is_file():
            raise SystemExit(f"Required dependency is missing: {path}")
    runtime.mkdir(parents=True, mode=0o700)
    shutil.copytree(
        repo / "src", runtime / "backend/src", ignore=shutil.ignore_patterns("__pycache__")
    )
    (runtime / "backend/tools").mkdir()
    shutil.copyfile(
        repo / "tools/sitl_gimbal_geometry_fixture.py",
        runtime / "backend/tools/sitl_gimbal_geometry_fixture.py",
    )
    shutil.copytree(
        repo / "configs",
        runtime / "configs",
        ignore=shutil.ignore_patterns("secrets", "config.yaml"),
    )
    import yaml

    config = yaml.safe_load((runtime / "configs/config_default.yaml").read_text(encoding="utf-8"))
    config["PX4"].update(
        EXTERNAL_MAVSDK_SERVER=True,
        MAVSDK_SERVER_ADDRESS="127.0.0.1",
        MAVSDK_SERVER_PORT=50051,
        SYSTEM_ADDRESS="udpin://127.0.0.1:14540",
    )
    config["Follower"]["USE_MAVLINK2REST"] = True
    config["MAVLink"].update(MAVLINK_ENABLED=True, MAVLINK_HOST="127.0.0.1", MAVLINK_PORT=8088)
    config["Follower"]["FOLLOWER_MODE"] = args.follower
    config["Follower"]["FOLLOWER_EXECUTION_MODE"] = "PX4"
    config["Follower"]["FollowerOverrides"]["GM_VELOCITY_VECTOR"]["LATERAL_GUIDANCE_MODE"] = (
        "coordinated_turn"
    )
    config["GimbalTracker"].update(
        MOUNT_TYPE=args.mount, ENABLED=False, CONTROL_ENABLED=False, UDP_HOST="127.0.0.1"
    )
    config["FOLLOWER_CIRCUIT_BREAKER"] = True
    if configure is not None:
        configure(config)
    (runtime / "configs/config.yaml").write_text(
        yaml.safe_dump(config, sort_keys=False), encoding="utf-8"
    )
    (runtime / "backend/configs").mkdir()
    names = [f"pixeagle-evidence-{uuid.uuid4().hex[:10]}-{suffix}" for suffix in ("px4", "worker")]
    token = uuid.uuid4().hex
    manifest = {
        "kind": getattr(args, "evidence_kind", "camera-free-production-dispatch-sih"),
        "follower": args.follower,
        "mount": args.mount,
        "network": "none",
        "host_ports": [],
        "physical_camera": False,
        "native_camera_selection_qualified": False,
        "source_head": command("git", "-C", str(repo), "rev-parse", "HEAD"),
        "source_checksums": {
            str(path.relative_to(runtime / "backend")): hashlib.sha256(
                path.read_bytes()
            ).hexdigest()
            for path in sorted((runtime / "backend/src").rglob("*.py"))
        },
        "fixture_sha256": hashlib.sha256(
            (runtime / "backend/tools/sitl_gimbal_geometry_fixture.py").read_bytes()
        ).hexdigest(),
        "config_sha256": hashlib.sha256((runtime / "configs/config.yaml").read_bytes()).hexdigest(),
        "driver_sha256": hashlib.sha256((driver_path or Path(__file__)).read_bytes()).hexdigest(),
        "simulator_model": getattr(args, "simulator_model", "sihsim_quadx"),
        "binary_checksums": {
            name: hashlib.sha256((args.bin_dir / name).read_bytes()).hexdigest()
            for name in ("mavsdk_server_bin", "mavlink2rest")
        },
        "python_packages": json.loads(
            command(
                str(args.python),
                "-c",
                "import importlib.metadata,json; print(json.dumps(sorted((d.metadata['Name'], d.version) for d in importlib.metadata.distributions())))",
            )
        ),
        "px4_image": json.loads(command("docker", "image", "inspect", PX4_IMAGE))[0]["Id"],
        "worker_image": json.loads(command("docker", "image", "inspect", WORKER_IMAGE))[0]["Id"],
    }
    write_json(runtime / "manifest.json", manifest)
    worker_script = runtime / "driver.py"
    shutil.copyfile((driver_path or Path(__file__)).resolve(), worker_script)
    if driver_path is not None:
        shutil.copyfile(Path(__file__).resolve(), runtime / "sih_evidence_runtime.py")
    entry = runtime / "entry.sh"
    worker_extra = shlex.join(getattr(args, "worker_extra_args", []))
    entry.write_text(
        "#!/usr/bin/env bash\nset -euo pipefail\n"
        '"$EVIDENCE_BIN/mavlink2rest" -c udpin:127.0.0.1:14550 -s 127.0.0.1:8088 > /work/evidence/mavlink2rest.log 2>&1 &\n'
        'rest_pid=$!\n"$EVIDENCE_BIN/mavsdk_server_bin" -p 50051 udpin://127.0.0.1:14540 > /work/evidence/mavsdk.log 2>&1 &\n'
        'sdk_pid=$!\ntrap \'kill "$rest_pid" "$sdk_pid" 2>/dev/null || true\' EXIT\n'
        '"$EVIDENCE_PYTHON" /work/driver.py --worker --follower "$EVIDENCE_FOLLOWER" --mount "$EVIDENCE_MOUNT"'
        + (f' {worker_extra}' if worker_extra else '') + '\n',
        encoding="utf-8",
    )
    try:
        command(
            "docker",
            "run",
            "-d",
            "-i",
            "--name",
            names[0],
            "--label",
            f"{LABEL}={token}",
            "--network",
            "none",
            "-e",
            f"PX4_SIM_MODEL={getattr(args, 'simulator_model', 'sihsim_quadx')}",
            PX4_IMAGE,
        )
        python_root = args.python.absolute().parent.parent
        command(
            "docker",
            "run",
            "-d",
            "--name",
            names[1],
            "--label",
            f"{LABEL}={token}",
            "--network",
            f"container:{names[0]}",
            "--user",
            f"{os.getuid()}:{os.getgid()}",
            "-e",
            "LD_LIBRARY_PATH=/mnt/hostlibs",
            "-e",
            "PIXEAGLE_PROJECT_ROOT=/work/evidence",
            "-e",
            "PYTHONPATH=/work/backend/src:/work/backend/tools",
            "-e",
            f"EVIDENCE_OWNER={token}",
            "-e",
            f"EVIDENCE_PYTHON={args.python}",
            "-e",
            f"EVIDENCE_BIN={args.bin_dir}",
            "-e",
            f"EVIDENCE_FOLLOWER={args.follower}",
            "-e",
            f"EVIDENCE_MOUNT={args.mount}",
            "-v",
            f"{runtime / 'backend'}:/work/backend:ro",
            "-v",
            f"{runtime / 'configs'}:/work/backend/configs",
            "-v",
            f"{runtime}:/work/evidence",
            "-v",
            f"{worker_script}:/work/driver.py:ro",
            "-v",
            f"{python_root}:{python_root}:ro",
            "-v",
            f"{args.bin_dir}:{args.bin_dir}:ro",
            "-v",
            "/usr/lib/x86_64-linux-gnu:/mnt/hostlibs:ro",
            "-v",
            f"{args.opencv_lib}:{args.opencv_lib}:ro",
            "-w",
            "/work/evidence",
            WORKER_IMAGE,
            "bash",
            "/work/evidence/entry.sh",
        )
        deadline = time.monotonic() + 240
        while time.monotonic() < deadline:
            state = json.loads(command("docker", "inspect", names[1]))[0]["State"]
            if not state["Running"]:
                with (runtime / "worker.log").open("w") as log:
                    subprocess.run(
                        ["docker", "logs", names[1]],
                        stdout=log,
                        stderr=subprocess.STDOUT,
                        check=True,
                    )
                if state["ExitCode"] != 0:
                    raise RuntimeError(f"SIH worker failed; inspect {runtime / 'worker.log'}")
                print(f"SIH evidence passed: {runtime / 'result.json'}")
                return
            time.sleep(1)
        raise RuntimeError("SIH qualification exceeded its 240-second deadline")
    finally:
        for name in reversed(names):
            probe = subprocess.run(
                ["docker", "inspect", name], capture_output=True, text=True, check=False
            )
            if probe.returncode != 0:
                continue
            info = json.loads(probe.stdout)[0]
            if info["Config"]["Labels"].get(LABEL) != token:
                raise RuntimeError(f"Refusing cleanup: container ownership changed for {name}")
            subprocess.run(
                ["docker", "logs", name],
                stdout=(runtime / f"{name}.log").open("w"),
                stderr=subprocess.STDOUT,
                check=False,
            )
            subprocess.run(["docker", "rm", "-f", name], stdout=subprocess.DEVNULL, check=True)


async def worker(args: argparse.Namespace) -> None:
    if not os.environ.get("EVIDENCE_OWNER") or set(os.listdir("/sys/class/net")) != {"lo"}:
        raise RuntimeError(
            "Worker requires the owned Docker namespace with only loopback networking"
        )
    import logging

    import requests
    from classes.app_controller import AppController
    from classes.follower import Follower
    from classes.mavlink_data_manager import MavlinkDataManager
    from classes.offboard_commander import OffboardCommander
    from classes.parameters import Parameters
    from classes.px4_interface_manager import PX4InterfaceManager
    from classes.target_continuity import ContinuityPolicy, TargetContinuitySupervisor
    from classes.tracker_output import TrackerDataType, TrackerOutput
    from classes.tracker_trace import TrackerTraceRecorder
    from sitl_gimbal_geometry_fixture import camera_angles_for_world_target, pose_from_mavlink2rest

    logging.basicConfig(level=logging.WARNING)
    output = Path("/work/evidence")
    app = AppController.__new__(AppController)
    app.mavlink_data_manager = MavlinkDataManager(
        Parameters.MAVLINK_HOST,
        Parameters.MAVLINK_PORT,
        Parameters.MAVLINK_POLLING_INTERVAL,
        Parameters.MAVLINK_DATA_POINTS,
        enabled=True,
    )
    manager = PX4InterfaceManager(app)
    app.px4_interface = manager
    app._active_following_controller = manager
    app._follower_state_lock = asyncio.Lock()
    app._tracker_model_state_lock = threading.RLock()
    app._tracking_session_generation = 0
    app.following_execution_mode = "PX4"
    app.telemetry_handler = SimpleNamespace(follower=None)
    app._following_session_id = uuid.uuid4().hex
    app._following_stopping = False
    app.camera_runtime = SimpleNamespace(
        provider=SimpleNamespace(), manual_snapshot=lambda: {"gesture_id": None, "state": "stopped"}
    )
    app._get_video_frame_status_for_following = lambda: {
        "source": "independent_sih_world_target",
        "usable_for_following": True,
    }
    app.tracker_trace_recorder = TrackerTraceRecorder(
        tracker_command_trace_path=output / "tracker.jsonl",
        offboard_publish_trace_path=output / "publish.jsonl",
        source="camera_free_sih_evidence",
    )
    app.target_continuity = TargetContinuitySupervisor(
        ContinuityPolicy.resolve(Parameters.TargetContinuity, args.follower)
    )
    app.target_continuity.reset_session(session_epoch=0, reason="sih_session_start")
    records = []

    async def pose():
        snapshot = await asyncio.to_thread(
            lambda: requests.get("http://127.0.0.1:8088/v1/mavlink", timeout=2).json()
        )
        return pose_from_mavlink2rest(snapshot, 1), snapshot

    async def wait_for(predicate, timeout=45):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            await asyncio.sleep(0.1)
        raise RuntimeError("Simulated aircraft readiness deadline expired")

    def target_for(position, quaternion, right, down):
        w, x, y, z = quaternion
        # Independently specify a world point using PX4 body-to-NED basis columns.
        ray = (
            (1 - 2 * (y * y + z * z)) * 30
            + 2 * (x * y - w * z) * right
            + 2 * (x * z + w * y) * down,
            2 * (x * y + w * z) * 30
            + (1 - 2 * (x * x + z * z)) * right
            + 2 * (y * z - w * x) * down,
            2 * (x * z - w * y) * 30
            + 2 * (y * z + w * x) * right
            + (1 - 2 * (x * x + y * y)) * down,
        )
        return tuple(a + b for a, b in zip(position, ray, strict=True))

    async def phase(name, target, duration, *, lost=False, delayed=0):
        begun = time.monotonic()
        while time.monotonic() - begun < duration and app.following_active:
            (position, quaternion), snapshot = await pose()
            now, monotonic = time.time(), time.monotonic()
            angular = camera_angles_for_world_target(position, target, quaternion, args.mount)
            usable = not lost and monotonic - begun >= delayed
            observation = TrackerOutput(
                data_type=TrackerDataType.GIMBAL_ANGLES,
                timestamp=now,
                tracker_id="independent_sih_world_target",
                angular=angular,
                tracking_active=usable,
                target_id=app._tracking_session_generation,
                raw_data={
                    "has_output": True,
                    "usable_for_following": usable,
                    "data_is_stale": False,
                    "identity_confirmed": usable,
                    "angle_sample_timestamp": now,
                    "angle_sample_monotonic": monotonic,
                    "angle_sample_sequence": len(records) + 1,
                    "tracking_sample_timestamp": now,
                    "camera_provider_instance": str(id(app.camera_runtime.provider)),
                    "coordinate_system": "gimbal_body",
                    "tracking_status": "TRACKING_ACTIVE" if usable else "TARGET_SELECTION",
                },
            )
            accepted = await app._dispatch_tracker_output_on_flight_loop(observation)
            record = {
                "phase": name,
                "elapsed_s": monotonic - begun,
                "timestamp": now,
                "submitted_monotonic_s": getattr(
                    getattr(getattr(app, "follower", None), "follower", None),
                    "_submitted_command_timestamp",
                    None,
                ),
                "position_ned": position,
                "body_to_ned_quaternion": quaternion,
                "angular": angular,
                "target_generation": app._tracking_session_generation,
                "accepted": accepted,
                "continuity": app.get_target_continuity_status(),
                "intent": app.get_target_continuity_status().get("effective_command_fields"),
                "px4_attitude": snapshot["vehicles"]["1"]["components"]["1"]["messages"][
                    "ATTITUDE"
                ]["message"],
            }
            records.append(record)
            with (output / "observations.jsonl").open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record) + "\n")
            await asyncio.sleep(0.05)
        return [record for record in records if record["phase"] == name]

    try:
        app.mavlink_data_manager.start_polling()
        await manager.connect()
        await wait_for(lambda: manager.is_command_connection_ready(require_fresh_telemetry=True))
        await manager.observe_aircraft_identity()
        identity = manager.get_aircraft_identity()
        if not identity.get("autopilot_uid"):
            raise RuntimeError("The isolated simulated aircraft UID was not observed")
        app._following_session_aircraft_uid = identity["autopilot_uid"]
        write_json(output / "identity.json", identity)
        # Commands become permissible only after isolated simulator identity observation.
        Parameters.FOLLOWER_CIRCUIT_BREAKER = False

        async def armable():
            async for health in manager.drone.telemetry.health():
                if health.is_armable:
                    return

        await asyncio.wait_for(armable(), timeout=45)
        await manager.drone.action.set_takeoff_altitude(10)
        await manager.drone.action.arm()
        await manager.drone.action.takeoff()
        await wait_for(
            lambda: manager.current_altitude is not None and manager.current_altitude > 8
        )
        # Do not confuse initial estimator/takeoff settling with guidance response.
        settling_deadline = time.monotonic() + 60
        hover_window = deque(maxlen=7)
        while time.monotonic() < settling_deadline:
            await asyncio.sleep(0.5)
            current_pose, current_snapshot = await pose()
            messages = current_snapshot["vehicles"]["1"]["components"]["1"]["messages"]
            hover = {
                "monotonic_s": time.monotonic(),
                "yaw_rad": messages["ATTITUDE"]["message"]["yaw"],
                "down_m": current_pose[0][2],
            }
            hover_window.append(hover)
            with (output / "hover.jsonl").open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(hover) + "\n")
            yaw_difference = hover["yaw_rad"] - hover_window[0]["yaw_rad"]
            yaw_difference = math.degrees(
                math.atan2(math.sin(yaw_difference), math.cos(yaw_difference))
            )
            if (
                len(hover_window) == 7
                and abs(yaw_difference) < 0.6
                and abs(hover["down_m"] - hover_window[0]["down_m"]) < 0.1
            ):
                break
        else:
            raise RuntimeError("SIH hover/heading did not stabilize before guidance injection")
        app.follower = Follower(manager, (0.0, 0.0))
        app.follower.follower.reset_control_session()
        app.offboard_commander = OffboardCommander(
            manager,
            app.follower.follower.setpoint_handler,
            on_publish_result=app._record_offboard_publish_result,
        )
        started = await manager.start_offboard_mode()
        if not started.get("executed"):
            raise RuntimeError(f"SIH Offboard was not entered: {started}")
        await app.offboard_commander.start()
        app.following_active = True
        (position, quaternion), _ = await pose()
        target1 = target_for(position, quaternion, 15, -5)
        right_up = await phase("right_up", target1, 8)
        if not app.following_active:
            raise RuntimeError("Initial guidance handed off unexpectedly")
        prepared = app._prepare_following_target_transition("sih_retarget")
        if not prepared.get("prepared"):
            raise RuntimeError(f"SIH retarget preparation failed: {prepared}")
        app._advance_tracking_session_generation(for_retarget=True)
        app._camera_selection_dispatched_wall_time = time.time()
        app._camera_selection_dispatched_monotonic = time.monotonic()
        (position, quaternion), _ = await pose()
        target2 = target_for(position, quaternion, -8, 15)
        left_down = await phase("retarget_left_down", target2, 9, delayed=0.4)
        loss = await phase("loss", target2, 0.8, lost=True)
        reacquired = await phase("reacquired", target2, 2)
        commander = app.offboard_commander
        concrete = app.follower.follower
        if not app.following_active:
            raise RuntimeError("Bounded retarget/loss recovery stopped before its budget")
        exhausted = await phase("budget_exhaustion", target2, 8.6, lost=True)
        handoff = getattr(app, "_last_following_handoff", {})
        if app.following_active or handoff.get("result") != "confirmed_hold":
            raise RuntimeError(f"Expected one confirmed Hold after exhausted recovery: {handoff}")
        await asyncio.sleep(1)
        status = commander.get_status()
        published = [
            json.loads(line) for line in (output / "publish.jsonl").read_text().splitlines()
        ]
        if status["failed_publishes"] or not all(
            row["publish_status"]["last_publish_success"] for row in published
        ):
            raise RuntimeError("SIH publisher recorded a failure")
        for samples in (left_down, reacquired):
            if samples[-1]["continuity"]["authority_state"] != "ACTIVE":
                raise RuntimeError(
                    "Retarget/reacquisition did not restore confirmed ACTIVE guidance"
                )
        maximum_velocity_step = concrete.authorized_command_acceleration / concrete.update_rate
        maximum_yaw_step = concrete.yaw_smoother.max_rate_change_deg_s2 / concrete.update_rate
        for previous, current in itertools.pairwise(records):
            if not previous["intent"] or not current["intent"]:
                continue
            if current["continuity"]["transition_phase"] == "loss":
                continue  # Safety restrictions intentionally bypass guidance smoothing.
            before, after = previous["intent"], current["intent"]
            step = math.sqrt(
                sum(
                    (after[field] - before[field]) ** 2
                    for field in ("vel_body_fwd", "vel_body_right", "vel_body_down")
                )
            )
            if step > maximum_velocity_step + 1e-6:
                raise RuntimeError(f"Submitted velocity slew exceeded configured bound: {step}")
            if abs(after["yawspeed_deg_s"] - before["yawspeed_deg_s"]) > maximum_yaw_step + 1e-6:
                raise RuntimeError("Submitted yaw slew exceeded configured bound")
        actual_intents = [row["command_intent"] for row in published if row.get("command_intent")]
        for previous, current in itertools.pairwise(actual_intents):
            if current["reason"] == "bounded_horizontal_decay":
                continue
            elapsed = (
                datetime.fromisoformat(current["created_at_utc"])
                - datetime.fromisoformat(previous["created_at_utc"])
            ).total_seconds()
            before, after = previous["fields"], current["fields"]
            step = math.sqrt(
                sum(
                    (after[field] - before[field]) ** 2
                    for field in ("vel_body_fwd", "vel_body_right", "vel_body_down")
                )
            )
            if step > concrete.authorized_command_acceleration * max(0.0, elapsed) + 1e-4:
                raise RuntimeError(
                    "Actual published velocity slew exceeded configured acceleration"
                )
            if (
                abs(after["yawspeed_deg_s"] - before["yawspeed_deg_s"])
                > concrete.yaw_smoother.max_rate_change_deg_s2 * max(0.0, elapsed) + 1e-4
            ):
                raise RuntimeError("Actual published yaw slew exceeded configured acceleration")
        response_evidence = []
        for name, samples, yaw_sign, altitude_sign in (
            ("right_up", right_up, 1, 1),
            ("left_down", left_down, -1, -1),
        ):
            initial, final = samples[0], samples[-1]
            publications = [
                row
                for row in published
                if initial["timestamp"] <= row["timestamp"] <= final["timestamp"]
                and row.get("command_intent")
            ]
            if not any(
                row["command_intent"]["fields"]["yawspeed_deg_s"] * yaw_sign > 0.05
                and row["command_intent"]["fields"]["vel_body_down"] * altitude_sign < -0.01
                for row in publications
            ):
                raise RuntimeError(
                    f"Expected turn/vertical signs were not actually published: {name}"
                )
            yaw0, yaw1 = initial["px4_attitude"]["yaw"], final["px4_attitude"]["yaw"]
            yaw_change = math.degrees(math.atan2(math.sin(yaw1 - yaw0), math.cos(yaw1 - yaw0)))
            altitude_change = initial["position_ned"][2] - final["position_ned"][2]
            if yaw_change * yaw_sign <= 0.2 or altitude_change * altitude_sign <= 0.03:
                raise RuntimeError(
                    f"Wrong/insufficient independent PX4 response {name}: yaw={yaw_change}, altitude={altitude_change}"
                )
            response_evidence.append(
                {
                    "phase": name,
                    "yaw_change_deg": yaw_change,
                    "altitude_change_m": altitude_change,
                    "publications": len(publications),
                }
            )
        if any(
            abs((row["continuity"].get("effective_command_fields") or {}).get(field, 0)) > 1e-9
            for row in loss
            for field in ("vel_body_down", "yawspeed_deg_s")
        ):
            raise RuntimeError("Ordinary loss retained yaw or vertical guidance")
        write_json(
            output / "result.json",
            {
                "passed": True,
                "follower": args.follower,
                "mount": args.mount,
                "publication": status,
                "handoff": handoff,
                "response_evidence": response_evidence,
                "slew_bounds": {
                    "velocity_step_m_s": maximum_velocity_step,
                    "yaw_step_deg_s": maximum_yaw_step,
                },
                "counts": {
                    "right_up": len(right_up),
                    "retarget_left_down": len(left_down),
                    "loss": len(loss),
                    "reacquired": len(reacquired),
                    "exhausted": len(exhausted),
                },
                "claims": "Production dispatch/continuity/shaping/publication and independent simulated response only",
            },
        )
    finally:
        if getattr(app, "offboard_commander", None):
            await app.offboard_commander.stop()
        if manager.active_mode:
            await manager.stop_offboard_mode()
            await manager.drone.action.land()
        await manager.stop()
        app.mavlink_data_manager.stop_polling()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument(
        "--backend-repo", type=Path, default=Path("/home/alireza/PixEagle-qgc-integration")
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--follower", choices=("gm_velocity_chase", "gm_velocity_vector"), required=True
    )
    parser.add_argument("--mount", choices=("HORIZONTAL", "VERTICAL"), required=True)
    parser.add_argument(
        "--python",
        type=Path,
        default=Path.home()
        / ".cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/runtime/full-ai-venv/bin/python",
    )
    parser.add_argument("--bin-dir", type=Path, default=Path.home() / "PixEagle/bin")
    parser.add_argument("--opencv-lib", type=Path, default=Path.home() / "PixEagle/.venv/lib")
    args = parser.parse_args()
    if args.worker:
        asyncio.run(worker(args))
    elif args.output is None:
        parser.error("--output is required")
    else:
        host(args)


if __name__ == "__main__":
    main()
