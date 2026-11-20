# Killfeed detector (Windows OCR)

Managed process that watches Delta Force UI regions with Windows OCR
(``winocr`` â†’ ``Windows.Media.Ocr``) and writes kill events to a JSONL file
the OBS plugin tails. The plugin launches it and forwards its output to OBS
logs.

## Requirements

- Windows 10/11 x64
- Python 3.10+
- The Windows OCR language pack (en-US):
  `Add-WindowsCapability -Online -Name Language.OCR~~~en-US~0.0.1.0`
  (the `winocr` package asserts on this and prints the exact command)
- `pip install -r requirements.txt`

## Quick start

```powershell
cd tools/killfeed_detector
python roi_tool.py --shot probe.png --full   # capture screen, find the killfeed
python roi_tool.py --probe                   # OCR the configured ROI once
python -m killfeed_detector                  # run the detector loop
```

## Configuration

`config.json` (next to this package) overrides the defaults in
`config.py`. The plugin writes its settings to the shared `detector.json`.
The CLI flags `--roi L T W H`, `--nick`, and `--events-file` win over the
file. Key settings:

| key | meaning |
|---|---|
| `rois` | screen-relative `{x, y, width, height}` regions for the multikill banner and killfeed |
| `capture_interval` | seconds between captures (default 0.25) |
| `upscale` | ROI scale factor before OCR (default 2) |
| `ocr_language` | Windows OCR language tag (default `en`) |
| `player_nick` | player label attached to emitted event records |
| `trigger_messages` | editable `{phrase, event}` popup mappings |
| `cooldown_seconds` | ignore repeats within this window |
| `events_file` | JSONL bridge file the OBS plugin tails |

## Event format

One JSON object per line appended to `events_file`:

```json
{"ts": 1234.5, "event": "double_kill", "player": "Absolution", "victim": "", "count": 2, "raw": "Double Kill +20 EXP"}
```

`event` is one of `kill`, `double_kill`, `triple_kill`, `multikill`,
`quadra_kill`, or `penta_kill`.
Only configured `trigger_messages` can emit events; killfeed rows alone do
not trigger clips. The default trigger phrases are `double kill`, `triple
kill`, `multikill`, `quadra kill`, `stinger kill`, `penta kill`, and `guided
missile kill`. Single-kill notices map to `kill`; double/triple notices map
to their matching event, quadra and penta notices retain their exact tier,
and generic multikill notices map to `multikill`. The bundled default regions
are normalized to screen size and can be overridden in `detector.json`. Each
`trigger_messages` entry maps a phrase to one of the event types above and may
include an optional `tag` string. The OBS dock exposes the tag as an editable
third column, so custom phrases can add tags to future clips. Saving the list
restarts the detector.
The dock also lets the user capture a primary-screen screenshot and select a
normalized OCR region. That selection replaces the bundled regions; users can
restore the defaults from the same settings panel.

```json
{"phrase": "guided missile kill", "event": "kill", "tag": "missile"}
```

The plugin groups detections until no new trigger arrives for the configured
`post_time` (default 3 seconds). Each new trigger restarts that quiet period,
and the clip keeps the highest recognized tier (`kill` < `double_kill` <
`triple_kill` < `multikill` < `quadra_kill` < `penta_kill`). One clip is
saved when the group settles. The OBS log records the selected label,
configured buffer/window values, clip tags, and one compact JSON line per
detection. OCR text is capped at 256 characters per log entry. The default
`guided missile kill` and `stinger kill` rows have `missile` and `launcher`
tags; tags are editable and can be assigned to any configured phrase.
After saving the OBS Replay Buffer file, the plugin can use `ffmpeg.exe` from
`PATH` to trim the grouped event window with stream copy. The boundaries are
keyframe-aligned, not frame-accurate. If FFmpeg is unavailable or trimming
fails, the plugin keeps and registers the full Replay Buffer clip; the reason
is written to the OBS log.
Matching tolerates one OCR character error across a phrase (for example,
`Triple kilU` matches `Triple Kill`). The plugin logs a compact OCR sample
every 10 seconds to the OBS log; use it to check whether the screen region was
captured and what Windows OCR recognized.

For example, the OBS dock can map `Squad Wipe` to `multikill`:

```json
{"phrase": "squad wipe", "event": "multikill"}
```

## VOD analysis

From the repository root, install FFmpeg and FFprobe on `PATH`, then run:

```powershell
$env:PYTHONPATH = ".\tools"
py -3 -m killfeed_detector.analyze_vod "F:\OBS\match.mp4" `
  --config "$env:APPDATA\DeltaForceClipper\detector.json"
```

The analyzer samples frames at `capture_interval`, applies the configured
normalized OCR regions and trigger phrases, and writes a JSONL event timeline
next to the video as `<video>.events.jsonl`. Each record's `ts` and `timecode`
refer to elapsed media time. It does not create clips; VOD clip extraction is
a separate planned step. Use `--output` to choose another file and
`--overwrite` to replace an existing result.

## Why Windows OCR

- Built into Windows 10/11 â€” no Tesseract install, no language-data download.
- Native WinRT API, fast on a small ROI, and better on light-text-on-dark
  game UI than Tesseract's binarization defaults.
- The `winocr` package wraps it with a Pillow-friendly API.
