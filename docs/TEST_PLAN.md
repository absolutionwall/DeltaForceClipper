# First test plan

1. Build the plugin against the official OBS plugin template.
2. Install it into the OBS plugins directory for OBS 32.2.2.
3. Start OBS and verify `Tools -> Delta Force Clipper` exists.
4. Open the plugin dialog.
5. Set clip folder to `F:\OBS\DeltaForceClips`.
6. Set buffer duration to 30 seconds initially.
7. Click `Start Buffer`.
8. Record a short gameplay segment.
9. Click `Test Save Clip`.
10. Verify a clip file is created below `F:\OBS\DeltaForceClips\yyyy-MM-dd\`.
11. Verify `%APPDATA%\DeltaForceClipper\clips.json` contains the clip metadata.

After this works, enable/verify auto-start and start testing different buffer durations.
