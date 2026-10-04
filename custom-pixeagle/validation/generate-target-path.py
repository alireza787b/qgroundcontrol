#!/usr/bin/env python3
"""Generate a clock-paced SIH target fixture and displayed-frame ground truth."""

from __future__ import annotations

import argparse
import hashlib
import json
from itertools import pairwise
from pathlib import Path
from tempfile import TemporaryDirectory

from PIL import Image, ImageDraw
from PIL import __version__ as pillow_version

WIDTH = 640
HEIGHT = 480
TARGET_SIZE = 120
DURATION_SECONDS = 30
FIXTURE_ID = "pixeagle-sih-target-path-v1"
WAYPOINTS = (
    (0, 0.5, 0.5),
    (3, 0.5, 0.5),
    (7, 0.7, 0.5),
    (9, 0.7, 0.5),
    (13, 0.3, 0.5),
    (15, 0.3, 0.5),
    (19, 0.5, 0.3),
    (21, 0.5, 0.3),
    (25, 0.5, 0.7),
    (27, 0.5, 0.7),
    (30, 0.5, 0.5),
)


def checksum(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def position(seconds: float) -> tuple[float, float]:
    for start, end in pairwise(WAYPOINTS):
        if seconds < end[0]:
            progress = (seconds - start[0]) / (end[0] - start[0])
            smooth = progress * progress * (3 - 2 * progress)
            return (
                start[1] + (end[1] - start[1]) * smooth,
                start[2] + (end[2] - start[2]) * smooth,
            )
    return WAYPOINTS[-1][1], WAYPOINTS[-1][2]


def marker() -> Image.Image:
    image = Image.new("RGB", (TARGET_SIZE, TARGET_SIZE), "#dddddd")
    draw = ImageDraw.Draw(image)
    draw.rectangle((10, 10, 109, 109), fill="black")
    draw.rectangle((20, 20, 99, 99), fill="#efaa00")
    draw.line((30, 30, 90, 90), fill="white", width=8)
    draw.line((30, 90, 90, 30), fill="white", width=8)
    draw.ellipse((48, 48, 72, 72), fill="red")
    return image


def generate(output: Path, fps: int) -> Path:
    identity = {
        "fixture_id": FIXTURE_ID,
        "generator_sha256": checksum(Path(__file__)),
        "pillow_version": pillow_version,
        "width": WIDTH,
        "height": HEIGHT,
        "fps": fps,
        "duration_seconds": DURATION_SECONDS,
        "frame_count": fps * DURATION_SECONDS,
    }
    manifest_path = output / "ground-truth.json"
    if output.exists():
        if not manifest_path.is_file():
            raise SystemExit(
                "Output exists without a fixture manifest. Choose a new output directory."
            )
        previous = json.loads(manifest_path.read_text())
        if any(previous.get(key) != value for key, value in identity.items()):
            raise SystemExit("Fixture version or settings changed. Choose a new output directory.")
        for frame in previous["frames"]:
            path = output / frame["file"]
            if not path.is_file() or checksum(path) != frame["sha256"]:
                raise SystemExit("Fixture integrity check failed. Choose a new output directory.")
        if len(previous["frames"]) != identity["frame_count"]:
            raise SystemExit("Fixture frame count is invalid. Choose a new output directory.")
        return manifest_path

    output.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    with TemporaryDirectory(prefix=f".{output.name}-", dir=output.parent) as temporary:
        staging = Path(temporary)
        write_fixture(staging, identity, fps)
        staging.rename(output)
    return manifest_path


def write_fixture(output: Path, identity: dict, fps: int) -> None:
    target = marker()
    frames = []
    for frame_index in range(identity["frame_count"]):
        seconds = frame_index / fps
        center_x, center_y = position(seconds)
        left = round(center_x * WIDTH - TARGET_SIZE / 2)
        top = round(center_y * HEIGHT - TARGET_SIZE / 2)
        image = Image.new("RGB", (WIDTH, HEIGHT), "#352b24")
        image.paste(target, (left, top))
        draw = ImageDraw.Draw(image)
        draw.rectangle((0, HEIGHT - 32, WIDTH, HEIGHT), fill="#171717")
        draw.text(
            (WIDTH / 2, HEIGHT - 16),
            f"SIH TARGET PATH | FRAME {frame_index:04d} | {seconds:05.2f}s",
            fill="white",
            font_size=16,
            anchor="mm",
        )
        filename = f"frame-{frame_index:05d}.png"
        path = output / filename
        image.save(path, format="PNG", compress_level=6)
        frames.append(
            {
                "frame_index": frame_index,
                "timestamp_seconds": round(seconds, 9),
                "timestamp_numerator": frame_index,
                "timestamp_denominator": fps,
                "file": filename,
                "bbox_pixels": [left, top, TARGET_SIZE, TARGET_SIZE],
                "bbox_normalized": [
                    left / WIDTH,
                    top / HEIGHT,
                    TARGET_SIZE / WIDTH,
                    TARGET_SIZE / HEIGHT,
                ],
                "center_normalized": [
                    (left + TARGET_SIZE / 2) / WIDTH,
                    (top + TARGET_SIZE / 2) / HEIGHT,
                ],
                "sha256": checksum(path),
            }
        )
    manifest = {
        **identity,
        "bbox_format": "left, top, width, height; half-open bounds in generated source-frame pixels",
        "coordinate_reference": (
            "Normalized coordinates cover the entire generated frame, including the timecode area. "
            "For resized backend output, compare normalized boxes or scale pixels using reported frame dimensions."
        ),
        "waypoints": [
            {"seconds": point[0], "center_normalized": list(point[1:])} for point in WAYPOINTS
        ],
        "interpolation": "smoothstep between waypoints; pixel positions rounded to nearest integer",
        "loop": "Target position and velocity join continuously; frame counter resets each loop.",
        "scope": "Synthetic SIH command-path and Classic tracker fixture, not a local AI detection scene.",
        "limitations": (
            "Image motion is scripted and independent of aircraft motion. This is not a closed-loop "
            "vehicle/world simulation or evidence of autonomous follower flight acceptance. Match the "
            "visible frame index to ground truth; elapsed wall time may differ from displayed frame time."
        ),
        "frames": frames,
    }
    (output / "ground-truth.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        required=True,
        type=Path,
        help="New fixture directory, or unchanged generated fixture",
    )
    parser.add_argument("--fps", type=int, default=30, help="Frames per second (1-60; default 30)")
    arguments = parser.parse_args()
    if not 1 <= arguments.fps <= 60:
        parser.error("--fps must be between 1 and 60")
    print(generate(arguments.output.resolve(), arguments.fps))


if __name__ == "__main__":
    main()
