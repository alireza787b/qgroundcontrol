"""Camera-free SIH launch boundaries without starting Docker or PX4."""

import argparse
import importlib.util
import json
import subprocess
from pathlib import Path

import pytest
import yaml


@pytest.fixture
def harness():
    path = Path(__file__).parents[2] / "custom-pixeagle/validation/run-gimbal-sih-evidence.py"
    spec = importlib.util.spec_from_file_location("gimbal_sih", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("custom_driver", [False, True])
def test_owned_loopback_launcher_preserves_driver_and_model(
    harness, tmp_path, monkeypatch, custom_driver
):
    repo = tmp_path / "repo"
    (repo / "src/classes").mkdir(parents=True)
    (repo / "src/classes/app_controller.py").write_text("# immutable source\n")
    (repo / "tools").mkdir()
    (repo / "tools/sitl_gimbal_geometry_fixture.py").write_text("# fixture\n")
    (repo / "configs").mkdir()
    config = {
        "PX4": {},
        "MAVLink": {},
        "GimbalTracker": {},
        "Follower": {"FollowerOverrides": {"GM_VELOCITY_VECTOR": {}}},
    }
    (repo / "configs/config_default.yaml").write_text(yaml.safe_dump(config))
    binaries = tmp_path / "bin"
    binaries.mkdir()
    for name in ("mavsdk_server_bin", "mavlink2rest", "python"):
        (binaries / name).write_text(name)
    args = argparse.Namespace(
        execute=True,
        backend_repo=repo,
        output=tmp_path / "evidence",
        python=binaries / "python",
        bin_dir=binaries,
        follower="gm_velocity_chase",
        mount="HORIZONTAL",
        opencv_lib=tmp_path,
    )
    driver = tmp_path / "normal.py"
    driver.write_text("# driver snapshot\n")
    calls, owners = [], {}

    def command(*command, **kwargs):
        calls.append(command)
        if command[:3] == ("docker", "image", "inspect"):
            return json.dumps([{"Id": "pinned-image"}])
        if command[:2] == ("docker", "run"):
            name = command[command.index("--name") + 1]
            label = command[command.index("--label") + 1]
            owners[name] = label.split("=", 1)[1]
            return name
        if command[:2] == ("docker", "inspect"):
            return json.dumps([{"State": {"Running": False, "ExitCode": 0}}])
        if command[0] == "git":
            return "source-head"
        return "[]"

    def run(command, **kwargs):
        calls.append(tuple(command))
        if command[:2] == ["docker", "inspect"]:
            return subprocess.CompletedProcess(
                command,
                0,
                json.dumps([{"Config": {"Labels": {harness.LABEL: owners[command[-1]]}}}]),
            )
        return subprocess.CompletedProcess(command, 0, "")

    monkeypatch.setattr(harness, "command", command)
    monkeypatch.setattr(harness.subprocess, "run", run)
    if custom_driver:
        args.simulator_model = "sihsim_airplane"
        args.evidence_kind = "normal-camera-production-dispatch-sih"
        args.worker_extra_args = ["--phase-duration", "3", "--require-attitude-envelope"]
        harness.host(args, driver_path=driver, configure=lambda cfg: cfg.update(fixture_only=True))
        assert (args.output / "driver.py").read_text() == "# driver snapshot\n"
        assert (args.output / "sih_evidence_runtime.py").is_file()
        assert (
            "--phase-duration 3 --require-attitude-envelope"
            in (args.output / "entry.sh").read_text()
        )
    else:
        harness.host(args)
        assert (args.output / "driver.py").read_bytes() == Path(harness.__file__).read_bytes()
    launches = [call for call in calls if call[:2] == ("docker", "run")]
    assert len(launches) == 2
    assert launches[0][launches[0].index("--network") + 1] == "none"
    assert launches[1][launches[1].index("--network") + 1].startswith(
        "container:pixeagle-evidence-"
    )
    assert all(
        "-p" not in call and "--publish" not in call and "--network=host" not in call
        for call in launches
    )
    assert f"PX4_SIM_MODEL={'sihsim_airplane' if custom_driver else 'sihsim_quadx'}" in launches[0]
    driver.write_text("# later mutation\n")
    if custom_driver:
        assert (args.output / "driver.py").read_text() == "# driver snapshot\n"
    assert len([call for call in calls if call[:3] == ("docker", "rm", "-f")]) == 2


def test_explicit_simulation_authorization_precedes_filesystem_mutation(harness, tmp_path):
    args = argparse.Namespace(execute=False, output=tmp_path / "never-created")
    with pytest.raises(SystemExit, match="--execute"):
        harness.host(args)
    assert not args.output.exists()
