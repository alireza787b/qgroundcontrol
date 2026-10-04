#!/usr/bin/env python3
"""Camera-free image-target guidance through production publication into PX4 SIH.

Inputs are normalized target measurements, not camera motor angles. This proves
command direction, publication and independent simulated response; it does not
qualify tracker acquisition, optics, installation calibration or visual convergence.
"""

from __future__ import annotations

import argparse
import asyncio
import importlib.util
import json
import math
import os
import threading
import time
import uuid
from pathlib import Path
from types import SimpleNamespace

MODES = (
    "mc_velocity_chase",
    "mc_velocity_ground",
    "mc_velocity_distance",
    "mc_velocity_position",
    "mc_attitude_rate",
    "fw_attitude_rate",
)


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
    config["Follower"]["General"]["ENABLE_ALTITUDE_CONTROL"] = True
    # A simulator overlay: fixed camera at the normalized image aim, not a
    # camera-owned tracker or a guessed physical mount transform.
    config["MC_ATTITUDE_RATE"]["TARGET_ALTITUDE_OFFSET"] = 0.0


def signed_angle_change(before, after):
    difference = after - before
    return math.degrees(math.atan2(math.sin(difference), math.cos(difference)))


def expectation(mode, direction):
    if mode == "mc_velocity_ground":
        return {"vel_body_right": direction, "vel_body_fwd": direction}
    if mode == "mc_velocity_distance":
        return {"vel_body_right": direction, "vel_body_down": -direction}
    if mode in {"mc_attitude_rate", "fw_attitude_rate"}:
        return {"yawspeed_deg_s": direction, "pitchspeed_deg_s": direction}
    return {"yawspeed_deg_s": direction, "vel_body_down": -direction}


async def worker(args):
    if not os.environ.get("EVIDENCE_OWNER") or set(os.listdir("/sys/class/net")) != {"lo"}:
        raise RuntimeError("Worker requires the owned loopback-only Docker namespace")
    import logging

    import requests
    from classes.app_controller import AppController
    from classes.follower import Follower
    from classes.mavlink_data_manager import MavlinkDataManager
    from classes.offboard_commander import OffboardCommander
    from classes.parameters import Parameters
    from classes.px4_interface_manager import PX4InterfaceManager, _evaluate_px4_command_gate
    from classes.target_continuity import ContinuityPolicy, TargetContinuitySupervisor
    from classes.tracker_output import TrackerDataType, TrackerOutput
    from classes.tracker_trace import TrackerTraceRecorder
    from sitl_gimbal_geometry_fixture import pose_from_mavlink2rest

    logging.basicConfig(level=logging.WARNING)
    output = Path("/work/evidence")
    runtime = runtime_module()
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
    app.camera_runtime = SimpleNamespace(provider=None)
    app._get_video_frame_status_for_following = lambda: {
        "source": "normalized_image_fixture",
        "usable_for_following": True,
    }
    app.tracker_trace_recorder = TrackerTraceRecorder(
        tracker_command_trace_path=output / "tracker.jsonl",
        offboard_publish_trace_path=output / "publish.jsonl",
        source="normal_camera_free_sih",
    )
    app.target_continuity = TargetContinuitySupervisor(
        ContinuityPolicy.resolve(Parameters.TargetContinuity, args.follower)
    )
    app.target_continuity.reset_session(session_epoch=0, reason="normal_sih_start")
    records = []

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

    async def phase(name, coordinates, duration):
        begun = time.monotonic()
        while time.monotonic() - begun < duration and app.following_active:
            measured = await snapshot()
            x, y = coordinates
            observation = TrackerOutput(
                data_type=TrackerDataType.POSITION_2D,
                timestamp=time.time(),
                tracking_active=True,
                tracker_id="local_image_fixture",
                position_2d=(x, y),
                normalized_bbox=(x - 0.025, y - 0.025, 0.05, 0.05),
                confidence=0.99,
                target_id=0,
                raw_data={
                    "has_output": True,
                    "usable_for_following": True,
                    "data_is_stale": False,
                    "identity_confirmed": True,
                },
            )
            accepted = await app._dispatch_tracker_output_on_flight_loop(observation)
            record = {
                "phase": name,
                "timestamp": time.time(),
                "coordinates": coordinates,
                "accepted": accepted,
                **measured,
                "continuity": app.get_target_continuity_status(),
            }
            records.append(record)
            with (output / "observations.jsonl").open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record) + "\n")
            if not accepted:
                raise RuntimeError(f"Production dispatch rejected {name}: {record['continuity']}")
            await asyncio.sleep(0.02 if "attitude" in args.follower else 0.05)
        if not app.following_active:
            raise RuntimeError(f"Following stopped unexpectedly in {name}")
        return [row for row in records if row["phase"] == name]

    try:
        app.mavlink_data_manager.start_polling()
        await manager.connect()
        await ready(lambda: manager.is_command_connection_ready(require_fresh_telemetry=True))
        await manager.observe_aircraft_identity()
        identity = manager.get_aircraft_identity()
        if not identity.get("autopilot_uid"):
            raise RuntimeError("No simulated aircraft UID was observed")
        app._following_session_aircraft_uid = identity["autopilot_uid"]
        runtime.write_json(output / "identity.json", identity)
        expected_airframe = "fixed_wing" if args.follower.startswith("fw_") else "multicopter"
        Parameters.FOLLOWER_CIRCUIT_BREAKER = False

        async def armable():
            async for health in manager.drone.telemetry.health():
                if health.is_armable:
                    return

        await asyncio.wait_for(armable(), timeout=45)
        altitude = 50 if expected_airframe == "fixed_wing" else 10
        await manager.drone.action.set_takeoff_altitude(altitude)
        await manager.drone.action.arm()
        await manager.drone.action.takeoff()
        await ready(
            lambda: manager.current_altitude is not None and manager.current_altitude > altitude - 2
        )
        if expected_airframe == "fixed_wing":
            await ready(lambda: (getattr(manager, "current_ground_speed", None) or 0) > 14)
        else:
            stable = []
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                measured = await snapshot()
                stable.append(measured)
                stable = stable[-7:]
                if (
                    len(stable) == 7
                    and abs(
                        signed_angle_change(
                            stable[0]["attitude"]["yaw"], measured["attitude"]["yaw"]
                        )
                    )
                    < 0.6
                    and abs(stable[0]["position_ned"][2] - measured["position_ned"][2]) < 0.1
                ):
                    break
                await asyncio.sleep(0.5)
            else:
                raise RuntimeError("SIH hover failed to settle")
        app.follower = Follower(manager, (0.0, 0.0))
        concrete = app.follower.follower
        concrete.reset_control_session()
        if app.follower.get_airframe_phase() != expected_airframe:
            raise RuntimeError("Follower airframe contract disagrees with the simulator")
        app.offboard_commander = OffboardCommander(
            manager,
            concrete.setpoint_handler,
            on_publish_result=app._record_offboard_publish_result,
        )
        started = await manager.start_offboard_mode()
        if not started.get("executed"):
            raise RuntimeError(f"Offboard failed: {started}")
        await app.offboard_commander.start()
        app.following_active = True
        duration = args.phase_duration or (1.2 if "attitude" in args.follower else 5)
        first = await phase("right_up", (0.15, -0.12), duration)
        second = await phase("left_down", (-0.15, 0.12), duration)
        commander = app.offboard_commander
        await app._stop_following_after_continuity_failure("operator_stop")
        handoff = getattr(app, "_last_following_handoff", {})
        if handoff.get("result") != "confirmed_hold" or app.following_active:
            raise RuntimeError(f"Stop did not confirm Hold: {handoff}")
        stop_count = len((output / "publish.jsonl").read_text().splitlines())
        await asyncio.sleep(0.3)
        if len((output / "publish.jsonl").read_text().splitlines()) != stop_count:
            raise RuntimeError("Publication continued after confirmed Stop")
        Parameters.FOLLOWER_CIRCUIT_BREAKER = True
        blocked = _evaluate_px4_command_gate("normal_sih_post_stop_probe")
        if not blocked.blocked or blocked.degraded or blocked.reason != "circuit_breaker_active":
            raise RuntimeError("The canonical circuit breaker did not block the probe")
        publications = [
            json.loads(line) for line in (output / "publish.jsonl").read_text().splitlines()
        ]
        if not publications or commander.get_status()["failed_publishes"]:
            raise RuntimeError("Publication failure or empty evidence")
        results = []
        for name, samples, direction in (("right_up", first, 1), ("left_down", second, -1)):
            rows = [
                row
                for row in publications
                if samples[0]["timestamp"] <= row["timestamp"] <= samples[-1]["timestamp"]
                and row.get("command_intent")
            ]
            required = expectation(args.follower, direction)
            if not any(
                all(
                    row["command_intent"]["fields"][field] * sign > 1e-4
                    for field, sign in required.items()
                )
                for row in rows
            ):
                raise RuntimeError(f"Expected command directions not published: {name}, {required}")
            initial, final = samples[0], samples[-1]
            yaw = signed_angle_change(initial["attitude"]["yaw"], final["attitude"]["yaw"])
            pitch = signed_angle_change(initial["attitude"]["pitch"], final["attitude"]["pitch"])
            translation = [
                b - a for a, b in zip(initial["position_ned"], final["position_ned"], strict=True)
            ]
            # Independent NED displacement projected onto the initial PX4
            # heading; no follower math participates in the expectation.
            heading = initial["attitude"]["yaw"]
            body_right = -math.sin(heading) * translation[0] + math.cos(heading) * translation[1]
            altitude_change = -translation[2]
            response = (
                pitch
                if "attitude" in args.follower
                else body_right
                if args.follower in {"mc_velocity_ground", "mc_velocity_distance"}
                else yaw
            )
            if response * direction <= (
                0.005 if args.follower in {"mc_velocity_ground", "mc_velocity_distance"} else 0.05
            ):
                raise RuntimeError(
                    f"Independent PX4 response did not match {name}: yaw={yaw}, pitch={pitch}, right={body_right}"
                )
            if (
                args.follower
                in {
                    "mc_velocity_chase",
                    "mc_velocity_distance",
                    "mc_velocity_position",
                }
                and altitude_change * direction <= 0.025
            ):
                raise RuntimeError(
                    f"Independent climb/descent response did not match {name}: {altitude_change}"
                )
            results.append(
                {
                    "phase": name,
                    "published_count": len(rows),
                    "yaw_change_deg": yaw,
                    "pitch_change_deg": pitch,
                    "body_right_displacement_m": body_right,
                    "altitude_change_m": altitude_change,
                    "required_command_signs": required,
                }
            )
        for row in publications:
            fields = (row.get("command_intent") or {}).get("fields", {})
            if not all(math.isfinite(value) for value in fields.values()):
                raise RuntimeError("Nonfinite published command")
            if concrete.get_control_type() == "velocity_body_offboard":
                limits = concrete.velocity_limits
                for field, bound in (
                    ("vel_body_fwd", limits.forward),
                    ("vel_body_right", limits.lateral),
                    ("vel_body_down", limits.vertical),
                ):
                    if abs(fields.get(field, 0)) > bound + 1e-6:
                        raise RuntimeError(f"Published field exceeds Safety bound: {field}")
                speed = math.sqrt(
                    sum(
                        fields.get(field, 0) ** 2
                        for field in ("vel_body_fwd", "vel_body_right", "vel_body_down")
                    )
                )
                if speed > limits.max_magnitude + 1e-6:
                    raise RuntimeError("Published velocity exceeds Safety magnitude")
            rates = concrete.rate_limits
            for field, limit in (
                ("yawspeed_deg_s", rates.yaw),
                ("pitchspeed_deg_s", rates.pitch),
                ("rollspeed_deg_s", rates.roll),
            ):
                if abs(fields.get(field, 0)) > math.degrees(limit) + 1e-6:
                    raise RuntimeError(f"Published rate exceeds Safety bound: {field}")
            if "thrust" in fields and not 0 <= fields["thrust"] <= 1:
                raise RuntimeError("Published thrust outside normalized contract")
        measured_pitch_max = max(abs(math.degrees(row["attitude"]["pitch"])) for row in records)
        measured_roll_max = max(abs(math.degrees(row["attitude"]["roll"])) for row in records)
        pitch_limit = float(Parameters.MC_ATTITUDE_RATE["MAX_PITCH_ANGLE"])
        roll_limit = float(Parameters.MC_ATTITUDE_RATE["MAX_ROLL_ANGLE"])
        if (
            args.follower == "mc_attitude_rate"
            and args.require_attitude_envelope
            and (measured_pitch_max > pitch_limit or measured_roll_max > roll_limit)
        ):
            raise RuntimeError(
                f"Measured MC envelope exceeded: pitch={measured_pitch_max}/{pitch_limit}, roll={measured_roll_max}/{roll_limit}"
            )
        attitude_envelope_exceeded = (
            args.follower == "mc_attitude_rate" and measured_pitch_max > pitch_limit
        )
        runtime.write_json(
            output / "result.json",
            {
                "passed": True,
                "passed_scope": "Command signs, publication bounds, selected independent response, Stop and post-Stop CB gate only",
                "release_qualified": False,
                "follower": args.follower,
                "airframe": expected_airframe,
                "physical_camera": False,
                "tracker_acquisition_qualified": False,
                "input": "normalized POSITION_2D plus bounding box and confidence",
                "test_overlay": {
                    "ENABLE_ALTITUDE_CONTROL": True,
                    "MC_ATTITUDE_RATE.TARGET_ALTITUDE_OFFSET": 0,
                },
                "publication": commander.get_status(),
                "response_evidence": results,
                "handoff": handoff,
                "cb_probe": blocked.reason,
                "cb_scope": "Canonical gate probe after confirmed Stop; not an active-follow CB-transition test",
                "measured_absolute_pitch_max_deg": measured_pitch_max,
                "measured_absolute_roll_max_deg": measured_roll_max,
                "measured_envelope_asserted": args.require_attitude_envelope,
                "phase_duration_s": duration,
                "mc_attitude_pitch_limit_deg": pitch_limit
                if args.follower == "mc_attitude_rate"
                else None,
                "attitude_envelope_exceeded": attitude_envelope_exceeded,
                "release_blocker": (
                    "Measured pitch exceeds configured pitch angle. Direct-rate angle envelope and coupled yaw need separate qualification."
                    if attitude_envelope_exceeded
                    else None
                ),
                "fixed_wing_airspeed_qualified": False,
                "fixed_wing_limitation": (
                    "Production controller has no airspeed field; follower uses ground speed. Zero-wind simulated guidance only; airspeed/stall qualification remains blocked."
                    if expected_airframe == "fixed_wing"
                    else None
                ),
                "claims": "Production dispatch/publication and independent simulated response only",
            },
        )
    finally:
        Parameters.FOLLOWER_CIRCUIT_BREAKER = False
        if getattr(app, "offboard_commander", None):
            await app.offboard_commander.stop()
        if manager.active_mode:
            await manager.stop_offboard_mode()
        await manager.stop()
        app.mavlink_data_manager.stop_polling()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--phase-duration", type=float)
    parser.add_argument("--require-attitude-envelope", action="store_true")
    parser.add_argument(
        "--backend-repo", type=Path, default=Path.home() / "PixEagle-qgc-integration"
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--follower", choices=MODES, required=True)
    parser.add_argument("--mount", default="HORIZONTAL", help=argparse.SUPPRESS)
    parser.add_argument(
        "--python",
        type=Path,
        default=Path.home()
        / ".cache/pixeagle-qgc-baseline/slice-3b-2026-09-22/runtime/full-ai-venv/bin/python",
    )
    parser.add_argument("--bin-dir", type=Path, default=Path.home() / "PixEagle/bin")
    parser.add_argument("--opencv-lib", type=Path, default=Path.home() / "PixEagle/.venv/lib")
    args = parser.parse_args()
    if args.phase_duration is not None and (
        not math.isfinite(args.phase_duration) or not 0 < args.phase_duration <= 10
    ):
        parser.error("--phase-duration must be finite and in (0, 10] seconds")
    if args.worker:
        try:
            asyncio.run(worker(args))
        except Exception as exc:
            output = Path("/work/evidence")
            if os.environ.get("EVIDENCE_OWNER") and output.exists():
                runtime_module().write_json(
                    output / "result.json",
                    {
                        "passed": False,
                        "release_qualified": False,
                        "follower": args.follower,
                        "reason": str(exc),
                        "physical_camera": False,
                        "publication_evidence_present": (output / "publish.jsonl").exists(),
                    },
                )
            raise
    elif args.output is None:
        parser.error("--output is required")
    else:
        args.simulator_model = (
            "sihsim_airplane" if args.follower.startswith("fw_") else "sihsim_quadx"
        )
        args.worker_extra_args = []
        if args.phase_duration is not None:
            args.worker_extra_args.extend(["--phase-duration", str(args.phase_duration)])
        if args.require_attitude_envelope:
            args.worker_extra_args.append("--require-attitude-envelope")
        args.evidence_kind = "normal-camera-production-dispatch-sih"
        runtime_module().host(args, driver_path=Path(__file__), configure=configure)


if __name__ == "__main__":
    main()
