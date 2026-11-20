"""Default configuration for the Delta Force killfeed detector.

Values here are overridable via killfeed_detector/config.json (same keys) and
via the CLI (which wins over the file).
"""

from .events import DEFAULT_TRIGGER_MESSAGES

CONFIG = {
    # --- capture ---
    # Resolution-relative regions. The notification region is deliberately
    # separate from the top-right killfeed region.
    "rois": [
        {"x": 0.39, "y": 0.66, "width": 0.25, "height": 0.20},
        {"x": 0.80, "y": 0.05, "width": 0.20, "height": 0.22},
    ],
    # Capture interval in seconds. 0.25 = 4 samples/s, cheap for a small ROI.
    "capture_interval": 0.25,
    # Upscale factor applied to the ROI before OCR. Small killfeed text reads
    # better at 2x; set to 1 to disable.
    "upscale": 2,
    # WinOCR language tag. en-US works for the English killfeed.
    "ocr_language": "en",
    # --- detection ---
    # Player label attached to emitted event records.
    "player_nick": "Absolution",
    # Editable phrase-to-event mappings for the central game notifications.
    "trigger_messages": DEFAULT_TRIGGER_MESSAGES,
    # Ignore events that repeat within this many seconds (killfeed refresh,
    # OCR re-reads of the same line, multikill popups).
    "cooldown_seconds": 0.5,
    # Keep at most this many recent kill texts for duplicate detection.
    "recent_texts_max": 8,
    # --- output ---
    # Shared bridge file. The OBS plugin tails this file for events.
    "events_file": "%APPDATA%/DeltaForceClipper/events.jsonl",
    # Append-only event log. The plugin detects truncation and restarts at 0.
    "log_level": "INFO",
}
