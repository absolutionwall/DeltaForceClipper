"""Killfeed detection engine: capture -> OCR -> parse -> dedup -> event log.

The engine writes one JSON object per line to ``events_file`` (the bridge the
OBS plugin tails). Events are append-only; the plugin handles truncation.
"""

from __future__ import annotations

import json
import logging
import time
from pathlib import Path
from typing import Optional

from . import events as ev
from .capture import capture_roi
from .ocr import OcrEngine

log = logging.getLogger("killfeed_detector")


class DetectorEngine:
    """Runs the capture/OCR/parse loop and emits kill events."""

    def __init__(self, config: dict, ocr: Optional[OcrEngine] = None, clock=time.monotonic):
        self.config = config
        self.ocr = ocr
        self.clock = clock
        self.roi = config.get("roi", {})
        self.player_nick = config.get("player_nick", "")
        self.cooldown = float(config.get("cooldown_seconds", 4.0))
        self.trigger_messages = config.get("trigger_messages", ev.DEFAULT_TRIGGER_MESSAGES)
        self.recent_max = int(config.get("recent_texts_max", 8))
        self.events_file = _expand(config.get("events_file", ""))
        self._last_time = 0.0
        self._recent: list[str] = []
        self._events_path = Path(self.events_file) if self.events_file else None
        if self._events_path:
            self._events_path.parent.mkdir(parents=True, exist_ok=True)

    # -- core -------------------------------------------------------------

    def process_frame(self, image, now: Optional[float] = None) -> Optional[dict]:
        """OCR one frame, parse it, dedup, and (maybe) write an event."""
        if image is None:
            return None
        text = self.ocr.recognize(image) if self.ocr else ""
        return self.process_text(text, now)

    def process_text(self, text: str, now: Optional[float] = None) -> Optional[dict]:
        """Parse combined OCR text from multiple screen regions."""
        if not text:
            return None
        event = ev.parse_trigger_message(text, self.player_nick, self.trigger_messages)
        if event is None:
            return None
        if self._is_duplicate(event):
            log.debug("deduped event: %r", event.raw)
            return None
        now = self.clock() if now is None else now
        if now - self._last_time < self.cooldown:
            log.debug("cooldown suppressed event: %r", event.raw)
            return None
        self._last_time = now
        self._remember(event)
        record = self._to_record(event, now)
        self._append(record)
        return record

    def _is_duplicate(self, event: ev.KillEvent) -> bool:
        key = f"{event.player}|{event.victim}|{event.count}|{ev.normalize(event.raw)}"
        return key in self._recent

    def _remember(self, event: ev.KillEvent) -> None:
        key = f"{event.player}|{event.victim}|{event.count}|{ev.normalize(event.raw)}"
        self._recent.append(key)
        if len(self._recent) > self.recent_max:
            self._recent.pop(0)

    def _to_record(self, event: ev.KillEvent, now: float) -> dict:
        record = {
            "ts": now,
            "event": event.event_type,
            "player": event.player,
            "victim": event.victim,
            "count": event.count,
            "raw": event.raw,
        }
        if event.tag:
            record["tag"] = event.tag
        return record

    def _append(self, record: dict) -> None:
        if not self._events_path:
            return
        line = json.dumps(record, ensure_ascii=False) + "\n"
        try:
            with open(self._events_path, "a", encoding="utf-8") as fh:
                fh.write(line)
                fh.flush()
        except OSError as exc:
            log.warning("could not write event: %s", exc)


def _expand(path: str) -> str:
    return str(Path(path).expanduser()) if path else ""
