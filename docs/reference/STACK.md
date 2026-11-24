# Toolchain reference

Checked 2026-11-09. Look up JUCE, CLAP, Catch2, CPM, and pluginval APIs with Ref.
Do not keep local copies of those manuals in this repo.

## What we build with

| Piece | Pin | Why |
| --- | --- | --- |
| Language | C++20 | JUCE 9 requires C++17. The DSP library is already C++20. |
| CMake | 3.25 minimum, 4.4.4 installed | Matches the current JUCE CMake template practice. JUCE 9 itself accepts 3.22. |
| Generator | Visual Studio 18 2026, x64, multi-config | Live trees are `build-msvc` (plugin on) and `build-audit` (plugin off). Pass `--config Release`. Do not reconfigure either tree with Ninja. JUCE 8 removed MinGW. |
| CPM.cmake | 0.43.2 in `cmake/CPM.cmake` | Current release. Wraps FetchContent. |
| JUCE | tag `9.0.5` (2 Nov 2026) | Current stable. AudioProcessor v2 and native CLAP are not in this release. |
| CLAP | `clap-juce-extensions` commit `6bde108329ff9fc24404b79b8b397261a75cf6a8` (6 Nov 2026) | JUCE 9.0.5 `FORMATS` are `Standalone Unity VST3 AU AUv3 AAX VST LV2` only. This commit is the current `main` and includes the JUCE 9 fixes. |
| Catch2 | `v3.16.1` (8 Oct 2026) | Current release. Link `Catch2::Catch2WithMain`. |
| Validator | pluginval `v2.0.0-beta3` (14 Oct 2026) | First line that builds against JUCE 9. `v1.0.5` is the last non-beta tag and is still on JUCE 8. |
| Windows installer | Inno Setup, after the plugin builds | Used by the current JUCE CMake template for the distributable `.exe`. Dev builds do not copy into `Program Files`. |

## Rules that already apply to this plugin

- `PLUGIN_MANUFACTURER_CODE` and `PLUGIN_CODE` need one leading capital for AU. Ours are `Dmzr` and `Dmgz`.
- Do not pass `VST` unless `juce_set_vst2_sdk_path` was called. VST2 stays off.
- CLAP is a second target, `Doomgazer_CLAP`, created by `clap_juce_extensions_plugin` after `juce_add_plugin`.
- `COPY_PLUGIN_AFTER_BUILD` stays off here. JUCE would copy the VST3 into `C:\Program Files\Common Files\VST3`, which needs an administrator. `tools/build_and_deploy.ps1` builds `build-msvc` and `tools/deploy_to_daw.ps1` copies VST3, CLAP, and the standalone to `\\DAW-LAPTOP`. `tools/dsp_check.ps1` is the headless check in `build-audit`.
- JUCE 9 turns Windows multi-touch off unless `PluginEditor::usesWindowsMultiTouch()` returns true. This plugin does not need that.
- Existing `AudioProcessor` and `AudioProcessorValueTreeState` code still compiles. The parameter rewrite (APv2) is not in 9.0.5.

## Upstream links

- https://github.com/juce-framework/JUCE/releases/tag/9.0.5
- https://github.com/free-audio/clap-juce-extensions/commit/6bde108329ff9fc24404b79b8b397261a75cf6a8
- https://github.com/catchorg/Catch2/releases/tag/v3.16.1
- https://github.com/cpm-cmake/CPM.cmake/releases/tag/v0.43.2
- https://github.com/Tracktion/pluginval/releases/tag/v2.0.0-beta3
- https://github.com/sudara/pamplejuce
