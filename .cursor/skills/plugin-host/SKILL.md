---
name: plugin-host
description: CLAP and VST3 contract for Doomgazer. Use when editing the processor or editor, parameters, state, MIDI, buses, tail, VST3, CLAP, or the standalone window, or when DPI, scale, REAPER, or Bitwig shows the wrong size, a stale automation value, or a different layout per format.
---

# Plugin host

JUCE is pinned at 9.0.5. CLAP is pinned at clap-juce-extensions `6bde108329ff9fc24404b79b8b397261a75cf6a8`. Both tags are in `CMakeLists.txt`. After configure, the wrappers are under `build-msvc/_deps/`. Read them before changing host-facing behavior. When a host API is unclear, search Ref, then confirm against those pinned sources.

Parameters, state, MIDI, buses, tail, and the audio thread are in [reference.md](reference.md). Window size is below.

## Size contract

The constructor calls `setResizeLimits(1080, 540, 2000, 1400)` and `setSize(1200, 580)`. That logical size is what both wrappers report to the host.

`resized()` lays out the bounds the wrapper set. A user drag is the wrapper calling `setBounds` on the editor.

The editor does not read the parent window and call `setSize` from `parentSizeChanged`, `visibilityChanged`, or `parentHierarchyChanged`.

`parentHierarchyChanged` may fit the standalone window to the screen once. That is the only editor-initiated size change after construction.

## VST3

Display scale is on the native peer (`PluginScaleFactorManager` in `juce_audio_plugin_client/detail/juce_PluginScaleFactorUtilities.h`), not `AudioProcessorEditor::setScaleFactor`.

`ContentWrapperComponent::onSize` in `juce_audio_plugin_client_VST3.cpp` writes the editor bounds while `resizingChild` is set, so the editor does not ask the host to resize again. `attached()` calls `resizeHostWindow()` with the editor's logical size.

A later `setSize` from the editor runs outside that guard. `childBoundsChanged` calls `resizeHostWindow()`, and REAPER scales the UI into the window it already opened. That shows up on launch and when the plugin window is closed and reopened.

## CLAP

Windows sizes are physical pixels. `EditorWrapperComponent` in `clap-juce-extensions-src/src/wrapper/clap-juce-wrapper.cpp` calls `editor->setScaleFactor`, forces the peer scale to 1, and `resized()` maps the window with `getLocalArea` so the scale is applied once.

Do not measure the parent and divide by the editor transform. That coordinate space differs from VST3, and the layouts diverge.

## Checks

After an editor size change, rebuild VST3 and CLAP from `build-msvc`. Compare both formats at open, after closing and reopening the window, and after a manual resize. Deploy only when asked.
