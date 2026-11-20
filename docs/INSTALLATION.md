# Development installation

1. Install Visual Studio 2022 with the C++ desktop workload, CMake 3.28+,
   Git, and Python 3.10+.
2. Double-click `Build-DeltaForceClipper.cmd` in the project root. It
   configures, builds, and creates the ZIP package; on first use it also
   downloads the official OBS plugin template's CMake infrastructure.

   Or run it from PowerShell:

   ```powershell
   .\scripts\setup-compilation.ps1
   ```

   The build uses CMake's Visual Studio 2022 generator, so a Developer
   PowerShell is not required.

3. To run the individual steps manually from the project root:

   ```powershell
   cmake --preset windows-x64
   cmake --build --preset windows-x64
   cpack --config build_x64/CPackConfig.cmake -C RelWithDebInfo -B build_x64
   ```

   The ZIP package is written under `build_x64/`. Running bare `cpack` from the
   project root does not find the generated configuration, so CPack has no
   generator configured.

4. Close OBS.
5. Install the plugin into OBS's standard plugins directory using the generated package, or copy the plugin module/data from the install tree into OBS's `obs-plugins/64bit` and `data/obs-plugins` directories.
6. (Optional) Install FFmpeg and add the folder containing `ffmpeg.exe` to
   `PATH`. Event-relative trimming uses stream copy and is keyframe-aligned.
   If FFmpeg is not available, or trimming cannot be completed, the plugin
   keeps the full Replay Buffer replay and records the reason in the OBS log.
   VOD analysis also requires `ffprobe.exe` on `PATH` (usually included with
   FFmpeg). See the VOD analysis command in the project README.
7. Install the detector dependencies in PowerShell:

   ```powershell
   py -3 -m pip install -r .\tools\killfeed_detector\requirements.txt
   ```

   Enable the Windows English OCR capability if it is not already installed:

   ```powershell
   Add-WindowsCapability -Online -Name Language.OCR~~~en-US~0.0.1.0
   ```

8. Start OBS 32.2.2 and use **Tools -> Delta Force Clipper** to open the dockable control panel.
   The plugin starts the detector when enabled. Detector startup and OCR
   messages are written to the OBS log. If Python or its requirements are
   missing, install them and restart the detector by saving the settings. The
   dock is placed with OBS's other docks and can be moved or floated.
9. To edit recognized popup phrases, open the dock's **Trigger phrases** tab.
   Double-click a phrase or optional tag to edit it, select a clip type in the
   second column, and use **Add trigger** or **Remove selected** to manage
   entries. Click **Save** to apply the list and restart the detector.
10. To tune OCR capture, open **Configuration -> OCR capture region** and select
   **Select screen region**. Switch to the game during the countdown; then drag
   around the notification text in the captured screen image. Click **Save** to
   apply it. The custom region replaces the defaults; select **Use default
   regions** to restore them.

A ZIP package is generated under `build_x64/`; it contains the OBS plugin and
detector source. Python and the Windows OCR capability are installed separately.

### No automatic clips

Open the latest OBS log from **Help -> Log Files -> View Current Log**. The
detector reports an OCR sample every 10 seconds, including how many screen
regions it captured. `captured 0/N` indicates a screen-capture problem;
`text=<empty>` means the regions were captured but OCR returned no text. A
recognized configured phrase should be followed by an `event:` detector line
and a plugin `detector event` line. The OCR sample contains on-screen game
text, so it remains in the local OBS log.
