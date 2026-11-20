"""Analyze a recorded video and write a media-time killfeed event timeline."""

from __future__ import annotations

import argparse
import json
import logging
import math
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from PIL import Image

from .__main__ import build_engine, load_config
from .events import KillEvent, parse_trigger_message

log = logging.getLogger("killfeed_detector.vod")
OUTPUT_WIDTH = 1920
OUTPUT_HEIGHT = 1080


def _executable(name: str, explicit_path: str | None = None) -> str:
    executable = explicit_path or shutil.which(name)
    if not executable:
        raise RuntimeError(f"{name}.exe was not found on PATH.")
    return executable


def _probe_dimensions(video_path: Path, ffprobe_path: str) -> tuple[int, int]:
    try:
        result = subprocess.run(
            [
                ffprobe_path,
                "-v", "error",
                "-select_streams", "v:0",
                "-show_entries", "stream=width,height",
                "-of", "json",
                str(video_path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f"Unable to inspect video with ffprobe: {exc}") from exc
    if result.returncode:
        raise RuntimeError(f"ffprobe failed: {result.stderr.strip() or 'unknown error'}")
    try:
        stream = json.loads(result.stdout)["streams"][0]
        width, height = int(stream["width"]), int(stream["height"])
    except (IndexError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        raise RuntimeError("ffprobe did not report a valid video stream size.") from exc
    if width <= 0 or height <= 0:
        raise RuntimeError("Video dimensions must be positive.")
    return width, height


def _output_geometry(source_width: int, source_height: int) -> tuple[int, int, int, int]:
    scale = min(1.0, OUTPUT_WIDTH / source_width, OUTPUT_HEIGHT / source_height)
    scaled_width = max(1, round(source_width * scale))
    scaled_height = max(1, round(source_height * scale))
    left = (OUTPUT_WIDTH - scaled_width) // 2
    top = (OUTPUT_HEIGHT - scaled_height) // 2
    return scaled_width, scaled_height, left, top


def _read_frame(stream, frame_size: int) -> bytes | None:
    chunks = bytearray()
    while len(chunks) < frame_size:
        chunk = stream.read(frame_size - len(chunks))
        if not chunk:
            if not chunks:
                return None
            raise RuntimeError("FFmpeg ended with a partial video frame.")
        chunks.extend(chunk)
    return bytes(chunks)


def _crop_region(image: Image.Image, region: dict, content: tuple[int, int, int, int]) -> Image.Image:
    try:
        x, y = float(region["x"]), float(region["y"])
        width, height = float(region["width"]), float(region["height"])
    except (KeyError, TypeError, ValueError) as exc:
        raise ValueError("Each VOD OCR region must define numeric x, y, width, and height values.") from exc
    if not all(math.isfinite(value) for value in (x, y, width, height)):
        raise ValueError("OCR regions must contain finite values.")
    if x < 0 or y < 0 or width <= 0 or height <= 0 or x + width > 1 or y + height > 1:
        raise ValueError("VOD analysis requires normalized OCR regions within the video frame.")
    left, top, content_width, content_height = content
    x1 = left + round(x * content_width)
    y1 = top + round(y * content_height)
    x2 = left + round((x + width) * content_width)
    y2 = top + round((y + height) * content_height)
    return image.crop((x1, y1, x2, y2))


class _PopupDeduplicator:
    def __init__(self, release_gap: float):
        self.release_gap = max(0.75, release_gap)
        self.active_key: tuple[str, str] | None = None
        self.last_seen = float("-inf")

    def observe(self, event: KillEvent | None, media_time: float) -> bool:
        if event is None:
            if media_time - self.last_seen > self.release_gap:
                self.active_key = None
            return False
        key = (event.event_type, event.tag)
        is_new = key != self.active_key or media_time - self.last_seen > self.release_gap
        self.active_key = key
        self.last_seen = media_time
        return is_new


def _timecode(seconds: float) -> str:
    milliseconds = round(seconds * 1000)
    hours, remainder = divmod(milliseconds, 3_600_000)
    minutes, remainder = divmod(remainder, 60_000)
    whole_seconds, milliseconds = divmod(remainder, 1000)
    return f"{hours:02}:{minutes:02}:{whole_seconds:02}.{milliseconds:03}"


def analyze_video(
    video_path: Path,
    config: dict,
    ocr,
    ffmpeg_path: str | None = None,
    ffprobe_path: str | None = None,
) -> list[dict]:
    video_path = video_path.resolve()
    if not video_path.is_file():
        raise FileNotFoundError(f"Video file does not exist: {video_path}")
    rois = config.get("rois")
    if not isinstance(rois, list) or not rois:
        raise ValueError("No OCR regions are configured.")
    try:
        interval = float(config.get("capture_interval", 0.25))
    except (TypeError, ValueError) as exc:
        raise ValueError("capture_interval must be a positive number.") from exc
    fps = 1.0 / interval if interval > 0 else 0
    if not math.isfinite(fps) or fps <= 0 or fps > 30:
        raise ValueError("capture_interval must produce a sampling rate between 0 and 30 fps.")

    ffmpeg = _executable("ffmpeg", ffmpeg_path)
    ffprobe = _executable("ffprobe", ffprobe_path)
    source_width, source_height = _probe_dimensions(video_path, ffprobe)
    scaled_width, scaled_height, left, top = _output_geometry(source_width, source_height)
    content = (left, top, scaled_width, scaled_height)
    filter_graph = (
        f"fps={fps:.6f},scale={scaled_width}:{scaled_height},"
        f"pad={OUTPUT_WIDTH}:{OUTPUT_HEIGHT}:{left}:{top}:black"
    )
    command = [
        ffmpeg,
        "-hide_banner", "-loglevel", "error", "-nostdin",
        "-i", str(video_path),
        "-vf", filter_graph,
        "-an", "-sn", "-dn",
        "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1",
    ]
    stderr_file = tempfile.TemporaryFile()
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=stderr_file)
    except OSError as exc:
        stderr_file.close()
        raise RuntimeError(f"Unable to start ffmpeg: {exc}") from exc
    assert process.stdout is not None

    try:
        cooldown = max(0.0, float(config.get("cooldown_seconds", 0.5)))
    except (TypeError, ValueError) as exc:
        raise ValueError("cooldown_seconds must be a non-negative number.") from exc
    deduplicator = _PopupDeduplicator(cooldown)
    events: list[dict] = []
    frame_size = OUTPUT_WIDTH * OUTPUT_HEIGHT * 3
    frame_index = 0
    last_progress = time.monotonic()
    try:
        while True:
            frame_data = _read_frame(process.stdout, frame_size)
            if frame_data is None:
                break
            media_time = frame_index / fps
            image = Image.frombytes("RGB", (OUTPUT_WIDTH, OUTPUT_HEIGHT), frame_data)
            text = "\n".join(
                ocr.recognize(_crop_region(image, region, content))
                for region in rois
            )
            event = parse_trigger_message(
                text,
                str(config.get("player_nick", "")),
                config.get("trigger_messages"),
            )
            if deduplicator.observe(event, media_time) and event is not None:
                record = {
                    "record_type": "vod_detection",
                    "video_file": str(video_path),
                    "ts": round(media_time, 3),
                    "timecode": _timecode(media_time),
                    "event": event.event_type,
                    "player": event.player,
                    "victim": event.victim,
                    "count": event.count,
                    "raw": event.raw,
                }
                if event.tag:
                    record["tag"] = event.tag
                events.append(record)
            frame_index += 1
            now = time.monotonic()
            if now - last_progress >= 10:
                log.info("Analyzed %d sampled frames (%.1f media seconds); found %d events",
                         frame_index, media_time, len(events))
                last_progress = now
    except Exception:
        process.kill()
        process.wait()
        stderr_file.close()
        raise
    finally:
        process.stdout.close()

    stderr_file.seek(0)
    stderr = stderr_file.read().decode("utf-8", errors="replace").strip()
    stderr_file.close()
    return_code = process.wait()
    if return_code:
        raise RuntimeError(f"ffmpeg failed: {stderr or f'exit code {return_code}'}")
    if frame_index == 0:
        raise RuntimeError("FFmpeg decoded no video frames.")
    return events


def write_events(output_path: Path, records: list[dict], overwrite: bool = False) -> None:
    output_path = output_path.resolve()
    if output_path.exists() and not overwrite:
        raise FileExistsError(f"Output already exists; use --overwrite to replace it: {output_path}")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n",
            dir=output_path.parent, prefix=f".{output_path.name}.", suffix=".tmp", delete=False,
        ) as output:
            temp_path = Path(output.name)
            for record in records:
                output.write(json.dumps(record, ensure_ascii=False) + "\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temp_path, output_path)
    finally:
        if temp_path and temp_path.exists():
            temp_path.unlink()


def extract_clips(
    video_path: Path,
    events_path: Path,
    output_dir: Path,
    pre_seconds: float = 3.0,
    post_seconds: float = 3.0,
    ffmpeg_path: str | None = None,
    overwrite: bool = False,
) -> list[Path]:
    """Extract clips from a video based on VOD events.

    Args:
        video_path: Path to the source video file.
        events_path: Path to the events.jsonl file from analyze_video.
        output_dir: Directory to write extracted clips.
        pre_seconds: Seconds before each event to include.
        post_seconds: Seconds after each event to include.
        ffmpeg_path: Optional explicit ffmpeg path.
        overwrite: Whether to overwrite existing clip files.

    Returns:
        List of paths to extracted clip files.
    """
    video_path = video_path.resolve()
    events_path = events_path.resolve()
    output_dir = output_dir.resolve()

    if not video_path.is_file():
        raise FileNotFoundError(f"Video file does not exist: {video_path}")
    if not events_path.is_file():
        raise FileNotFoundError(f"Events file does not exist: {events_path}")

    ffmpeg = _executable("ffmpeg", ffmpeg_path)

    output_dir.mkdir(parents=True, exist_ok=True)

    events: list[dict] = []
    with events_path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                log.warning("Skipping invalid JSON line in events file: %s", line[:200])

    if not events:
        log.info("No events found in %s", events_path)
        return []

    extracted: list[Path] = []
    for idx, event in enumerate(events):
        media_time = event.get("ts")
        if media_time is None:
            continue
        event_type = event.get("event", "clip")
        tag = event.get("tag", "")
        count = event.get("count", 0)

        start = max(0.0, media_time - pre_seconds)
        duration = pre_seconds + post_seconds
        end = media_time + post_seconds

        tag_suffix = f"_{tag}" if tag else ""
        clip_name = f"{video_path.stem}_{event_type}_c{count}{tag_suffix}_{idx:03d}.mp4"
        clip_path = output_dir / clip_name

        if clip_path.exists() and not overwrite:
            log.info("Clip already exists, skipping: %s", clip_path)
            extracted.append(clip_path)
            continue

        arguments = [
            ffmpeg,
            "-hide_banner", "-loglevel", "error", "-nostdin",
            "-ss", f"{start:.3f}",
            "-i", str(video_path),
            "-t", f"{duration:.3f}",
            "-map", "0:v?", "-map", "0:a?",
            "-c", "copy",
            "-avoid_negative_ts", "make_zero",
        ]
        if overwrite:
            arguments.append("-y")
        arguments.append(str(clip_path))

        log.info("Extracting clip %d/%d: %s (%.1fs-%.1fs)", idx + 1, len(events), clip_name, start, end)
        result = subprocess.run(arguments, capture_output=True, text=True, timeout=300)
        if result.returncode != 0:
            log.error("FFmpeg failed for clip %s: %s", clip_name, result.stderr.strip())
            continue

        if not clip_path.is_file() or clip_path.stat().st_size == 0:
            log.error("Clip extraction produced empty file: %s", clip_path)
            continue

        extracted.append(clip_path)
        log.info("Extracted: %s (%.2f MB)", clip_path, clip_path.stat().st_size / 1_048_576)

    return extracted


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Analyze a recorded video for configured killfeed events.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # analyze subcommand
    analyze_parser = subparsers.add_parser("analyze", help="Analyze video and write events.jsonl")
    analyze_parser.add_argument("video", type=Path, help="recorded video to analyze")
    analyze_parser.add_argument("--config", help="detector config JSON; defaults to bundled settings")
    analyze_parser.add_argument("--output", type=Path, help="output JSONL path (default: <video>.events.jsonl)")
    analyze_parser.add_argument("--ffmpeg", help="ffmpeg executable path (default: search PATH)")
    analyze_parser.add_argument("--ffprobe", help="ffprobe executable path (default: search PATH)")
    analyze_parser.add_argument("--overwrite", action="store_true", help="replace an existing output file")

    # extract subcommand
    extract_parser = subparsers.add_parser("extract", help="Extract clips from events.jsonl")
    extract_parser.add_argument("video", type=Path, help="source video file")
    extract_parser.add_argument("events", type=Path, help="events.jsonl from analyze subcommand")
    extract_parser.add_argument("--output", "-o", type=Path, required=True, help="output directory for clips")
    extract_parser.add_argument("--pre", type=float, default=3.0, help="seconds before event (default: 3.0)")
    extract_parser.add_argument("--post", type=float, default=3.0, help="seconds after event (default: 3.0)")
    extract_parser.add_argument("--ffmpeg", help="ffmpeg executable path (default: search PATH)")
    extract_parser.add_argument("--overwrite", action="store_true", help="overwrite existing clips")

    args = parser.parse_args(argv)

    if args.command == "analyze":
        logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

        video_path = args.video.resolve()
        output_path = args.output or Path(f"{video_path}.events.jsonl")
        if output_path.resolve() == video_path:
            analyze_parser.error("output path must not overwrite the input video")
        if output_path.exists() and not args.overwrite:
            log.error("Output already exists; use --overwrite to replace it: %s", output_path)
            return 1
        try:
            config = load_config(args.config)
            engine = build_engine({**config, "events_file": ""})
            records = analyze_video(video_path, config, engine.ocr, args.ffmpeg, args.ffprobe)
            write_events(output_path, records, args.overwrite)
        except (FileNotFoundError, FileExistsError, OSError, RuntimeError, ValueError) as exc:
            log.error("%s", exc)
            return 1
        print(f"Analyzed {video_path}; found {len(records)} events in {output_path.resolve()}")
        return 0

    if args.command == "extract":
        logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

        try:
            clips = extract_clips(
                args.video,
                args.events,
                args.output,
                pre_seconds=args.pre,
                post_seconds=args.post,
                ffmpeg_path=args.ffmpeg,
                overwrite=args.overwrite,
            )
        except (FileNotFoundError, OSError, RuntimeError, ValueError) as exc:
            log.error("%s", exc)
            return 1

        if not clips:
            print("No clips extracted.")
            return 0

        print(f"Extracted {len(clips)} clips to {args.output.resolve()}")
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
