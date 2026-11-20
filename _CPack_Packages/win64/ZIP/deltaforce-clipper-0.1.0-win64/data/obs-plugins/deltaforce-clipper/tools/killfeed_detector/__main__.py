"""CLI entry point: run the killfeed detector loop.

Usage::

    python -m killfeed_detector                 # run with defaults
    python -m killfeed_detector --once          # process one frame and exit
    python -m killfeed_detector --roi 1600 0 320 300 --nick Absolution
    python -m killfeed_detector --config path.json
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import sys
import time
from pathlib import Path

from . import config as cfg
from .engine import DetectorEngine
from .ocr import WinOcrEngine


def load_config(file: str | None) -> dict:
    merged = dict(cfg.CONFIG)
    for source in (Path(__file__).parent / "config.json", Path(file) if file else None):
        if source and source.exists():
            try:
                merged.update(json.loads(source.read_text(encoding="utf-8")))
            except (OSError, json.JSONDecodeError) as exc:
                logging.getLogger("killfeed_detector").warning("bad config %s: %s", source, exc)
    return merged


def build_engine(config: dict) -> DetectorEngine:
    try:
        ocr = WinOcrEngine(config.get("ocr_language", "en"), config.get("upscale", 1))
    except Exception as exc:
        raise RuntimeError(f"winocr unavailable: {exc}") from exc
    return DetectorEngine(config, ocr)


def run(engine: DetectorEngine, once: bool = False) -> None:
    from .capture import capture_roi

    interval = float(engine.config.get("capture_interval", 0.25))
    diagnostic_interval = max(1.0, float(engine.config.get("ocr_log_interval_seconds", 10)))
    log_ocr_text = bool(engine.config.get("log_ocr_text", False))
    next_diagnostic = 0.0
    log = logging.getLogger("killfeed_detector")
    rois = engine.config.get("rois") or [engine.roi]
    log.info("detector started (regions=%s)", rois)
    while True:
        texts = []
        captured_regions = 0
        for roi in rois:
            image = capture_roi(roi)
            if image is not None:
                captured_regions += 1
                if engine.ocr:
                    texts.append(engine.ocr.recognize(image))
        now = time.monotonic()
        if log_ocr_text and now >= next_diagnostic:
            sample = " | ".join(text.strip() for text in texts if text.strip())
            log.info(
                "OCR diagnostic: captured %d/%d regions; text=%s",
                captured_regions,
                len(rois),
                sample[:400] if sample else "<empty>",
            )
            next_diagnostic = now + diagnostic_interval
        record = engine.process_text("\n".join(texts))
        if record:
            log.info("event: %s", record["event"])
        if once:
            return
        time.sleep(interval)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Delta Force killfeed detector (Windows OCR)")
    parser.add_argument("--config", help="path to config.json")
    parser.add_argument("--roi", nargs=4, type=int, metavar=("L", "T", "W", "H"), help="ROI left top width height")
    parser.add_argument("--nick", help="player label attached to event records")
    parser.add_argument("--events-file", help="path to events.jsonl bridge file")
    parser.add_argument("--once", action="store_true", help="capture one frame and exit")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)

    config = load_config(args.config)
    if args.roi:
        config["roi"] = {"left": args.roi[0], "top": args.roi[1], "width": args.roi[2], "height": args.roi[3]}
        config["rois"] = [config["roi"]]
    if args.nick:
        config["player_nick"] = args.nick
    if args.events_file:
        config["events_file"] = args.events_file

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    try:
        engine = build_engine(config)
        run(engine, once=args.once)
    except KeyboardInterrupt:
        pass
    except RuntimeError as exc:
        logging.getLogger("killfeed_detector").error("%s", exc)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
