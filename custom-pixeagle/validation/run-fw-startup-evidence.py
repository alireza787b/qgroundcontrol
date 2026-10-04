#!/usr/bin/env python3
"""Reproduce pinned airplane SIH startup without camera, following or Offboard.

The simulator-only thrust correction matches the 6 N requested by airframe 10041
using its current fixed-wing parameter. This does not qualify following or stall limits.
"""

from __future__ import annotations

import argparse
import asyncio
import hashlib
import importlib.util
import json
import math
import os
import time
from pathlib import Path
from types import SimpleNamespace

PX4_SOURCE_REVISION = "381149fb012762f5e38c4a7fdc1b905b28038970"
BASELINE = {
    "SYS_AUTOSTART": 10041,
    "SIH_T_MAX": 6.0,
    "SIH_F_T_MAX": 2.0,
    "SIH_F_CT0": 0.0,
    "SIH_KDV": 0.2,
    "RWTO_TKOFF": 1,
    "FW_AIRSPD_MIN": 10.0,
    "FW_TKO_AIRSPD": -1.0,
    "RWTO_ROT_AIRSPD": -1.0,
}


def validate_baseline(values):
    """Refuse to tune any fixture other than the characterized pinned airplane."""
    for name, expected in BASELINE.items():
        actual = values.get(name)
        if (
            isinstance(actual, bool)
            or not isinstance(actual, (int, float))
            or not math.isfinite(actual)
            or abs(actual - expected) > 1e-5
        ):
            raise RuntimeError(f"Pinned SIH baseline mismatch for {name}: {actual!r}")


def runtime_module():
    directory = Path(__file__).resolve().parent
    path = directory / "sih_evidence_runtime.py"
    if not path.exists() and os.environ.get("EVIDENCE_OWNER"):
        path = Path("/work/evidence/sih_evidence_runtime.py")
    if not path.exists():
        path = directory / "run-gimbal-sih-evidence.py"
    spec = importlib.util.spec_from_file_location("sih_evidence_runtime", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def configure(config):
    config["Tracking"]["DEFAULT_TRACKING_ALGORITHM"] = "CSRT"
    config["FOLLOWER_CIRCUIT_BREAKER"] = True


async def worker(args):
    if not os.environ.get("EVIDENCE_OWNER") or set(os.listdir("/sys/class/net")) != {"lo"}:
        raise RuntimeError("Worker requires the owned loopback-only Docker namespace")
    import logging

    import requests
    from classes.mavlink_data_manager import MavlinkDataManager
    from classes.parameters import Parameters
    from classes.px4_interface_manager import PX4InterfaceManager
    from sitl_gimbal_geometry_fixture import pose_from_mavlink2rest

    logging.basicConfig(level=logging.WARNING)
    output = Path("/work/evidence")
    runtime = runtime_module()
    app = SimpleNamespace()
    app.mavlink_data_manager = MavlinkDataManager(
        Parameters.MAVLINK_HOST,
        Parameters.MAVLINK_PORT,
        Parameters.MAVLINK_POLLING_INTERVAL,
        Parameters.MAVLINK_DATA_POINTS,
        enabled=True,
    )
    manager = PX4InterfaceManager(app)
    measurement_task = None

    async def snapshot():
        result = await asyncio.to_thread(
            lambda: requests.get("http://127.0.0.1:8088/v1/mavlink", timeout=2).json()
        )
        position, quaternion = pose_from_mavlink2rest(result, 1)
        attitude = result["vehicles"]["1"]["components"]["1"]["messages"]["ATTITUDE"]["message"]
        return {"position_ned": position, "quaternion": quaternion, "attitude": attitude}

    async def ready(predicate, timeout=60):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            await asyncio.sleep(0.1)
        raise RuntimeError("Simulated aircraft readiness deadline expired")

    try:
        app.mavlink_data_manager.start_polling()
        await manager.connect()
        await ready(lambda: manager.is_command_connection_ready(require_fresh_telemetry=True))
        await manager.observe_aircraft_identity()
        identity = manager.get_aircraft_identity()
        if not identity.get("autopilot_uid"):
            raise RuntimeError("No simulated UID")
        runtime.write_json(output / "identity.json", identity)
        Parameters.FOLLOWER_CIRCUIT_BREAKER = True
        before = {}
        for name in BASELINE:
            if name in {"SYS_AUTOSTART", "RWTO_TKOFF"}:
                before[name] = await manager.drone.param.get_param_int(name)
            else:
                before[name] = await manager.drone.param.get_param_float(name)
        validate_baseline(before)
        await manager.drone.param.set_param_float("SIH_F_T_MAX", 6.0)
        after = dict(before)
        after["SIH_F_T_MAX"] = await manager.drone.param.get_param_float("SIH_F_T_MAX")
        if after["SIH_F_T_MAX"] != 6.0:
            raise RuntimeError("Simulator thrust correction was not confirmed")
        runtime.write_json(
            output / "parameters.json",
            {
                "before": before,
                "after": after,
                "simulator_only_override": True,
                "reason": "Pinned airframe sets obsolete SIH_T_MAX=6; airplane dynamics consume SIH_F_T_MAX default2",
            },
        )

        async def armable():
            async for health in manager.drone.telemetry.health():
                if health.is_armable:
                    return

        await asyncio.wait_for(armable(), 45)
        await manager.drone.telemetry.set_rate_fixedwing_metrics(10)
        airspeed = {}

        async def measure_airspeed():
            async for metric in manager.drone.telemetry.fixedwing_metrics():
                airspeed.update(value=metric.airspeed_m_s, timestamp=time.time())

        measurement_task = asyncio.create_task(measure_airspeed())
        await manager.drone.action.set_takeoff_altitude(50)
        await manager.drone.action.arm()
        await manager.drone.action.takeoff()
        deadline = time.monotonic() + 90
        reached = False
        observations = []
        while time.monotonic() < deadline:
            measured = await snapshot()
            row = {
                "timestamp": time.time(),
                "position_ned": measured["position_ned"],
                "attitude": measured["attitude"],
                "relative_altitude": manager.current_altitude,
                "ground_speed": getattr(manager, "current_ground_speed", None),
                "indicated_airspeed": dict(airspeed),
            }
            observations.append(row)
            with (output / "startup-observations.jsonl").open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(row) + "\n")
            if manager.current_altitude is not None and manager.current_altitude > 48:
                reached = True
                break
            await asyncio.sleep(0.5)
        measurement_task.cancel()
        await asyncio.gather(measurement_task, return_exceptions=True)
        runtime.write_json(
            output / "result.json",
            {
                "passed": reached,
                "stage": "airplane_startup_only",
                "release_qualified": False,
                "follower_qualified": False,
                "offboard_requested": False,
                "follower_commands_sent": 0,
                "physical_camera": False,
                "simulator_model": "10041_sihsim_airplane",
                "simulator_only_thrust_override": 6,
                "commands_blocked": bool(Parameters.FOLLOWER_CIRCUIT_BREAKER),
                "duration_s": observations[-1]["timestamp"] - observations[0]["timestamp"],
                "max_relative_altitude": max(row["relative_altitude"] or 0 for row in observations),
                "max_indicated_airspeed": max(
                    row["indicated_airspeed"].get("value", 0) for row in observations
                ),
                "final_observation": observations[-1],
            },
        )
        if not reached:
            raise RuntimeError("Airplane startup-only probe did not reach 48 m")
    finally:
        if measurement_task is not None:
            measurement_task.cancel()
            await asyncio.gather(measurement_task, return_exceptions=True)
        await manager.stop()
        app.mavlink_data_manager.stop_polling()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument(
        "--backend-repo", type=Path, default=Path.home() / "PixEagle-qgc-integration"
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--follower",
        choices=["fw_attitude_rate"],
        default="fw_attitude_rate",
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--mount", choices=["HORIZONTAL"], default="HORIZONTAL", help=argparse.SUPPRESS
    )
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
        try:
            asyncio.run(worker(args))
        except Exception as exc:
            output = Path("/work/evidence")
            if os.environ.get("EVIDENCE_OWNER") and output.exists():
                result_path = output / "result.json"
                result = (
                    json.loads(result_path.read_text())
                    if result_path.exists()
                    else {
                        "passed": False,
                        "release_qualified": False,
                        "follower": args.follower,
                        "physical_camera": False,
                        "publication_evidence_present": False,
                    }
                )
                result.update(passed=False, reason=str(exc))
                runtime_module().write_json(result_path, result)
            raise
    elif args.output is None:
        parser.error("--output is required")
    else:
        args.simulator_model = "sihsim_airplane"
        args.evidence_kind = "fixed-wing-sih-startup-only-diagnostic"
        runtime = runtime_module()

        def configure_snapshot(config):
            configure(config)
            runtime.write_json(
                args.output.resolve() / "startup-provenance.json",
                {
                    "driver_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                    "source_revision": PX4_SOURCE_REVISION,
                    "source_url": f"https://github.com/PX4/PX4-Autopilot/tree/{PX4_SOURCE_REVISION}",
                    "source_basis": "Pinned image airframe10041 and binary Git revision characterized by prior startup probe",
                    "simulator_only_override": {"SIH_F_T_MAX": 6.0},
                    "claims": "Takeoff readiness only; no Offboard, following, stall or hardware qualification",
                },
            )

        runtime.host(args, driver_path=Path(__file__), configure=configure_snapshot)


if __name__ == "__main__":
    main()
