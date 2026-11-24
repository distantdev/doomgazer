# CLAP and VST3

One `DoomgazerAudioProcessor` serves both formats. VST3 comes from `juce_add_plugin`. CLAP comes from `clap_juce_extensions_plugin` (`CLAP_ID` `com.doomgazer.plugin`, features `audio-effect` and `distortion`). The wrappers under `build-msvc/_deps/` are the format boundary. Change `plugin/PluginProcessor.cpp` and `plugin/PluginEditor.cpp`. Leave the wrappers alone unless a pin update changed them.

Window size is in [SKILL.md](SKILL.md). This file is the rest of the format contract.

## What each wrapper reads

| Concern | Plugin code | What the host sees |
| --- | --- | --- |
| Parameters | `AudioProcessorValueTreeState` ids | VST3 parameters and CLAP params, from those ids |
| State | `getStateInformation` / `setStateInformation` | VST3 chunk and CLAP `state` |
| MIDI | `acceptsMidi()` true, `producesMidi()` false | One CLAP note port, MIDI dialect. VST3 event input. Both deliver a `MidiBuffer` |
| Buses | `isBusesLayoutSupported`: mono or stereo, input matches output | CLAP audio ports from `getBusCount` |
| Tail | `getTailLengthSeconds()` returns 8 | Host tail / PDC compensation |
| Programs | `getNumPrograms()` returns 128. The index is the PC code, not the browser row. | VST3 program parameter carries host program changes into `setCurrentProgram`. CLAP has no program list; it uses the MIDI program-change message |

`processBlock` is the only MIDI consumer. Do not add a second path for one format.

## Audio thread

`processBlock` runs on the audio thread in both formats.

- Keep `ScopedNoDenormals`.
- Read sound through `getRawParameterValue` and the existing atomics (`outputMono`, `monoPaInvertR`, `subPitch`, the CC map).
- Tempo comes from `getPlayHead()` inside `processBlock`.
- No allocation, file IO, `MessageManager` calls, or editor calls on this thread.
- `presetLock` is held on the message thread. Leave it out of `processBlock`.
- `setValueNotifyingHost` runs on the message thread. `queueHostNotification` and `holdHostParams` exist so `processBlock` does not copy knobs mid-write. Keep that split when a preset or gesture notifies the host.

## Values the host can automate

A value the host should automate or record as a parameter is an APVTS parameter. Slider attachments and `setValueNotifyingHost` notify VST3 and CLAP. A write that skips both moves the sound and leaves the host's stored value stale.

`outputMono`, `monoPaInvertR`, `subPitch`, the CC map, stomp maps, and the A/B slots are state properties. They persist in the chunk. They do not appear as a VST3 lane or a CLAP param until they become APVTS parameters. Adding one is a single APVTS change. Both formats pick it up.

## State

`getStateInformation` writes one XML chunk. `setStateInformation` reads it. After a CLAP load, the wrapper calls `paramsRescan(CLAP_PARAM_RESCAN_VALUES)` so the host re-reads parameter values. Do not add a second rescan on top.

Saved sessions are not a compatibility target. The prototype-presets rule owns that. The chunk may change shape. Do not add a version field, or a CLAP-only or VST3-only fallback, so an older project still loads.

## UI thread

The editor, its timer, file choosers, and `handleAsyncUpdate` run on the message thread. The audio thread wakes that work with `triggerAsyncUpdate()`.

CLAP scales the editor with `setScaleFactor`. VST3 scales the native peer. The layout code is the same. Follow the size contract in [SKILL.md](SKILL.md) so those two coordinate spaces stay aligned.

## Before calling a format change done

Rebuild VST3 and CLAP from `build-msvc`. In the same host, open each one, close and reopen the window, resize it, reload the session, move one hosted parameter, and send a CC. The standalone window may fit itself to the screen once. Deploy only when asked.

If a wrapper seems to ignore the processor, read the pinned source before editing the plugin:

- VST3: `juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp`
- CLAP: `clap-juce-extensions-src/src/wrapper/clap-juce-wrapper.cpp`

Search Ref when a host API is unclear, then confirm the pin. The pin wins.
