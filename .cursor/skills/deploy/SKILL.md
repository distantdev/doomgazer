---
name: deploy
description: Build the Release VST3, CLAP, and standalone, then copy them to the DAW laptop. Use when the user says deploy, deploy to daw laptop, DAW-LAPTOP, or asks to copy the plugin into Bitwig or REAPER.
---

# Deploy

When the user says deploy, run this from the repo root. Do not inspect the build, CMake, or JUCE first.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_and_deploy.ps1
```

`-SkipBuild` copies the existing Release artefacts. `-TargetHost` defaults to `DAW-LAPTOP`.

The script configures `build-msvc` with Visual Studio 18 2026 only when `CMakeCache.txt` is missing. If the generator is anything else, it stops. Do not pass `-G Ninja`.

Do not launch the standalone unless the user asked. A missing `presets/` folder is normal: factory presets are compiled in. Do not create that folder.

`JUCE_ASIO=1` is already set on the plugin target. Do not search for the ASIO SDK unless the build error names a missing `iasiodrv.h`.

Report only what the script printed: which copies succeeded, their timestamps, and the rescan reminder. If it exits non-zero, quote the error and stop.
