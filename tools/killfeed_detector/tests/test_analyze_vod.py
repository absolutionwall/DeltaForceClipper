import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch, MagicMock

from PIL import Image

from killfeed_detector import analyze_vod
from killfeed_detector.events import KillEvent


class FakeOcr:
    def recognize(self, image):
        return "Double Kill +20 EXP"


class FakeProcess:
    def __init__(self, stdout, stderr=b"", return_code=0):
        self.stdout = io.BytesIO(stdout)
        self.stderr = io.BytesIO(stderr)
        self.return_code = return_code

    def wait(self):
        return self.return_code

    def kill(self):
        self.return_code = -9


class VodAnalyzerTests(unittest.TestCase):
    def test_output_geometry_preserves_aspect_ratio_and_centers_image(self):
        self.assertEqual(analyze_vod._output_geometry(2560, 1080), (1920, 810, 0, 135))

    def test_crop_region_maps_normalized_coordinates_to_video_content(self):
        image = Image.new("RGB", (1920, 1080))
        region = {"x": 0.25, "y": 0.5, "width": 0.5, "height": 0.25}
        crop = analyze_vod._crop_region(image, region, (0, 135, 1920, 810))
        self.assertEqual(crop.size, (960, 203))

    def test_popup_deduplicator_releases_after_popup_disappears(self):
        deduplicator = analyze_vod._PopupDeduplicator(0.5)
        event = KillEvent("player", "", 2, "Double Kill", "double_kill")
        self.assertTrue(deduplicator.observe(event, 0.0))
        self.assertFalse(deduplicator.observe(event, 0.25))
        self.assertFalse(deduplicator.observe(None, 1.1))
        self.assertTrue(deduplicator.observe(event, 1.2))

    def test_analyze_video_emits_media_timed_events_without_live_event_file(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "match.mp4"
            video.touch()
            frame = bytes([20, 40, 60]) * 4
            process = FakeProcess(frame + frame)
            config = {
                "rois": [{"x": 0, "y": 0, "width": 1, "height": 1}],
                "capture_interval": 0.25,
                "cooldown_seconds": 0.5,
                "player_nick": "Absolution",
                "trigger_messages": [{"phrase": "double kill", "event": "double_kill"}],
            }
            with (
                patch.object(analyze_vod, "OUTPUT_WIDTH", 2),
                patch.object(analyze_vod, "OUTPUT_HEIGHT", 2),
                patch.object(analyze_vod, "_executable", side_effect=lambda name, path: name),
                patch.object(analyze_vod, "_probe_dimensions", return_value=(2, 2)),
                patch.object(analyze_vod.subprocess, "Popen", return_value=process),
            ):
                events = analyze_vod.analyze_video(video, config, FakeOcr())

        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["event"], "double_kill")
        self.assertEqual(events[0]["ts"], 0.0)
        self.assertEqual(events[0]["timecode"], "00:00:00.000")
        self.assertEqual(events[0]["record_type"], "vod_detection")

    def test_write_events_refuses_to_replace_output_without_overwrite(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            output = Path(temp_dir) / "events.jsonl"
            analyze_vod.write_events(output, [{"event": "double_kill"}])
            with self.assertRaises(FileExistsError):
                analyze_vod.write_events(output, [])
            analyze_vod.write_events(output, [], overwrite=True)
            self.assertEqual(output.read_text(encoding="utf-8"), "")

    def test_extract_clips_uses_ffmpeg_stream_copy_and_names_clips(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "match.mp4"
            video.touch()
            events = Path(temp_dir) / "match.mp4.events.jsonl"
            events.write_text(
                json.dumps({"ts": 10.0, "event": "triple_kill", "count": 3, "tag": ""})
                + "\n"
                + json.dumps({"ts": 25.5, "event": "penta_kill", "count": 5, "tag": "missile"})
                + "\n",
                encoding="utf-8",
            )
            output_dir = Path(temp_dir) / "clips"

            clip1 = output_dir / "match_triple_kill_c3_000.mp4"
            clip2 = output_dir / "match_penta_kill_c5_missile_001.mp4"

            def create_clip(args, *a, **kwargs):
                # args is the command list, last element is output path
                Path(args[-1]).write_bytes(b"dummy")
                result = MagicMock()
                result.returncode = 0
                result.stdout = ""
                result.stderr = ""
                return result

            with patch.object(analyze_vod, "_executable", return_value="ffmpeg"):
                with patch.object(analyze_vod.subprocess, "run", side_effect=create_clip) as mock_run:
                    clips = analyze_vod.extract_clips(
                        video, events, output_dir, pre_seconds=3.0, post_seconds=3.0
                    )

            self.assertEqual(len(clips), 2)
            self.assertEqual(clips[0], clip1)
            self.assertEqual(clips[1], clip2)

            calls = mock_run.call_args_list
            self.assertEqual(len(calls), 2)

            args1 = calls[0].args[0]
            self.assertIn("-ss", args1)
            ss_idx = args1.index("-ss")
            self.assertEqual(args1[ss_idx + 1], "7.000")
            self.assertIn("-t", args1)
            t_idx = args1.index("-t")
            self.assertEqual(args1[t_idx + 1], "6.000")
            self.assertIn("-c", args1)
            c_idx = args1.index("-c")
            self.assertEqual(args1[c_idx + 1], "copy")
            self.assertIn(str(clip1), args1)

            args2 = calls[1].args[0]
            self.assertIn("-ss", args2)
            ss_idx = args2.index("-ss")
            self.assertEqual(args2[ss_idx + 1], "22.500")

    def test_extract_clips_skips_existing_without_overwrite(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "match.mp4"
            video.touch()
            events = Path(temp_dir) / "match.mp4.events.jsonl"
            events.write_text(
                json.dumps({"ts": 10.0, "event": "kill", "count": 1, "tag": ""}) + "\n",
                encoding="utf-8",
            )
            output_dir = Path(temp_dir) / "clips"
            output_dir.mkdir()
            existing_clip = output_dir / "match_kill_c1_000.mp4"
            existing_clip.write_bytes(b"dummy")

            mock_ffmpeg = MagicMock()

            with patch.object(analyze_vod, "_executable", return_value="ffmpeg"):
                with patch.object(analyze_vod.subprocess, "run", mock_ffmpeg):
                    clips = analyze_vod.extract_clips(
                        video, events, output_dir, pre_seconds=3.0, post_seconds=3.0
                    )

            self.assertEqual(len(clips), 1)
            self.assertEqual(clips[0], existing_clip)
            mock_ffmpeg.assert_not_called()

    def test_extract_clips_overwrites_existing_with_flag(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "match.mp4"
            video.touch()
            events = Path(temp_dir) / "match.mp4.events.jsonl"
            events.write_text(
                json.dumps({"ts": 10.0, "event": "kill", "count": 1, "tag": ""}) + "\n",
                encoding="utf-8",
            )
            output_dir = Path(temp_dir) / "clips"
            output_dir.mkdir()
            existing_clip = output_dir / "match_kill_c1_000.mp4"
            existing_clip.write_bytes(b"old")

            def create_clip(args, *a, **kwargs):
                Path(args[-1]).write_bytes(b"new")
                result = MagicMock()
                result.returncode = 0
                result.stdout = ""
                result.stderr = ""
                return result

            with patch.object(analyze_vod, "_executable", return_value="ffmpeg"):
                with patch.object(analyze_vod.subprocess, "run", side_effect=create_clip) as mock_run:
                    clips = analyze_vod.extract_clips(
                        video, events, output_dir, pre_seconds=3.0, post_seconds=3.0, overwrite=True
                    )

            self.assertEqual(len(clips), 1)
            self.assertEqual(clips[0], existing_clip)
            mock_run.assert_called_once()

    def test_extract_clips_handles_empty_events_file(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "match.mp4"
            video.touch()
            events = Path(temp_dir) / "match.mp4.events.jsonl"
            events.write_text("", encoding="utf-8")
            output_dir = Path(temp_dir) / "clips"

            with patch.object(analyze_vod, "_executable", return_value="ffmpeg"):
                clips = analyze_vod.extract_clips(
                    video, events, output_dir, pre_seconds=3.0, post_seconds=3.0
                )

            self.assertEqual(clips, [])

    def test_extract_clips_raises_on_missing_video(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "missing.mp4"
            events = Path(temp_dir) / "events.jsonl"
            events.touch()
            output_dir = Path(temp_dir) / "clips"

            with self.assertRaises(FileNotFoundError):
                analyze_vod.extract_clips(video, events, output_dir)

    def test_extract_clips_raises_on_missing_events(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            video = Path(temp_dir) / "match.mp4"
            video.touch()
            events = Path(temp_dir) / "missing.jsonl"
            output_dir = Path(temp_dir) / "clips"

            with self.assertRaises(FileNotFoundError):
                analyze_vod.extract_clips(video, events, output_dir)


if __name__ == "__main__":
    unittest.main()
