# Delta Force Clipper

Windows x64 OBS plugin for automatic Delta Force highlight clipping.

## Current target setup

- Windows 11 x64
- OBS Studio 32.2.2
- Delta Force at 4K / 120 FPS
- OBS `Window Capture`
- In-game nickname: `Absolution`
- Primary mode: `Warfare`
- Planned secondary mode: `Operations`
- Recording directory: `F:\OBS`

## Features

- Dockable OBS interface with Replay Buffer status and manual test-save control.
- OBS restores the dock's position between launches; clip history drops entries
  when their video files are deleted.
- Python `winocr` detector launched and managed by the OBS plugin; detector
  output is forwarded to the OBS log.
- Configured popup phrases are the only detector triggers; OCR'd killfeed rows
  do not independently create single-kill clips.
- Editable popup-message triggers with a selectable clip type for each phrase.
- Primary-screen OCR region selection with a one-click restore to default areas.
- Grouped detections keep the highest tier; quadra/penta labels are preserved,
  with event details and tags recorded in the OBS log.
- EN, ES, and PT-BR interface languages, event timing, clip folder, and
  detector settings.
- Clip metadata library and history table refreshed after a completed save.
- Offline VOD analyzer that writes configured killfeed detections with video-relative timestamps.
- One-click Windows build and ZIP package.

No game memory access, injection, network inspection, or anti-cheat interaction is used or planned.

## Configure trigger phrases

In OBS, open **Tools -> Delta Force Clipper**, then select the **Trigger
phrases** tab. Double-click a phrase to edit it, choose the clip type in the
second column, and set an optional tag in the third column. Use **Add trigger**
or **Remove selected** to manage rows. Click **Save** to apply the trigger list
and restart the detector with the new configuration.

To adjust OCR capture, use **Configuration -> OCR capture region -> Select
screen region**. Switch to the game during the countdown, then drag around the
notification text in the captured screen image. Save to apply, or choose **Use
default regions** to restore the bundled areas.

## Analyze a recorded VOD

The detector package includes an offline analyzer. Install FFmpeg and FFprobe
on `PATH`, then run this from the repository root in PowerShell:

```powershell
$env:PYTHONPATH = ".\tools"
py -3 -m killfeed_detector.analyze_vod analyze "F:\OBS\match.mp4" `
  --config "$env:APPDATA\DeltaForceClipper\detector.json"
```

It samples frames at the configured `capture_interval`, uses the configured
OCR regions and trigger phrases, and writes a `<video>.events.jsonl` timeline.
The output contains media-relative seconds and timecodes; it does not create
clips yet. Existing output is preserved unless `--overwrite` is specified.
This keeps VOD analysis separate from live capture and the live event bridge.

### Extract clips from VOD events

After analyzing a VOD, extract clips for each detected event:

```powershell
$env:PYTHONPATH = ".\tools"
py -3 -m killfeed_detector.analyze_vod extract "F:\OBS\match.mp4" `
  "F:\OBS\match.mp4.events.jsonl" `
  --output "F:\OBS\match_clips" `
  --pre 3.0 --post 3.0
```

This uses FFmpeg stream copy (keyframe-aligned) to cut clips around each event
timestamp. Adjust `--pre` and `--post` to control the clip window.

## Build and package (Windows)

Install Visual Studio 2022 with the C++ desktop workload, CMake 3.28+, and
Git. Double-click `Build-DeltaForceClipper.cmd` to configure, build, and create
the ZIP package. A Developer PowerShell is not required. On first run, the
script downloads the official OBS plugin template's CMake infrastructure and
the OBS/Qt build dependencies.

The package is written to `build_x64/`. See [docs/INSTALLATION.md](docs/INSTALLATION.md)
for manual build commands and installation details.

## Data locations

Configuration:

`%APPDATA%\DeltaForceClipper\config.json`

Library:

`%APPDATA%\DeltaForceClipper\clips.json`

Default clips:

`F:\OBS\DeltaForceClips\yyyy-MM-dd\...`

## Important

The detector requires Python 3.10+ and the Windows OCR language pack. See
[docs/INSTALLATION.md](docs/INSTALLATION.md) for setup. The plugin and detector
use OBS's Replay Buffer; automatic saves require it to be active. The requested
pre-event and post-event context is applied after OBS saves the replay. If
`ffmpeg.exe` is on `PATH`, the plugin asynchronously trims the replay with
stream copy; cuts are keyframe-aligned, not frame-accurate. If FFmpeg is
unavailable or trimming fails, the full replay is kept and the reason is
reported in the OBS log.
