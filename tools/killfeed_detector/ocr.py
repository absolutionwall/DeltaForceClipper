"""Windows OCR (winocr) wrapper with graceful degradation.

The detector runs against Windows.Media.Ocr via the ``winocr`` package. All
OCR access goes through this module so tests can substitute a fake recognizer.
"""

from __future__ import annotations

from PIL import Image
from typing import Protocol, runtime_checkable


@runtime_checkable
class OcrEngine(Protocol):
    """Minimal shape the detector needs from an OCR engine."""

    def recognize(self, image) -> str:
        """Return the recognized text of a PIL image."""
        ...


class WinOcrEngine:
    """OCR engine backed by winocr / Windows.Media.Ocr.

    ``recognize`` never raises: recognition errors degrade to empty text so
    one bad frame cannot take the detector down.
    """

    def __init__(self, language: str = "en", upscale: int = 1):
        import winocr  # imported lazily: only needed on Windows at runtime

        self._winocr = winocr
        self.language = language
        self.upscale = max(1, int(upscale))
        self._logged_failure = False

    def recognize(self, image) -> str:
        try:
            if self.upscale > 1:
                image = image.resize(
                    (image.width * self.upscale, image.height * self.upscale),
                    Image.Resampling.LANCZOS,
                )
            result = self._winocr.recognize_pil_sync(image, self.language)
            return str(result.get("text", "") or "")
        except Exception as exc:  # pragma: no cover - defensive
            import logging

            if not self._logged_failure:
                logging.getLogger("killfeed_detector").warning("Windows OCR failed: %s", exc)
                self._logged_failure = True
            return ""


class NullOcrEngine:
    """Engine that always returns empty text (used when OCR is unavailable)."""

    def recognize(self, image) -> str:
        return ""
