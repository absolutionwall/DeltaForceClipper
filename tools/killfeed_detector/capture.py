"""Screen capture of the killfeed ROI.

Uses ``mss`` (fast, per-monitor, no window focus needed) with a fallback that
returns ``None`` when the platform is not Windows or no monitor is available,
so the detector degrades gracefully instead of crashing.
"""

from __future__ import annotations

import logging
from typing import Optional

log = logging.getLogger("killfeed_detector.capture")
_capture_error_logged = False
_pillow_error_logged = False

try:
    from PIL import Image  # type: ignore
except Exception:  # pragma: no cover - Pillow is a hard dep but keep import safe
    Image = None  # type: ignore


def capture_roi(roi: dict) -> Optional["Image.Image"]:
    """Grab a resolution-relative ROI as a PIL image.

    Coordinates are fractions of the primary screen. Legacy absolute
    {left, top, width, height} rectangles are also accepted.
    """
    if Image is None:
        global _pillow_error_logged
        if not _pillow_error_logged:
            log.error("Pillow is unavailable; screen capture is disabled.")
            _pillow_error_logged = True
        return None
    try:
        import mss

        with mss.mss() as sct:
            screen = sct.monitors[1]
            if all(key in roi for key in ("x", "y", "width", "height")):
                left = screen["left"] + round(float(roi["x"]) * screen["width"])
                top = screen["top"] + round(float(roi["y"]) * screen["height"])
                width = round(float(roi["width"]) * screen["width"])
                height = round(float(roi["height"]) * screen["height"])
            else:
                left = int(roi.get("left", 0))
                top = int(roi.get("top", 0))
                width = int(roi.get("width", 0))
                height = int(roi.get("height", 0))
            if width <= 0 or height <= 0:
                return None
            monitor = {"left": left, "top": top, "width": width, "height": height}
            shot = sct.grab(monitor)
            return Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")
    except Exception as exc:
        global _capture_error_logged
        if not _capture_error_logged:
            log.warning("Screen capture failed: %s", exc)
            _capture_error_logged = True
        return None
