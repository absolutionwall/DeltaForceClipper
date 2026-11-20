"""Delta Force killfeed detector (Windows OCR / winocr)."""

from .engine import DetectorEngine
from .events import KillEvent, parse_trigger_message
from .ocr import NullOcrEngine, WinOcrEngine

__all__ = ["DetectorEngine", "KillEvent", "NullOcrEngine", "WinOcrEngine", "parse_trigger_message"]
