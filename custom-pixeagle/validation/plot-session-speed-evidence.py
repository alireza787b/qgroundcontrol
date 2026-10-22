#!/usr/bin/env python3
"""Export review plots and CSV from successful isolated SIH speed runs.

Requires matplotlib in the validation environment, not in QGC or PixEagle.
Reads recorded evidence only; never starts a simulator or sends commands.
"""

from __future__ import annotations

import argparse
import csv
import html
import json
import math
from pathlib import Path


def magnitude(fields):
    return math.sqrt(
        sum(
            fields.get(name, 0) ** 2 for name in ("vel_body_fwd", "vel_body_right", "vel_body_down")
        )
    )


def export(runtime: Path, output: Path) -> str:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    result = json.loads((runtime / "result.json").read_text())
    if result.get("passed") is not True or not result.get("session_speed_evidence"):
        raise ValueError(f"No successful session-speed evidence: {runtime}")
    rows = [json.loads(line) for line in (runtime / "observations.jsonl").read_text().splitlines()]
    publications = [
        json.loads(line) for line in (runtime / "publish.jsonl").read_text().splitlines()
    ]
    origin = rows[0]["timestamp"]
    samples = []
    for row in rows:
        control = next(
            (
                item
                for item in row.get("session_controls", [])
                if item["control_id"] == "requested_speed"
            ),
            {},
        )
        fields = row.get("intent") or {}
        samples.append(
            {
                "elapsed_s": row["timestamp"] - origin,
                "phase": row["phase"],
                "requested_3d_m_s": control.get("value"),
                "submitted_3d_m_s": magnitude(fields),
                "px4_measured_3d_m_s": row.get("px4_speed_m_s"),
                "submitted_forward_m_s": fields.get("vel_body_fwd", 0),
                "submitted_right_m_s": fields.get("vel_body_right", 0),
                "submitted_down_m_s": fields.get("vel_body_down", 0),
                "submitted_yaw_deg_s": fields.get("yawspeed_deg_s", 0),
                "px4_height_local_ned_m": -row["position_ned"][2],
                "px4_yaw_deg": math.degrees(row["px4_attitude"]["yaw"]),
                "continuity": row["continuity"].get("authority_state"),
                "flight_mode": row.get("flight_state", {}).get("flight_mode"),
                "target_generation": row["target_generation"],
            }
        )
    stem = runtime.name
    with (output / f"{stem}.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(samples[0]))
        writer.writeheader()
        writer.writerows(samples)
    times = [row["elapsed_s"] for row in samples]
    fig, axes = plt.subplots(6, 1, figsize=(14, 16), sharex=True, constrained_layout=True)
    fig.suptitle(
        f"Session speed — {result['mount']} mount\nIsolated PX4 SIH · synthetic target observations · software evidence"
    )
    axes[0].step(
        times,
        [row["requested_3d_m_s"] for row in samples],
        where="post",
        label="Requested 3D speed",
        linewidth=2,
    )
    axes[0].plot(
        times, [row["submitted_3d_m_s"] for row in samples], label="Submitted intent", alpha=0.7
    )
    axes[0].plot(
        times,
        [row["px4_measured_3d_m_s"] for row in samples],
        label="Independent PX4 velocity",
        linewidth=1.5,
    )
    actual = [
        row
        for row in publications
        if row.get("command_intent") and origin <= row["timestamp"] <= rows[-1]["timestamp"]
    ]
    axes[0].plot(
        [row["timestamp"] - origin for row in actual],
        [magnitude(row["command_intent"]["fields"]) for row in actual],
        linestyle="none",
        marker=".",
        markersize=2,
        label="Publication trace (intent)",
        alpha=0.5,
    )
    axes[0].set_ylabel("3D speed (m/s)")
    for name, label in [("forward", "Forward"), ("right", "Right"), ("down", "Down (+ descent)")]:
        axes[1].plot(times, [row[f"submitted_{name}_m_s"] for row in samples], label=label)
    axes[1].set_ylabel("Body velocity (m/s)")
    axes[2].plot(
        times, [row["px4_height_local_ned_m"] for row in samples], label="PX4 -local NED z"
    )
    axes[2].set_ylabel("SIH height (m)")
    axes[3].plot(times, [row["px4_yaw_deg"] for row in samples], label="PX4 yaw (deg)")
    axes[3].plot(
        times, [row["submitted_yaw_deg_s"] for row in samples], label="Submitted yaw rate (deg/s)"
    )
    axes[3].set_ylabel("Yaw / yaw rate")
    states = list(dict.fromkeys(row["continuity"] for row in samples))
    axes[4].step(
        times,
        [states.index(row["continuity"]) for row in samples],
        where="post",
        label="Continuity authority",
    )
    axes[4].set_yticks(range(len(states)), states)
    modes = list(dict.fromkeys(row["flight_mode"] for row in samples))
    axes[5].step(
        times,
        [modes.index(row["flight_mode"]) for row in samples],
        where="post",
        label="Observed PX4 mode",
    )
    mode_labels = {393216: "Offboard", 50593792: "Hold", 100925440: "Land", 84148224: "RTL"}
    axes[5].set_yticks(
        range(len(modes)),
        [mode_labels.get(mode, str(mode)) if mode is not None else "Unknown" for mode in modes],
    )
    axes[5].set_xlabel("Seconds since first guidance observation")
    phases = [
        (row["elapsed_s"], row["phase"])
        for i, row in enumerate(samples)
        if i == 0 or row["phase"] != samples[i - 1]["phase"]
    ]
    for axis in axes:
        axis.grid(alpha=0.2)
        axis.legend(loc="upper left", fontsize=8)
        for when, _ in phases:
            axis.axvline(when, color="grey", alpha=0.2, linewidth=0.7)
    for when, phase in phases:
        axes[0].text(
            when,
            1,
            phase.replace("_", " "),
            transform=axes[0].get_xaxis_transform(),
            rotation=35,
            fontsize=7,
            va="bottom",
        )
    fig.savefig(output / f"{stem}.png", dpi=160)
    fig.savefig(output / f"{stem}.svg")
    plt.close(fig)
    title = html.escape(f"{result['mount']} — {stem}")
    takeover = result.get("land_takeover_evidence") or {}
    preserved = (
        takeover.get("mode_before") == takeover.get("mode_after")
        and takeover.get("cleanup", {}).get("local_sender_quiesced") is True
    )
    return (
        f'<h2>{title}</h2><a href="{stem}.svg"><img src="{stem}.png" alt="{title}"></a>'
        f"<p>After the plotted guidance: confirmed Hold on budget expiry; delayed cleanup preserved Land: {preserved}.</p>"
        f'<p><a href="{stem}.csv">CSV samples</a></p>'
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtimes", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sections = [export(runtime, args.output) for runtime in args.runtimes]
    (args.output / "index.html").write_text(
        '<!doctype html><html lang="en"><meta charset="utf-8"><title>PixEagle session-speed evidence</title>'
        "<style>body{font:16px system-ui;max-width:1400px;margin:2rem auto;padding:1rem}img{width:100%}</style>"
        "<h1>Requested speed: SIH evidence</h1><p>Recorded software/simulated response only. "
        "Not a physical flight or camera-tracking qualification. Requested speed is not guaranteed aircraft speed.</p>"
        + "".join(sections)
        + "</html>",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
