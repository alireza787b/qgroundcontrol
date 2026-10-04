"""Startup-only airplane fixture boundaries without Docker or aircraft commands."""

import argparse
import asyncio
import importlib.util
import json
from pathlib import Path

import pytest


@pytest.fixture
def probe():
    path = Path(__file__).parents[2] / "custom-pixeagle/validation/run-fw-startup-evidence.py"
    spec = importlib.util.spec_from_file_location("fw_startup_evidence", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("value", [None, True, float("nan"), float("inf"), 6.0, "2"])
def test_unknown_simulator_baseline_is_rejected(probe, value):
    values = dict(probe.BASELINE, SIH_F_T_MAX=value)
    with pytest.raises(RuntimeError, match="SIH_F_T_MAX"):
        probe.validate_baseline(values)


def test_observed_float_rounding_does_not_change_the_baseline(probe):
    probe.validate_baseline(dict(probe.BASELINE, SIH_KDV=0.20000000298023224))


@pytest.mark.parametrize("interfaces,owner", [(["lo", "eth0"], "test"), (["lo"], "")])
def test_worker_requires_owned_isolated_namespace(probe, monkeypatch, interfaces, owner):
    monkeypatch.setattr(probe.os, "listdir", lambda path: interfaces)
    monkeypatch.setenv("EVIDENCE_OWNER", owner)
    with pytest.raises(RuntimeError, match="owned loopback-only"):
        asyncio.run(probe.worker(argparse.Namespace()))


def test_explicit_execution_precedes_output_creation(probe, tmp_path, monkeypatch):
    monkeypatch.setattr("sys.argv", ["probe", "--output", str(tmp_path / "evidence")])
    with pytest.raises(SystemExit, match="--execute"):
        probe.main()
    assert not (tmp_path / "evidence").exists()


def test_startup_snapshot_keeps_commands_blocked_and_driver_immutable(probe, tmp_path, monkeypatch):
    output = tmp_path / "evidence"
    monkeypatch.setattr("sys.argv", ["probe", "--execute", "--output", str(output)])
    runtime = probe.runtime_module()

    def host(args, *, driver_path, configure):
        assert args.simulator_model == "sihsim_airplane"
        assert args.evidence_kind == "fixed-wing-sih-startup-only-diagnostic"
        assert args.follower == "fw_attitude_rate"
        output.mkdir()
        config = {"Tracking": {}, "FOLLOWER_CIRCUIT_BREAKER": False}
        configure(config)
        assert config["FOLLOWER_CIRCUIT_BREAKER"] is True
        assert config["Tracking"]["DEFAULT_TRACKING_ALGORITHM"] == "CSRT"
        provenance = json.loads((output / "startup-provenance.json").read_text())
        assert provenance["simulator_only_override"] == {"SIH_F_T_MAX": 6.0}
        assert (
            provenance["driver_sha256"]
            == probe.hashlib.sha256(driver_path.read_bytes()).hexdigest()
        )

    monkeypatch.setattr(runtime, "host", host)
    monkeypatch.setattr(probe, "runtime_module", lambda: runtime)
    probe.main()
