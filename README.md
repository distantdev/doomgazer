# Doomgazer

Downtuned guitar processor, version 0.1.0, from [Distant Nebula](https://distantnebula.com/).

It is a VST3, a CLAP, and a standalone app for downtuned guitar. Two paths share one contour, then grit, space, and modulation.

## Set the level

The input meter is the held peak after Input Trim.

- Blue is under -18 dB.
- Green is -18 to -8 dB. The factory sounds were made in this window.
- Red is hotter than -8 dB.

The lamp means the guitar reached the plugin within 1 dB of full scale, before the trim. If the interface gain is already low and the lamp is on, turn Input down.

Mute silences the output so you can tune. The tuner stays live: green is in tune, red is sharp or flat. While Mute is on, the scope shows the dry input. Otherwise the scope is the output, with the input as a faint trace behind it. The spectrum is the dry input, from 50 Hz to 10 kHz. Peak and RMS are the output, and their lamp lights when the output clips.

## CORE and TWIN

**CORE** is one path. Drive is the gain into it: light picking stays cleaner, harder playing breaks up. Tone and Level shape and trim that path, and Sub mixes a fundamental underneath.

**TWIN** splits the note. Below the split is the low path, with its own gain and an octave divider. Above the split is the harmonic wall. Blend moves the balance from the low foundation to the wall. Carve ducks the low path while a melody plays. Twin Level is this engine's trim, separate from Core Level.

The contour, the doubler, grit, and the effects after them are the same on both paths. The CORE / TWIN switch is in the header.

## Sections

**CORE.** Drive (gain, tone, level) and Sub.

**TWIN.** Low, Wall, Carve, and Sub-Oct. Sub-Oct is the divided octave: its level, its tone, and a solo blend that fades the dry path until only that octave is left.

**CONTOUR.** The contour both paths share: thump, scoop, high cut, and the body and presence centres. Doubler runs a second contour on the same split. Its Level is its own output. Spread pans it opposite the original. Invert flips its phase.

**DIRT.** Grit, a parallel clipped band after the cores. Character runs from a sustained scooped clip to a hard mid bark. Amount at zero leaves the dry path through.

**SPACE.** Delay is a tape echo or a reverse swell. Reverb is an abyss hall, a shimmer, or a spring. Pre puts the effect before the contour, Post after it. Atmosphere chooses which of delay and reverb runs first, and holds Freeze. Sync snaps delay time and tremolo speed to the host tempo. The type pickers do not include Off: the action-row button is what brings the effect in and out, and it remembers the type while the effect is off.

Freeze latches on the panel. From MIDI it is momentary: the tail stays frozen while the control is down, and lets go when you release it.

**MOD.** Tremolo (tube bias, opto, harmonic, or flutter), Glide (early reflections or a reverse wash), Swell (fades new notes in), Phaser, Chorus, Howl, and Pedal. Chorus can be full range or baritone-voiced. Howl is a resonator on a harmonic of the note. Pedal is a volume pedal or a wah. Heel is silence or a dark wah, toe is the saved level or an open wah, and the pedal never adds gain.

**IN/OUT.** Input is the trim, the high-pass into the drive, anti-fizz, and the pre-drive cutoff. Leveler is the sustain and the supply sag. Sag lets harder hits compress and lose a little edge. Gate, on the action row, is the quiet floor under the leveler.

**Rig**, on IN/OUT, is the output pair. PA STEREO leaves the image the preset saved. PA MONO sums the output to one channel and does not rewrite the preset. INV OUT flips the right side of that sum. Modules that have their own Mono / Stereo choice keep that choice stored; PA MONO only changes the output you hear.

## Action row

CONTOUR, CARVE, SUB-OCT, DOUBLER, INVERT, GRIT, DELAY, REVERB, TREMOLO, GLIDE, SWELL, PHASER, CHORUS, HOWL, PEDAL, GATE, SUSTAIN.

Each button wears its section's colour. Right-click one for MIDI Learn or Clear MIDI.

## Presets

Thirty-three factory presets are built in. You cannot overwrite them. A star after the name means the sound has been edited.

Click the name to open the browser. Filter by name or category. Load, save as, rename, delete (it asks you to confirm), open the preset folder, or rescan. The arrows beside the name step through the list and wrap. Scrolling on the name does the same.

Save writes the current user preset. On a factory preset, Save opens Save As. Revert drops the edits and reloads the preset you had.

A/B keeps two sounds. The button shows A or B. Click it to store the sound you are on and flip to the other. Both come back with the project.

The sound stored in the DAW project is the one that reopens. A preset file is read only when you load it.

User presets are text files in `%APPDATA%\Doomgazer\Presets`, one `*.doomgazer` file each. A file has a name, a category, optional notes, and one line per control. You can edit one by hand. A control the plugin does not know is ignored. A missing control keeps its default.

Each preset can carry a MIDI program number. In the browser that number is on the row, and a blank one is a dash. The host sees 128 programs, and the program slot is that number, not the preset's place in the list. A program change loads the preset with that number. A number with no preset is skipped.

The factory set, in the browser's groups:

- **Doom.** TWIN CORE, DYNAMIC HARMONIC CORE, SUBTERRANEAN LIFE DIVIDER, BONE-CRUSHER, WOOL VEIL, DRY CHOKE, RITUAL MASS, PICKED HALO, BARITONE VEIL, GAZE WALL.
- **Drone.** CELESTIAL SHIMMER CATHEDRAL, MAXIMUM SHIELDS, SLOWDIVE SOFT FOCUS, HARMONIC OPTO-TREM DRONE, ETHEREAL REVERSE SWELL, HUM DRONE, SLOW HYMN.
- **Space.** DENSE ROOM, DELAY BLOOM, TREBLE CREEP, RED ROOM, SPRING DRIP.
- **Motion.** INVERTED ACOUSTIC BEAT, TUBE BIAS VOODOO PULSE, BLOWN CONE FLUTTER, TORN SPEAKER CHATTER, PHASE RITUAL, PEDAL SWELL.
- **Grit.** VIOLIN SMOKE, BROKEN PA, COCKED RESIN, SPEAKER HOWL.
- **Noise.** RUNAWAY NOISE CATASTROPHE.

## MIDI

Right-click a knob, a switch, or a mode picker: MIDI Learn, Clear MIDI, or Reset to default. If that controller was already on another control, it moves.

Knobs follow the controller. A switch latches: it toggles when a CC crosses 64. Freeze is the exception above. An action-row button latches the same way, and it can learn a note as well as a CC. A note toggles it on note-on. The preset arrows learn from their own right-click, or from the MIDI page.

MIDI opens one map of every control, the action row, and the two preset arrows. You can listen on every channel or on one channel, turn program change on or off, and clear the map.

A new project starts on an Akai LPD8 layout: knobs on CC 12-43, pads on CC 70-101, preset select on CC 103, and the volume/wah switch on CC 104. Clear all leaves that project with nothing bound. The map is stored in the project.

On that page, Select by CC lets one controller pick a preset by its value. The CC number is next to the switch.

## Build from source

Windows, CMake 3.25 or newer, and Visual Studio 2026. From this directory:

```
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Release
```

Use that Visual Studio generator for `build-msvc`. The build writes:

- `build-msvc\Doomgazer_artefacts\Release\VST3\Doomgazer.vst3`
- `build-msvc\Doomgazer_artefacts\Release\CLAP\Doomgazer.clap`
- `build-msvc\Doomgazer_artefacts\Release\Standalone\Doomgazer.exe`

It does not copy them into a host. Put the VST3 in `C:\Program Files\Common Files\VST3` and the CLAP in `C:\Program Files\Common Files\CLAP`, then rescan. The standalone uses ASIO. Run `Doomgazer.exe`.

## License

Doomgazer is released under the MIT License. See [LICENSE](LICENSE).

The plugin is built with JUCE under the JUCE licence. JUCE is not part of the MIT grant.
