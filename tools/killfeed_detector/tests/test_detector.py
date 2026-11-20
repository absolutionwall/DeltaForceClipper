"""Detector unit tests (no OCR engine or screen required)."""

import json
import tempfile
import time
from types import SimpleNamespace
from pathlib import Path
from unittest.mock import patch

import unittest
from PIL import Image

from killfeed_detector import config as cfg
from killfeed_detector.__main__ import load_config
from killfeed_detector.engine import DetectorEngine
from killfeed_detector.events import DEFAULT_TRIGGER_MESSAGES, normalize, parse_trigger_message
from killfeed_detector.ocr import WinOcrEngine


class FakeOcr:
    def __init__(self, text=""):
        self.text = text
        self.calls = 0

    def recognize(self, image):
        self.calls += 1
        return self.text


class FakeImage:
    """PIL-like image the fake engine never inspects."""


class ConfigTests(unittest.TestCase):
    def test_custom_ocr_regions_load_from_detector_config(self):
        regions = [{"x": 0.1, "y": 0.2, "width": 0.3, "height": 0.4}]
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "detector.json"
            config_path.write_text(json.dumps({"rois": regions}), encoding="utf-8")
            loaded = load_config(str(config_path))
        self.assertEqual(loaded["rois"], regions)


class OcrWrapperTests(unittest.TestCase):
    def test_upscale_uses_pillow_and_calls_windows_ocr(self):
        seen = {}

        def recognize(image, language):
            seen["size"] = image.size
            seen["language"] = language
            return {"text": "Double Kill"}

        with patch.dict("sys.modules", {"winocr": SimpleNamespace(recognize_pil_sync=recognize)}):
            engine = WinOcrEngine("en", upscale=2)
            result = engine.recognize(Image.new("RGB", (10, 5)))

        self.assertEqual(result, "Double Kill")
        self.assertEqual(seen, {"size": (20, 10), "language": "en"})


class NormalizeTests(unittest.TestCase):
    def test_normalizes_ocr_noise(self):
        self.assertEqual(normalize("Absolution  eliminated__ Rival-7 !! x3"), "absolution eliminated rival 7 x3")

    def test_empty(self):
        self.assertEqual(normalize(""), "")
        self.assertEqual(normalize(None), "")


class ParseTests(unittest.TestCase):
    def test_default_triggers_match_requested_phrases_and_event_types(self):
        expected = [
            {"phrase": "double kill", "event": "double_kill"},
            {"phrase": "triple kill", "event": "triple_kill"},
            {"phrase": "multikill", "event": "multikill"},
            {"phrase": "quadra kill", "event": "quadra_kill"},
            {"phrase": "stinger kill", "event": "kill", "tag": "launcher"},
            {"phrase": "penta kill", "event": "penta_kill"},
            {"phrase": "guided missile kill", "event": "kill", "tag": "missile"},
        ]
        self.assertEqual(DEFAULT_TRIGGER_MESSAGES, expected)
        for trigger in expected:
            with self.subTest(phrase=trigger["phrase"]):
                event = parse_trigger_message(trigger["phrase"], "Absolution")
                self.assertIsNotNone(event)
                self.assertEqual(event.event_type, trigger["event"])
                if trigger["event"] == "quadra_kill":
                    self.assertEqual(event.count, 4)
                elif trigger["event"] == "penta_kill":
                    self.assertEqual(event.count, 5)

    def test_unconfigured_killfeed_rows_are_ignored(self):
        self.assertIsNone(parse_trigger_message("Absolution eliminated Rival_7", "Absolution"))
        self.assertIsNone(parse_trigger_message("Absolution Usul2705", "Absolution"))

    def test_empty_text(self):
        self.assertIsNone(parse_trigger_message("", "Absolution"))

    def test_multikill_banner_from_sample_ocr(self):
        ev = parse_trigger_message("1498 Hit +80EXP Double Kill +20 EXP Objective Defended", "Absolution")
        self.assertIsNotNone(ev)
        self.assertEqual(ev.event_type, "double_kill")

    def test_triple_kill_banner(self):
        ev = parse_trigger_message("Triple Kill +20 EXP", "Absolution")
        self.assertIsNotNone(ev)
        self.assertEqual(ev.event_type, "triple_kill")

    def test_triple_kill_banner_tolerates_one_ocr_character_error(self):
        ev = parse_trigger_message("Triple kilU +30 EXP", "Absolution")
        self.assertIsNotNone(ev)
        self.assertEqual(ev.event_type, "triple_kill")

    def test_fuzzy_match_does_not_match_unrelated_killfeed_text(self):
        ev = parse_trigger_message("Triple Killer eliminated Absolution", "Absolution")
        self.assertIsNone(ev)

    def test_custom_notification_phrase_and_clip_type(self):
        ev = parse_trigger_message(
            "Team Wipe +20 EXP",
            "Absolution",
            trigger_messages=[{"phrase": "team wipe", "event": "multikill"}],
        )
        self.assertIsNotNone(ev)
        self.assertEqual(ev.event_type, "multikill")

    def test_removed_notification_phrase_is_not_still_hardcoded(self):
        ev = parse_trigger_message("Double Kill +20 EXP", "Absolution", trigger_messages=[])
        self.assertIsNone(ev)

    def test_single_kill_requires_configured_phrase(self):
        ev = parse_trigger_message(
            "First Blood +20 EXP",
            "Absolution",
            trigger_messages=[{"phrase": "first blood", "event": "kill"}],
        )
        self.assertIsNotNone(ev)
        self.assertEqual(ev.event_type, "kill")

    def test_guided_missile_and_stinger_triggers_have_equipment_tags(self):
        for phrase, tag in (
            ("Guided Missile Kill", "missile"),
            ("Stinger Kill", "launcher"),
        ):
            with self.subTest(phrase=phrase):
                event = parse_trigger_message(phrase, "Absolution")
                self.assertIsNotNone(event)
                self.assertEqual(event.tag, tag)

    def test_custom_phrase_can_configure_a_tag(self):
        event = parse_trigger_message(
            "Skyfire",
            "Absolution",
            trigger_messages=[{"phrase": "skyfire", "event": "kill", "tag": "rocket"}],
        )
        self.assertIsNotNone(event)
        self.assertEqual(event.tag, "rocket")


class EngineTests(unittest.TestCase):
    def _engine(self, ocr, **overrides):
        config = dict(cfg.CONFIG)
        config.update(overrides)
        return DetectorEngine(config, ocr)

    def test_full_pipeline_writes_event(self):
        with tempfile.TemporaryDirectory() as tmp:
            events = str(Path(tmp) / "events.jsonl")
            engine = self._engine(FakeOcr("Double Kill +20 EXP"), events_file=events, player_nick="Absolution")
            record = engine.process_frame(FakeImage())
            self.assertIsNotNone(record)
            self.assertEqual(record["event"], "double_kill")
            written = json.loads(Path(events).read_text(encoding="utf-8"))
            self.assertEqual(written["event"], "double_kill")
            self.assertEqual(written["count"], 2)

    def test_quadra_and_penta_tiers_survive_event_bridge(self):
        for phrase, event_type, count in (
            ("Quadra Kill", "quadra_kill", 4),
            ("Penta Kill", "penta_kill", 5),
        ):
            with self.subTest(phrase=phrase), tempfile.TemporaryDirectory() as tmp:
                events = Path(tmp) / "events.jsonl"
                engine = self._engine(FakeOcr(phrase), events_file=str(events), player_nick="Absolution")
                record = engine.process_frame(FakeImage())
                persisted = json.loads(events.read_text(encoding="utf-8"))
                self.assertEqual(record["event"], event_type)
                self.assertEqual(record["count"], count)
                self.assertEqual(persisted["event"], event_type)
                self.assertEqual(persisted["count"], count)

    def test_equipment_tags_survive_event_bridge(self):
        for phrase, expected_tag in (
            ("Guided Missile Kill", "missile"),
            ("Stinger Kill", "launcher"),
        ):
            with self.subTest(phrase=phrase), tempfile.TemporaryDirectory() as tmp:
                events = Path(tmp) / "events.jsonl"
                engine = self._engine(FakeOcr(phrase), events_file=str(events), player_nick="Absolution")
                record = engine.process_frame(FakeImage())
                persisted = json.loads(events.read_text(encoding="utf-8"))
                self.assertEqual(record["tag"], expected_tag)
                self.assertEqual(persisted["tag"], expected_tag)

    def test_unconfigured_killfeed_row_does_not_write_event(self):
        with tempfile.TemporaryDirectory() as tmp:
            events = Path(tmp) / "events.jsonl"
            engine = self._engine(
                FakeOcr("Absolution eliminated Rival_7"),
                events_file=str(events),
                player_nick="Absolution",
            )
            self.assertIsNone(engine.process_frame(FakeImage()))
            self.assertFalse(events.exists())

    def test_custom_notification_mapping_writes_selected_clip_type(self):
        with tempfile.TemporaryDirectory() as tmp:
            events = str(Path(tmp) / "events.jsonl")
            engine = self._engine(
                FakeOcr("Squad Wipe +20 EXP"),
                events_file=events,
                trigger_messages=[{"phrase": "squad wipe", "event": "multikill", "tag": "team"}],
                cooldown_seconds=0,
            )
            record = engine.process_frame(FakeImage())
            self.assertEqual(record["event"], "multikill")
            self.assertEqual(record["tag"], "team")
            persisted = json.loads(Path(events).read_text(encoding="utf-8"))
            self.assertEqual(persisted["event"], "multikill")
            self.assertEqual(persisted["tag"], "team")

    def test_dedup_rejects_repeat(self):
        with tempfile.TemporaryDirectory() as tmp:
            events = str(Path(tmp) / "events.jsonl")
            engine = self._engine(FakeOcr("Double Kill +20 EXP"), events_file=events, cooldown_seconds=0)
            self.assertIsNotNone(engine.process_frame(FakeImage()))
            self.assertIsNone(engine.process_frame(FakeImage()))  # same text -> dedup
            self.assertEqual(len(Path(events).read_text().strip().splitlines()), 1)

    def test_cooldown_suppresses_different_event(self):
        with tempfile.TemporaryDirectory() as tmp:
            events = str(Path(tmp) / "events.jsonl")
            engine = self._engine(FakeOcr(""), events_file=events, cooldown_seconds=60)
            engine.ocr = FakeOcr("Double Kill")
            self.assertIsNotNone(engine.process_frame(FakeImage()))
            engine.ocr = FakeOcr("Triple Kill")
            self.assertIsNone(engine.process_frame(FakeImage()))  # within cooldown
            self.assertEqual(len(Path(events).read_text().strip().splitlines()), 1)

    def test_dedup_window_forgets_old(self):
        with tempfile.TemporaryDirectory() as tmp:
            events = str(Path(tmp) / "events.jsonl")
            engine = self._engine(FakeOcr(""), events_file=events, cooldown_seconds=0, recent_texts_max=2)
            for phrase in ("Double Kill", "Triple Kill", "Multikill"):
                engine.ocr = FakeOcr(phrase)
                engine.process_frame(FakeImage())
            engine.ocr = FakeOcr("Double Kill")  # This event fell out of the 2-entry window.
            self.assertIsNotNone(engine.process_frame(FakeImage()))

    def test_no_nick_match_no_event(self):
        with tempfile.TemporaryDirectory() as tmp:
            engine = self._engine(FakeOcr("Nobody eliminated Anybody"), events_file=str(Path(tmp) / "e.jsonl"))
            self.assertIsNone(engine.process_frame(FakeImage()))

    def test_none_frame_is_noop(self):
        engine = self._engine(FakeOcr("x"))
        self.assertIsNone(engine.process_frame(None))

    def test_clock_control(self):
        with tempfile.TemporaryDirectory() as tmp:
            ticks = iter([100.0, 101.0, 200.0])
            engine = self._engine(FakeOcr(""), events_file=str(Path(tmp) / "e.jsonl"), cooldown_seconds=10)
            engine.clock = lambda: next(ticks)
            engine.ocr = FakeOcr("Double Kill")
            self.assertIsNotNone(engine.process_frame(FakeImage()))
            engine.ocr = FakeOcr("Triple Kill")
            self.assertIsNone(engine.process_frame(FakeImage()))  # 1s later, cooldown 10
            engine.ocr = FakeOcr("Multikill")
            self.assertIsNotNone(engine.process_frame(FakeImage()))  # 99s later, allowed


if __name__ == "__main__":
    unittest.main()
