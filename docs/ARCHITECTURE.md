# Delta Force Clipper architecture

## Current capture path

The Windows plugin manages the detector process and polls its append-only event bridge:

OBS dock -> managed Python/winocr detector -> resolution-relative UI regions -> JSONL event -> grouped Replay Buffer save -> optional FFmpeg stream-copy trim -> clip organization -> clips.json

The detector prioritizes centered Double/Triple Kill notifications and only
emits events for configured trigger phrases. Nickname matching, event timing,
UI language, and clip history are configured in the dock. Python and the
Windows OCR language pack are runtime prerequisites; detector output is
forwarded to the OBS log.

The plugin targets Windows x64 and OBS Studio 32.x. OCR can be tuned in the
separate detector without rebuilding the OBS plugin.
The dock can capture a primary-screen screenshot and select a normalized OCR
region. A custom selection replaces the default regions; the user can restore
the defaults at any time.

Automatic detections in one quiet-time group are labeled with the highest
recognized tier. Quadra and penta kills retain distinct event types and kill
counts. For each automatic clip, the OBS log records grouped OCR detections, UTC
receive times, configured buffer/window values, and the final clip label.
Detection records are logged as compact JSON lines; OCR text is capped at 256
characters per entry.
Detector monotonic timestamps are process-relative and are not media offsets.
Tags are configured per trigger phrase in the dock. The defaults tag Guided
Missile Kill as `missile` and Stinger Kill as `launcher`; users can assign
other tags to custom phrases. A grouped clip can retain multiple tags.

After OBS saves the Replay Buffer file, the plugin uses `ffmpeg.exe` from
`PATH` to trim around the grouped detections using stream copy. The requested
window spans the first detection minus the configured pre-event time through
the last detection plus the configured post-event time. Stream-copy cuts are
keyframe-aligned rather than frame-accurate. FFmpeg runs asynchronously so the
OBS UI remains responsive. If FFmpeg is unavailable, the requested window is
larger than the configured buffer, or trimming/replacement fails, the original
full replay is retained and registered; the OBS log metadata records the
fallback reason. The plugin does not restart an active Replay Buffer to change
its duration, avoiding loss of pre-roll.

## VOD analysis and planned extraction

The VOD analyzer decodes sampled frames from a recording with FFmpeg, applies
the configured normalized OCR regions and Windows OCR, then writes a separate
JSONL timeline with media-relative timestamps. It reuses the configured
trigger phrases and does not write to the live event bridge or create clips.
FFmpeg-based clip extraction from this timeline remains a future step.

Selectable OBS-source capture is deferred; live detection continues to use
desktop capture and the existing normalized screen-region selector.

## Library

V0.1 uses `clips.json` for transparency and portability. The schema is designed so that SQLite can replace JSON later without changing the UI/domain model.
