"""ROI calibration helper: capture the current ROI or the full screen.

Usage::

    python -m killfeed_detector.roi_tool --shot roi.png
    python -m killfeed_detector.roi_tool --shot full.png --full
    python -m killfeed_detector.roi_tool --probe

The killfeed sits at the top-right of the screen during Warfare/Operations.
Capture a frame, look at the image, and set ``roi`` in config.json to the
rectangle that tightly bounds the feed.
"""

from __future__ import annotations

import argparse

from . import config as cfg
from .capture import capture_roi
from .ocr import NullOcrEngine, WinOcrEngine


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Killfeed ROI calibration tool")
    parser.add_argument("--shot", help="save a capture to this PNG file")
    parser.add_argument("--full", action="store_true", help="capture the full primary screen instead of the ROI")
    parser.add_argument("--probe", action="store_true", help="OCR the ROI once and print the text")
    args = parser.parse_args(argv)

    rois = cfg.CONFIG.get("rois", [])
    roi = rois[0] if rois else {}
    if args.full:
        roi = {"left": 0, "top": 0, "width": 0, "height": 0}  # mss uses monitor 1 when unset
        # capture_roi rejects zero-sized ROIs, so grab the primary monitor directly.
        import mss

        with mss.mss() as sct:
            shot = sct.grab(sct.monitors[1])
        from PIL import Image

        img = Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")
    else:
        img = capture_roi(roi)

    if img is None:
        print("capture failed: is the ROI set and non-zero?")
        return 1

    if args.shot:
        img.save(args.shot)
        print(f"saved {args.shot} ({img.width}x{img.height})")

    if args.probe:
        ocr = WinOcrEngine(cfg.CONFIG.get("ocr_language", "en"), cfg.CONFIG.get("upscale", 1))
        print("OCR text:", repr(ocr.recognize(img)))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
