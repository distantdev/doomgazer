#include "PluginEditor.h"

#include <algorithm>
#include <cmath>


namespace {

using dgui::AccentRole;

// The parameters are grouped into modules, and the modules into seven bands.
// One module is on screen at a time; the master output and the action row are
// always visible. Nothing is hidden behind an expander or a scrollbar.
constexpr int kBandCount = 7;
constexpr int kModuleCount = 22;
constexpr int kRigModule = 20;

const char* const kBandName[kBandCount] = { "CORE", "TWIN", "CONTOUR", "DIRT", "SPACE", "MOD",
                                            "IN/OUT" };

const dgui::AccentRole kBandRole[kBandCount] = {
    dgui::AccentRole::rust,
    dgui::AccentRole::moss,
    dgui::AccentRole::slate,
    dgui::AccentRole::gold,
    dgui::AccentRole::slate,
    dgui::AccentRole::violet,
    dgui::AccentRole::sage,
};

const char* const kBandHelp[kBandCount] = {
    "The single-path harmonic core: drive, tone, level, and sub weight.",
    "The Twin core: low path, harmonic wall, carve, and the divided octave.",
    "The cabinet both cores share, and a second contour with its own level and spread.",
    "Grit, the dirt box after the cores.",
    "Delay, reverb, and the atmosphere order and freeze.",
    "Tremolo, glide, swell, phaser, chorus, howl, and the pedal.",
    "Input, leveler, and the PA output pair.",
};

struct ModuleSpec
{
    int band;
    const char* name;
    const char* help;
};

const ModuleSpec kModules[kModuleCount] = {
    { 0, "DRIVE", "Gain, tone, and output trim of the single-path harmonic core." },
    { 0, "SUB", "A sub-bass fundamental mixed under the harmonic core." },
    { 2, "CONTOUR", "The cabinet both cores share: thump, scoop, cut, body and presence centres." },
    { 1, "LOW", "The low path: gain, the octave divider, and the split." },
    { 1, "WALL", "The harmonic wall above the split: gain, tilt, and the blend between paths." },
    { 2, "DOUBLER", "A second cabinet on the same split. Level is its output. Spread pans it opposite the original." },
    { 1, "CARVE", "Ducks the low path while a melody plays." },
    { 1, "SUB-OCT", "The divided octave: level, tone, and the solo blend." },
    { 3, "GRIT", "Parallel hard-clipped band: character, amount, frequency, and trim." },
    { 4, "DELAY", "Tape echo or reverse swell, with pre/post position, stereo, and tempo sync." },
    { 4, "REVERB", "Abyss, shimmer, or spring, with pre/post position." },
    { 5, "TREMOLO", "Tube bias, opto, harmonic, or flutter, with width and mix." },
    { 5, "GLIDE", "Early-reflection or reverse wash." },
    { 5, "SWELL", "Fades new notes in." },
    { 5, "PHASER", "Swept notches with feedback." },
    { 5, "CHORUS", "Modulated doubling, optionally baritone-voiced." },
    { 5, "HOWL", "A resonator that tracks a harmonic of the note." },
    { 5, "PEDAL", "The expression pedal: volume or wah." },
    { 6, "INPUT", "Input trim, the level window, pre-drive tightness, and anti-fizz." },
    { 6, "LEVELER", "The slow leveler and the supply sag." },
    { 6, "RIG", "The PA output pair." },
    { 4, "ATMOSPHERE", "Shared space controls: which of delay and reverb runs first, and the tail freeze." },
};

struct KnobSpec
{
    int parameter;
    const char* caption;
    int module;
};

const KnobSpec kKnobSpecs[] = {
    { 2, "DRIVE", 0 },
    { 35, "TONE", 0 },
    { 3, "LEVEL", 0 },
    { 4, "SUB", 1 },
    { 21, "THUMP", 2 },
    { 22, "SCOOP", 2 },
    { 23, "CUT OFF", 2 },
    { 32, "BODY", 2 },
    { 33, "PRES", 2 },
    { 7, "LOW", 3 },
    { 8, "OCT", 3 },
    { 31, "SPLIT", 3 },
    { 64, "CORE OCT", 3 },
    { 65, "TWIN OCT", 3 },
    { 5, "SUB DRV", 3 },
    { 96, "LEVEL", 3 },
    { 11, "WALL", 4 },
    { 12, "TILT", 4 },
    { 6, "BLEND", 4 },
    { 16, "LEVEL", 5 },
    { 15, "SPREAD", 5 },
    { 18, "THUMP", 5 },
    { 97, "SCOOP", 5 },
    { 98, "CUT OFF", 5 },
    { 99, "BODY", 5 },
    { 100, "PRES", 5 },
    { 10, "CARVE", 6 },
    { 57, "SUB LVL", 7 },
    { 58, "SUB TONE", 7 },
    { 68, "SOLO", 7 },
    { 69, "CHAR", 8 },
    { 70, "GRIT", 8 },
    { 71, "FREQ", 8 },
    { 72, "LEVEL", 8 },
    { 38, "MIX", 9 },
    { 39, "TIME", 9 },
    { 40, "FDBK", 9 },
    { 42, "DAMP", 9 },
    { 44, "MIX", 10 },
    { 45, "SIZE", 10 },
    { 46, "DECAY", 10 },
    { 48, "SHEEN", 10 },
    { 51, "SPEED", 11 },
    { 52, "DEPTH", 11 },
    { 54, "WIDTH", 11 },
    { 56, "MIX", 11 },
    { 60, "WASH", 12 },
    { 80, "ATK", 13 },
    { 81, "AMT", 13 },
    { 73, "RATE", 14 },
    { 74, "DEPTH", 14 },
    { 75, "FDBK", 14 },
    { 76, "MIX", 14 },
    { 77, "RATE", 15 },
    { 78, "MIX", 15 },
    { 83, "HOWL", 16 },
    { 84, "SENSE", 16 },
    { 94, "TOE", 17 },
    { 63, "INPUT", 18 },
    { 13, "TIGHT", 18 },
    { 29, "FIZZ", 18 },
    { 30, "PRE HZ", 18 },
    { 20, "SUSTAIN", 19 },
    { 28, "SAG", 19 },
};

struct SwitchSpec
{
    int parameter;
    const char* caption;
    int module;
    int width; // wide enough for the caption inline at the left
    bool inlineCaption;
};

const SwitchSpec kSwitchSpecs[] = {
    { 17, "INVERT", 5, 96, true },
    { 79, "BARITONE", 15, 104, true },
    { 67, "SYNC", 9, 78, true },
    { 66, "FREEZE", 21, 92, false },
};

// Two named states where both are on. Both labels are always drawn. PRE / POST
// carries no caption; the hover hint explains where it sits.
struct SelectorSpec
{
    int module;
    int parameter;
    const char* caption;
    const char* first;
    const char* second;
};

const SelectorSpec kSelectorSpecs[] = {
    { 2, 92, "", "MONO", "STEREO" },
    { 9, 41, "", "PRE", "POST" },
    { 9, 85, "", "MONO", "STEREO" },
    { 10, 47, "", "PRE", "POST" },
    { 10, 86, "", "MONO", "STEREO" },
    { 11, 89, "", "MONO", "STEREO" },
    { 12, 61, "", "PRE", "POST" },
    { 12, 90, "", "MONO", "STEREO" },
    { 14, 88, "", "MONO", "STEREO" },
    { 15, 87, "", "MONO", "STEREO" },
    { 16, 91, "", "MONO", "STEREO" },
    { 17, 95, "", "VOL", "WAH" },
};

// The atmosphere order: both paths, and the switch that picks one.
struct OrderSpec
{
    int module;
    int parameter;
    const char* first;
    const char* second;
};

const OrderSpec kOrderSpecs[] = {
    { 21, 49, "DELAY", "REVERB" },
};

struct ComboSpec
{
    int module;
    int parameter;
    const char* caption;
    const char* items;
    int firstValue; // the parameter value the first item stands for
};

const ComboSpec kComboSpecs[] = {
    { 9, 37, "", "TAPE ECHO|REVERSE", 1 },
    { 10, 43, "", "ABYSS|SHIMMER|SPRING", 1 },
    { 11, 53, "", "TUBE BIAS|OPTO|HARMONIC|FLUTTER", 0 },
    { 12, 59, "", "EARLY REF|REVERSE", 1 },
    { 16, 82, "PITCH", "H2|H3|H4|H5", 0 },
};

// The always-visible action rows: every effect's on/off shortcut, including the
// three that used to carry a toggle inside their own module. Each one is a plain
// button with MIDI learn; none of them is a parameter toggle duplicated from a
// module.
struct StripSpec
{
    doomgazer::Stomp stomp;
    const char* label;
    int band;
};

const StripSpec kStripSpecs[] = {
    { doomgazer::Stomp::contour, "CONTOUR", 2 },
    { doomgazer::Stomp::carve, "CARVE", 1 },
    { doomgazer::Stomp::subOctave, "SUB-OCT", 1 },
    { doomgazer::Stomp::doubler, "DOUBLER", 2 },
    { doomgazer::Stomp::invert, "INVERT", 2 },
    { doomgazer::Stomp::grit, "GRIT", 3 },
    { doomgazer::Stomp::delay, "DELAY", 4 },
    { doomgazer::Stomp::reverb, "REVERB", 4 },
    { doomgazer::Stomp::tremolo, "TREMOLO", 5 },
    { doomgazer::Stomp::glide, "GLIDE", 5 },
    { doomgazer::Stomp::swell, "SWELL", 5 },
    { doomgazer::Stomp::phaser, "PHASER", 5 },
    { doomgazer::Stomp::chorus, "CHORUS", 5 },
    { doomgazer::Stomp::howl, "HOWL", 5 },
    { doomgazer::Stomp::pedal, "PEDAL", 5 },
    { doomgazer::Stomp::gate, "GATE", 6 },
    { doomgazer::Stomp::sustain, "SUSTAIN", 6 },
};

const char* helpFor(int parameter)
{
    switch (parameter)
    {
        case 1: return "Engine. Harmonic core is the single-path amp. Twin core adds the low path, the wall, and carve.";
        case 2: return "Core drive. Gain into the harmonic core. Light picking stays clean; harder playing breaks up.";
        case 3: return "Core level. Output trim for the harmonic core only.";
        case 4: return "Sub weight. Mixes a sub-bass fundamental under the harmonic core.";
        case 5: return "Sub drive. Soft saturation on the octave divider of whichever core is running.";
        case 6: return "Spectrum blend. Moves the balance from the low foundation to the high fuzz wall.";
        case 7: return "Low gain. Drive for the band below the crossover.";
        case 8: return "Sub-octave level. Gain of the octave-down divider on the Twin core low path.";
        case 9: return "Melody carve. Ducks the low path while a melody plays. Twin core only.";
        case 10: return "Carve depth. How far the low path ducks when melody carve is on.";
        case 11: return "Harmonic wall. Gain into the four-stage fuzz above the crossover.";
        case 12: return "Spectral tilt. High-path tone, from thick sludge toward a brighter roar.";
        case 13: return "Input tight. How much of the pre-fuzz high-pass is mixed in.";
        case 14: return "Doubler. Splits the signal in front of the contour and runs a second cabinet beside it.";
        case 15: return "Spread. Zero keeps the original and the second contour centred. Higher settings pan the original left and the second contour right by the same amount.";
        case 16: return "Level. Output level of the second contour only.";
        case 17: return "Doubler polarity. Flips phase on the second contour. Needs the doubler on.";
        case 18: return "Thump. Body resonance of the second contour, centred on its body frequency.";
        case 19: return "Sustain. Turns the leveler on or off. Off, picking stays uncompressed.";
        case 20: return "Sustain depth. How hard the leveler lifts and compresses.";
        case 21: return "Thump. Body resonance of the contour, centred on the body frequency.";
        case 22: return "Mid contour. Scoop or push around 550 Hz.";
        case 23: return "High cut. Steep cutoff at the end of the contour.";
        case 24: return "Master. Final output level before the limiter and the PA mono fold.";
        case 25: return "Controller mode. When on, the preset CC below selects a preset by message value.";
        case 26: return "Preset number. Follows the loaded preset.";
        case 27: return "Preset CC. The controller number that selects a preset when controller mode is on.";
        case 28: return "Sag. Power-supply sag. Harder hits compress and lose a little edge.";
        case 29: return "Anti-fizz. Low-pass between fuzz stages. Lower settings remove harsh fizz.";
        case 30: return "Pre-drive cutoff. Frequency of the tightness high-pass.";
        case 31: return "Split frequency. Crossover between the low path and the harmonic wall.";
        case 32: return "Body frequency. Centre of the contour's low resonance.";
        case 33: return "Presence. Centre of the contour's upper-mid peak.";
        case 34: return "Contour. Turns the body EQ on or off.";
        case 35: return "Core tone. Low cut before the drive and presence after it. Left is full, right is tight.";
        case 36: return "Limiter. Turns the peak limiter at the end of the chain on or off.";
        case 37: return "Delay type. Tape echo or reverse swell. The DELAY button below switches the echo in and out.";
        case 38: return "Delay mix. Wet level of the delay.";
        case 39: return "Delay time. Length of the tape echo or the reverse swell.";
        case 40: return "Delay feedback. Above full scale the repeats can self-oscillate. The limiter holds the peak.";
        case 41: return "Delay position. Pre is before the contour cab; post is after it.";
        case 42: return "Delay damping. Darkens the repeats.";
        case 43: return "Reverb type. Abyss hall, shimmer, or spring. The REVERB button below switches the space in and out.";
        case 44: return "Reverb mix. Wet level of the reverb.";
        case 45: return "Reverb size. Length of the space, from a small room toward a long cavern.";
        case 46: return "Reverb decay. How long the reverb trail hangs on.";
        case 47: return "Reverb position. Pre is before the contour cab; post is after it.";
        case 48: return "Reverb sheen. Darkens the reverb, or opens the shimmer.";
        case 49: return "Atmosphere order. When both run in the same pass: delay first, or reverb first.";
        case 50: return "Tremolo. Turns the tremolo on or off.";
        case 51: return "Tremolo speed. How fast the tremolo cycles.";
        case 52: return "Tremolo depth. How far the level dips on each cycle.";
        case 53: return "Tremolo shape. Tube bias, opto, harmonic, or flutter. Flutter chatters on loud notes and settles as the level falls.";
        case 54: return "Tremolo width. Stereo offset, from a single pulse toward ping-pong.";
        case 55: return "Mute. Silences the main output so you can tune. The scope switches to the dry input while muted.";
        case 56: return "Tremolo mix. Dry through the tremolo, up to fully wet.";
        case 57: return "Sub-octave level. Gain of the octave-down divider on the Twin core low path.";
        case 58: return "Sub-octave tone. Low-pass on the divided octave.";
        case 59: return "Glide wash type. Early reflections or reverse swell. The GLIDE button below switches the wash in and out.";
        case 60: return "Glide mix. How much of the early-reflection or reverse wash is mixed in.";
        case 61: return "Glide position. Pre is in front of the amp; post is on the amp output.";
        case 62: return "Input gate. Turns the quiet floor on the leveler and the sub-octave tracker on or off.";
        case 63: return "Input trim. Pad or boost the guitar before the amp. Turn it down when the chain clips with the interface gain already low.";
        case 64: return "Core octave. The number is the interval in semitones. -12 is one octave down.";
        case 65: return "Twin core octave. The number is the interval in semitones. -12 is one octave down.";
        case 66: return "Freeze. With reverb on, latches the tail so the space keeps blooming after you stop playing.";
        case 67: return "Sync. Snaps delay time and tremolo rate to the host tempo.";
        case 68: return "Sub solo. Fades the dry core or fuzz out until only the divided octave is left.";
        case 69: return "Grit character. Left is the sustained scooped fuzz. Right is the hard mid bark.";
        case 70: return "Grit amount. Mixes the parallel grit path. Zero is dry amp only.";
        case 71: return "Grit frequency. Notch centre on the scooped side, bark centre on the hard side.";
        case 72: return "Grit level. Trim for the grit path. Zero amount ignores it.";
        case 73: return "Phaser rate. How fast the phaser sweeps.";
        case 74: return "Phaser depth. How far the notches travel.";
        case 75: return "Phaser feedback. More regeneration in the sweep.";
        case 76: return "Phaser mix. Zero is dry.";
        case 77: return "Chorus rate. How fast the modulation runs.";
        case 78: return "Chorus mix. Zero is dry.";
        case 79: return "Baritone filter on the wet chorus. Off is full range, on is the baritone voicing.";
        case 80: return "Swell attack. How long the volume fade takes.";
        case 81: return "Swell amount. Fades new notes in. Zero is off unless the swell switch restores it.";
        case 82: return "Howl partial. Picks which harmonic the resonator tracks.";
        case 83: return "Howl amount. Feeds the resonator partial. Zero is off unless the howl switch restores it.";
        case 84: return "Howl sense. How strongly picking excites the resonator.";
        case 85: return "Delay stereo image. Off is mono, on is stereo.";
        case 86: return "Reverb stereo image. Off is mono, on is stereo.";
        case 87: return "Chorus stereo image. Off is mono, on is stereo.";
        case 88: return "Phaser stereo image. Off is mono, on is stereo.";
        case 89: return "Tremolo stereo image. Off is mono, on is stereo.";
        case 90: return "Glide stereo image. Off is mono, on is stereo.";
        case 91: return "Howl stereo image. Off is mono, on is stereo.";
        case 92: return "Contour stereo image. Off is mono, on is stereo.";
        case 93: return "Pedal. Inserts the expression pedal in the chain.";
        case 94: return "Pedal toe. Heel is silence or a dark wah; toe is the saved level or open wah. It never adds gain.";
        case 95: return "Pedal mode. Off is a volume pedal; on is a wah.";
        case 96: return "Twin Level. Output trim for the Twin core, separate from the harmonic core's LEVEL.";
        case 97: return "Scoop. Mid cut or boost of the second contour, around 550 Hz.";
        case 98: return "High cut. Steep cutoff at the end of the second contour.";
        case 99: return "Body frequency. Centre of the second contour's low resonance.";
        case 100: return "Presence. Centre of the second contour's upper-mid peak.";
        default: return "";
    }
}

juce::String captionFor(int parameter)
{
    for (const auto& spec : kSwitchSpecs)
        if (spec.parameter == parameter)
            return spec.caption;
    for (const auto& spec : kComboSpecs)
        if (spec.parameter == parameter)
            return spec.caption;
    return juce::String(doomgazer::parameterName(parameter));
}

void styleButton(juce::TextButton& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, dgui::panel());
    button.setColour(juce::TextButton::buttonOnColourId, accent);
    button.removeColour(juce::TextButton::textColourOffId);
    button.removeColour(juce::TextButton::textColourOnId);
}

void styleEditor(juce::TextEditor& editor)
{
    editor.setColour(juce::TextEditor::backgroundColourId, dgui::panel());
    editor.setColour(juce::TextEditor::textColourId, dgui::text());
    editor.setColour(juce::TextEditor::outlineColourId, dgui::line());
    editor.setColour(juce::TextEditor::focusedOutlineColourId, dgui::amber());
    editor.setColour(juce::TextEditor::highlightColourId, dgui::amber().withAlpha(0.22f));
    editor.setColour(juce::TextEditor::highlightedTextColourId, dgui::text());
    editor.setFont(dgui::font(12.0f));
    editor.setIndents(6, 0);
}

void setPlain(DoomgazerAudioProcessor& processor, const char* id, float plain)
{
    if (auto* parameter = processor.findParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}

} // namespace

StompButton::StompButton(const juce::String& label)
    : juce::Button(label)
{
    setClickingTogglesState(false);
    setWantsKeyboardFocus(false);
}

void StompButton::setBound(bool isBound)
{
    if (bound == isBound)
        return;
    bound = isBound;
    repaint();
}

void StompButton::setLearnArmed(bool isArmed)
{
    if (armed == isArmed)
        return;
    armed = isArmed;
    repaint();
}

void StompButton::setAccent(juce::Colour newAccent)
{
    if (accent == newAccent)
        return;
    accent = newAccent;
    repaint();
}

void StompButton::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown() && onContextMenu)
    {
        onContextMenu();
        return;
    }
    juce::Button::mouseDown(event);
}

void StompButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = 5.0f;
    const bool on = getToggleState();

    g.setColour(on ? accent.withAlpha(0.90f)
                   : (down || highlighted ? dgui::panelHigh() : dgui::panel().withAlpha(0.55f)));
    g.fillRoundedRectangle(area, radius);
    g.setColour(on ? accent.darker(0.22f) : dgui::line());
    g.drawRoundedRectangle(area, radius, 1.0f);

    // The lamp is the same state language as the switches: lit means on. The
    // button never reads as a two-state mode because both labels are not shown.
    const auto lamp = juce::Rectangle<float>(area.getX() + 8.0f,
                                             area.getCentreY() - 3.0f, 6.0f, 6.0f);
    if (on)
    {
        // The state lamp is always red when the action is on, whatever the
        // section colour behind it.
        g.setColour(dgui::red());
        g.fillEllipse(lamp);
    }
    else
    {
        g.setColour(dgui::faint());
        g.drawEllipse(lamp, 1.2f);
    }

    g.setColour(on ? dgui::ink() : dgui::text());
    g.setFont(dgui::trackedFont(9.0f, 0.8f, true));
    g.drawFittedText(getButtonText(),
                     getLocalBounds().withTrimmedLeft(20).withTrimmedRight(bound ? 14 : 4),
                     juce::Justification::centred, 1);

    if (armed)
    {
        g.setColour(dgui::amber().withAlpha(0.9f));
        g.drawRoundedRectangle(area.expanded(1.5f), radius + 1.5f, 1.4f);
    }

    if (bound)
    {
        g.setColour(on ? dgui::ink().withAlpha(0.75f) : accent);
        g.fillEllipse(area.getRight() - 9.0f, area.getY() + 4.0f, 4.0f, 4.0f);
    }
}

// Preset browser ---------------------------------------------------------------

PresetBrowser::PresetBrowser(DoomgazerAudioProcessor& processor)
    : audioProcessor(processor)
{
    setInterceptsMouseClicks(true, true);

    addAndMakeVisible(search);
    styleEditor(search);
    search.setTextToShowWhenEmpty("Search presets", dgui::faint());
    search.setJustification(juce::Justification::centredLeft);
    search.onTextChange = [this] { refresh(); };

    addAndMakeVisible(categoryBox);
    categoryBox.onChange = [this] { refresh(); };

    addAndMakeVisible(pcEditor);
    styleEditor(pcEditor);
    pcEditor.setTextToShowWhenEmpty("---", dgui::faint());
    pcEditor.setJustification(juce::Justification::centred);
    pcEditor.setInputRestrictions(3, "0123456789");
    pcEditor.onReturnKey = [this] { commitProgramCode(); };
    pcEditor.onEscapeKey = [this] { syncProgramEditor(true); };
    pcEditor.onFocusLost = [this] { commitProgramCode(); };

    addAndMakeVisible(list);
    list.setModel(this);
    list.setRowHeight(26);
    list.setOutlineThickness(1);

    struct ButtonSpec
    {
        juce::TextButton* button;
        juce::Colour accent;
        std::function<void()> action;
    };

    const ButtonSpec specs[] = {
        { &loadButton, dgui::amber(), [this] { loadSelected(); } },
        { &saveAsButton, dgui::amber(), [this] { showSaveAs(); } },
        { &renameButton, dgui::steel(), [this] { showRename(); } },
        { &deleteButton, dgui::red(), [this] { deleteSelected(); } },
        { &folderButton, dgui::steel(), [this] {
          // Open the folder and show what is in it, rather than its parent with the
          // folder selected.
          const auto folder = DoomgazerAudioProcessor::presetFolder();
          if (!folder.startAsProcess())
              folder.revealToUser();
      } },
        { &rescanButton, dgui::steel(), [this] {
              audioProcessor.refreshPresets();
              refresh();
              if (onLibraryChanged)
                  onLibraryChanged();
          } },
        { &closeButton, dgui::steel(), [this] { if (onClose) onClose(); } },
        { &commitButton, dgui::amber(), [this] { commitInline(); } },
        { &cancelButton, dgui::steel(), [this] { cancelInline(); } },
    };

    for (const auto& spec : specs)
    {
        addAndMakeVisible(*spec.button);
        styleButton(*spec.button, spec.accent);
        spec.button->onClick = spec.action;
    }

    addAndMakeVisible(nameEditor);
    addAndMakeVisible(categoryEditor);
    addAndMakeVisible(notesEditor);
    styleEditor(nameEditor);
    styleEditor(categoryEditor);
    styleEditor(notesEditor);
    nameEditor.setTextToShowWhenEmpty("Preset name", dgui::faint());
    categoryEditor.setTextToShowWhenEmpty("Category", dgui::faint());
    notesEditor.setTextToShowWhenEmpty("Notes (optional)", dgui::faint());

    nameEditor.onReturnKey = [this] { commitInline(); };
    categoryEditor.onReturnKey = [this] { commitInline(); };
    notesEditor.onReturnKey = [this] { commitInline(); };
    nameEditor.onEscapeKey = [this] { cancelInline(); };
    categoryEditor.onEscapeKey = [this] { cancelInline(); };
    notesEditor.onEscapeKey = [this] { cancelInline(); };
    nameEditor.onTextChange = [this] { overwriteArmed = false; };

    addAndMakeVisible(statusLabel);
    statusLabel.setFont(dgui::font(11.0f));
    statusLabel.setColour(juce::Label::textColourId, dgui::dim());
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setText({}, juce::dontSendNotification);

    rebuildCategories();
    updateEditVisibility();
    refresh();
}

void PresetBrowser::rebuildCategories()
{
    const juce::StringArray categories = audioProcessor.presets().categories();
    if (categories == knownCategories)
        return;

    // Ids are assigned from the sorted list, so a new category shifts them.
    // Remember the name, not the id that used to mean it.
    const int categoryId = categoryBox.getSelectedId();
    const juce::String previous = categoryId > 1 ? categoryBox.getText() : juce::String();
    knownCategories = categories;
    categoryBox.clear(juce::dontSendNotification);
    categoryBox.addItem("All categories", 1);
    for (int index = 0; index < categories.size(); ++index)
        categoryBox.addItem(categories[index], index + 2);

    int selectedId = 1;
    if (previous.isNotEmpty())
    {
        const int found = categories.indexOf(previous, true);
        if (found >= 0)
            selectedId = found + 2;
    }
    categoryBox.setSelectedId(selectedId, juce::dontSendNotification);
}

void PresetBrowser::lookAndFeelChanged()
{
    for (auto* editor : { &search, &pcEditor, &nameEditor, &categoryEditor, &notesEditor })
        styleEditor(*editor);
    for (auto* button : { &loadButton, &saveAsButton, &renameButton, &deleteButton,
                          &folderButton, &rescanButton,
                          &closeButton, &commitButton, &cancelButton })
        styleButton(*button, dgui::amber());
    list.setColour(juce::ListBox::backgroundColourId, dgui::well());
    list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list.repaint();
    repaint();
}

void PresetBrowser::refresh()
{
    // The browser is constructed before the saved theme is applied, so the palette
    // is re-read here every time it opens.
    list.setColour(juce::ListBox::backgroundColourId, dgui::well());
    rebuildCategories();

    const bool reveal = preferredSelection >= 0;
    int keepSelected = reveal ? preferredSelection : selectedEntry();
    preferredSelection = -1;

    if (reveal && keepSelected >= 0)
    {
        const auto& entry = audioProcessor.presets().entry(keepSelected);
        const juce::String queryNow = search.getText().trim();
        const int categoryIdNow = categoryBox.getSelectedId();
        const juce::String categoryNow = categoryIdNow > 1 ? categoryBox.getText()
                                                           : juce::String();
        const bool hiddenBySearch = queryNow.isNotEmpty()
            && !entry.name.containsIgnoreCase(queryNow);
        const bool hiddenByCategory = categoryNow.isNotEmpty()
            && !entry.category.equalsIgnoreCase(categoryNow);
        if (entry.name.isNotEmpty() && (hiddenBySearch || hiddenByCategory))
        {
            search.setText({}, juce::dontSendNotification);
            categoryBox.setSelectedId(1, juce::dontSendNotification);
        }
    }

    const juce::String query = search.getText().trim();
    const int categoryId = categoryBox.getSelectedId();
    // getText() is the selected item. Item ids start at 1, so id - 2 is the
    // previous row ("All categories" when the first real category is selected).
    const juce::String category = categoryId > 1 ? categoryBox.getText() : juce::String();

    const int count = audioProcessor.presets().size();
    visibleEntries.clear();
    visibleEntries.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        const auto& entry = audioProcessor.presets().entry(index);
        if (!query.isEmpty() && !entry.name.containsIgnoreCase(query))
            continue;
        if (category.isNotEmpty() && !entry.category.equalsIgnoreCase(category))
            continue;
        visibleEntries.push_back(index);
    }

    list.updateContent();

    // Keep the row the user picked, then fall back to the loaded preset.
    const auto rowFor = [this](int entryIndex) {
        for (int index = 0; index < static_cast<int>(visibleEntries.size()); ++index)
            if (visibleEntries[static_cast<std::size_t>(index)] == entryIndex)
                return index;
        return -1;
    };

    int row = keepSelected >= 0 ? rowFor(keepSelected) : -1;
    if (row < 0)
        row = rowFor(audioProcessor.currentPresetIndex());

    if (row >= 0)
        list.selectRow(row);
    else
        list.deselectAllRows();

    deleteArmed = false;
    deleteButton.setButtonText("DELETE");
    syncProgramEditor(false);
}

int PresetBrowser::getNumRows()
{
    return static_cast<int>(visibleEntries.size());
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                                     bool rowIsSelected)
{
    if (row < 0 || row >= static_cast<int>(visibleEntries.size()))
        return;

    const int index = visibleEntries[static_cast<std::size_t>(row)];
    const auto& entry = audioProcessor.presets().entry(index);

    g.fillAll(dgui::well());
    if (rowIsSelected)
    {
        g.setColour(dgui::amber().withAlpha(0.14f));
        g.fillRoundedRectangle(1.0f, 1.0f, static_cast<float>(width - 2),
                               static_cast<float>(height - 2), 3.0f);
    }
    else if (row % 2 == 1)
    {
        g.setColour(dgui::panel().withAlpha(0.35f));
        g.fillRect(1, 1, width - 2, height - 2);
    }

    if (entry.factory)
        dgui::drawChip(g, juce::Rectangle<int>(8, (height - 13) / 2, 50, 13), "FACTORY",
                       dgui::blue(), false);
    else
        dgui::drawChip(g, juce::Rectangle<int>(8, (height - 13) / 2, 36, 13), "USER",
                       dgui::green(), false);

    g.setColour(rowIsSelected ? dgui::text() : dgui::text().withAlpha(0.88f));
    g.setFont(dgui::font(12.5f, true));
    g.drawText(entry.name, 66, 0, juce::jmax(0, width - 270), height,
               juce::Justification::centredLeft);

    const int code = audioProcessor.presets().programCode(index);
    g.setColour(code < 0 ? dgui::faint() : dgui::dim());
    g.setFont(dgui::font(10.5f));
    g.drawText(code < 0 ? "---" : juce::String(code).paddedLeft('0', 3),
               width - 196, 0, 44, height, juce::Justification::centredRight);

    g.setColour(dgui::dim());
    g.setFont(dgui::font(10.5f));
    g.drawText(entry.category, width - 148, 0, 140, height, juce::Justification::centredRight);
}

void PresetBrowser::selectedRowsChanged(int row)
{
    selectedRow = row;
    deleteArmed = false;
    deleteButton.setButtonText("DELETE");
    if (pcEditor.hasKeyboardFocus(true) && programEditIndex >= 0
        && programEditIndex != selectedEntry())
        commitProgramCode();
    syncProgramEditor(false);
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= static_cast<int>(visibleEntries.size()))
        return;
    selectedRow = row;
    loadSelected();
}

bool PresetBrowser::handleOverlayKey(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (mode != EditorMode::none)
            cancelInline();
        else if (onClose)
            onClose();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        if (mode != EditorMode::none)
            commitInline();
        else
            loadSelected();
        return true;
    }

    return false;
}

void PresetBrowser::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    dgui::drawFloatingCard(g, area);

    const auto titleArea = area.reduced(16).removeFromTop(20);
    g.setColour(dgui::text());
    g.setFont(dgui::trackedFont(13.0f, 2.6f, true));
    g.drawText("PRESETS", titleArea, juce::Justification::centredLeft);

    int factoryCount = 0;
    for (int index = 0; index < audioProcessor.presets().size(); ++index)
        if (audioProcessor.presets().entry(index).factory)
            ++factoryCount;
    const int userCount = audioProcessor.presets().size() - factoryCount;

    g.setColour(dgui::faint());
    g.setFont(dgui::trackedFont(9.0f, 1.2f));
    g.drawText(juce::String(factoryCount) + " FACTORY   " + juce::String(userCount) + " USER",
               titleArea.withTrimmedRight(80), juce::Justification::centredRight);

    g.setColour(dgui::dim());
    g.setFont(dgui::font(11.0f));
    g.drawFittedText(hoverText, hintRowBounds, juce::Justification::centredLeft, 1);
}

void PresetBrowser::pollHoverHint()
{
    struct Item { juce::Component* component; const char* text; };
    const Item items[] = {
        { &search, "Filter presets by name." },
        { &pcEditor, "Program change for the selected preset. Blank (---) skips PC switching." },
        { &categoryBox, "Show one category only." },
        { &list, "Click to select, double-click to load." },
        { &loadButton, "Load the selected preset." },
        { &saveAsButton, "Save the current sound as a new preset." },
        { &renameButton, "Rename the selected user preset." },
        { &deleteButton, "Delete the selected user preset. Press twice to confirm." },
        { &folderButton, "Open the preset folder on disk." },
        { &rescanButton, "Reload the library from disk." },
        { &closeButton, "Close the browser." },
        { &nameEditor, "Preset name." },
        { &categoryEditor, "Category shown in the list." },
        { &notesEditor, "Notes, shown when hovering the preset bar." },
        { &commitButton, "Save the preset." },
        { &cancelButton, "Cancel." },
    };

    const auto mouse = getMouseXYRelative();
    juce::String next = "Hover a control to see what it does.";
    for (const auto& item : items)
        if (item.component->isVisible() && item.component->getBounds().contains(mouse))
            next = item.text;

    if (next == hoverText)
        return;
    hoverText = next;
    repaint(hintRowBounds);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto title = area.removeFromTop(20);
    closeButton.setBounds(title.removeFromRight(66).withSizeKeepingCentre(66, 20));

    area.removeFromTop(8);
    auto searchRow = area.removeFromTop(26);
    categoryBox.setBounds(searchRow.removeFromRight(150));
    searchRow.removeFromRight(6);
    pcEditor.setBounds(searchRow.removeFromRight(52));
    searchRow.removeFromRight(8);
    search.setBounds(searchRow);

    area.removeFromTop(8);
    hintRowBounds = area.removeFromBottom(18);
    area.removeFromBottom(2);
    statusLabel.setBounds(area.removeFromBottom(16));
    area.removeFromBottom(6);

    auto editRow = area.removeFromBottom(26);
    area.removeFromBottom(6);
    auto buttonRow = area.removeFromBottom(26);
    area.removeFromBottom(8);

    list.setBounds(area);

    auto placeRow = [](juce::Rectangle<int> row,
                       std::initializer_list<juce::TextButton*> buttons) {
        const int gap = 6;
        const int count = static_cast<int>(buttons.size());
        const int width = (row.getWidth() - gap * (count - 1)) / count;
        int index = 0;
        for (auto* button : buttons)
        {
            if (index == count - 1)
                button->setBounds(row);
            else
            {
                button->setBounds(row.removeFromLeft(width));
                row.removeFromLeft(gap);
            }
            ++index;
        }
    };

    placeRow(buttonRow, { &loadButton, &saveAsButton, &renameButton, &deleteButton,
                          &folderButton, &rescanButton });

    auto inlineArea = editRow;
    cancelButton.setBounds(inlineArea.removeFromRight(66));
    inlineArea.removeFromRight(5);
    commitButton.setBounds(inlineArea.removeFromRight(58));
    inlineArea.removeFromRight(6);
    const int inlineWidth = inlineArea.getWidth();
    nameEditor.setBounds(inlineArea.removeFromLeft(inlineWidth * 38 / 100));
    inlineArea.removeFromLeft(5);
    categoryEditor.setBounds(inlineArea.removeFromLeft(inlineWidth * 24 / 100));
    inlineArea.removeFromLeft(5);
    notesEditor.setBounds(inlineArea);

    updateEditVisibility();
}

void PresetBrowser::updateEditVisibility()
{
    const bool editing = mode != EditorMode::none;
    nameEditor.setVisible(editing);
    categoryEditor.setVisible(mode == EditorMode::saveAs);
    notesEditor.setVisible(mode == EditorMode::saveAs);
    commitButton.setVisible(editing);
    cancelButton.setVisible(editing);

    loadButton.setVisible(!editing);
    saveAsButton.setVisible(!editing);
    renameButton.setVisible(!editing);
    deleteButton.setVisible(!editing);
    folderButton.setVisible(!editing);
    rescanButton.setVisible(!editing);

    commitButton.setButtonText(mode == EditorMode::rename ? "RENAME" : "SAVE");
}

void PresetBrowser::syncProgramEditor(bool force)
{
    if (committingProgram && !force)
        return;
    const int index = selectedEntry();
    if (!force && pcEditor.hasKeyboardFocus(true) && programEditIndex == index)
        return;

    fillingProgramEditor = true;
    programEditIndex = index;
    pcEditor.setEnabled(index >= 0);
    if (index < 0)
        pcEditor.setText({}, juce::dontSendNotification);
    else
    {
        const int code = audioProcessor.presets().programCode(index);
        pcEditor.setText(code < 0 ? juce::String() : juce::String(code),
                         juce::dontSendNotification);
    }
    fillingProgramEditor = false;
}

void PresetBrowser::commitProgramCode()
{
    if (fillingProgramEditor || committingProgram)
        return;

    const int index = programEditIndex;
    if (index < 0 || index >= audioProcessor.presets().size())
        return;

    const juce::String text = pcEditor.getText().trim();
    int code = -1;
    if (text.isNotEmpty())
    {
        if (!text.containsOnly("0123456789"))
        {
            setStatus("PC is 0-127, or blank", true);
            syncProgramEditor(true);
            return;
        }
        code = text.getIntValue();
        if (code > 127)
        {
            setStatus("PC is 0-127, or blank", true);
            syncProgramEditor(true);
            return;
        }
    }

    if (audioProcessor.presets().programCode(index) == code)
        return;

    committingProgram = true;
    juce::String error;
    const bool saved = audioProcessor.setPresetProgram(index, code, error);
    committingProgram = false;
    if (!saved)
    {
        setStatus(error.isEmpty() ? "Could not set the PC" : error, true);
        syncProgramEditor(true);
        return;
    }

    setStatus(code < 0 ? "PC cleared" : "PC " + juce::String(code), false);
    refresh();
    syncProgramEditor(true);
}

int PresetBrowser::selectedEntry() const
{
    if (selectedRow < 0 || selectedRow >= static_cast<int>(visibleEntries.size()))
        return -1;
    return visibleEntries[static_cast<std::size_t>(selectedRow)];
}

void PresetBrowser::loadSelected()
{
    const int index = selectedEntry();
    if (index < 0)
    {
        setStatus("Select a preset first", true);
        return;
    }
    audioProcessor.loadPreset(index);
    setStatus("Loaded " + audioProcessor.currentPresetName(), false);
    if (onLibraryChanged)
        onLibraryChanged();
    refresh();
}

void PresetBrowser::showSaveAs()
{
    mode = EditorMode::saveAs;
    overwriteArmed = false;

    const int index = selectedEntry();
    if (index >= 0)
    {
        const auto& entry = audioProcessor.presets().entry(index);
        nameEditor.setText(entry.name, false);
        categoryEditor.setText(entry.category, false);
        notesEditor.setText(entry.notes, false);
    }
    else
    {
        nameEditor.setText(audioProcessor.currentPresetName(), false);
        categoryEditor.setText(audioProcessor.currentPresetCategory(), false);
        notesEditor.setText(audioProcessor.currentPresetNotes(), false);
    }

    nameEditor.selectAll();
    updateEditVisibility();
    resized();
    nameEditor.grabKeyboardFocus();
    setStatus("Name the preset, then press Save. Existing names are not overwritten by accident.",
              false);
}

void PresetBrowser::showRename()
{
    const int index = selectedEntry();
    if (index < 0)
    {
        setStatus("Select a preset first", true);
        return;
    }
    if (audioProcessor.presets().entry(index).factory)
    {
        setStatus("Factory presets cannot be renamed; use Save As", true);
        return;
    }

    mode = EditorMode::rename;
    overwriteArmed = false;
    nameEditor.setText(audioProcessor.presets().entry(index).name, false);
    nameEditor.selectAll();
    updateEditVisibility();
    resized();
    nameEditor.grabKeyboardFocus();
}

void PresetBrowser::commitInline()
{
    juce::String error;

    if (mode == EditorMode::rename)
    {
        const int index = selectedEntry();
        if (index < 0)
        {
            setStatus("Select a preset first", true);
            return;
        }
        if (!audioProcessor.renamePreset(index, nameEditor.getText(), error))
        {
            setStatus(error, true);
            return;
        }
        // The stored name is sanitised, so the text that was typed can miss the row.
        const juce::String stored = juce::String(doomgazer::sanitisePresetText(
            nameEditor.getText().trim().toStdString(), 64));
        const int renamed = audioProcessor.presets().indexOfName(stored, false);
        preferredSelection = renamed >= 0 ? renamed : audioProcessor.currentPresetIndex();
        setStatus("Renamed", false);
    }
    else
    {
        if (!audioProcessor.savePresetCopy(nameEditor.getText(), categoryEditor.getText(),
                                           notesEditor.getText(), overwriteArmed, error))
        {
            if (error.containsIgnoreCase("already exists"))
            {
                overwriteArmed = true;
                setStatus(error + " - press SAVE again to replace it", true);
            }
            else
            {
                setStatus(error, true);
            }
            return;
        }
        // The row that was selected is still the one LOAD would use. Point it at
        // the preset that was just written, and scroll that row into view.
        preferredSelection = audioProcessor.currentPresetIndex();
        setStatus("Saved " + audioProcessor.currentPresetName(), false);
    }

    mode = EditorMode::none;
    overwriteArmed = false;
    updateEditVisibility();
    resized();
    if (onLibraryChanged)
        onLibraryChanged();
    refresh();
}

void PresetBrowser::cancelInline()
{
    mode = EditorMode::none;
    overwriteArmed = false;
    updateEditVisibility();
    resized();
    setStatus("Cancelled", false);
}

void PresetBrowser::deleteSelected()
{
    const int index = selectedEntry();
    if (index < 0)
    {
        setStatus("Select a preset first", true);
        return;
    }
    if (audioProcessor.presets().entry(index).factory)
    {
        setStatus("Factory presets cannot be deleted", true);
        return;
    }
    if (!deleteArmed)
    {
        deleteArmed = true;
        deleteButton.setButtonText("CONFIRM");
        setStatus("Press CONFIRM to delete " + audioProcessor.presets().entry(index).name, true);
        return;
    }

    deleteArmed = false;
    deleteButton.setButtonText("DELETE");

    juce::String error;
    if (!audioProcessor.deletePreset(index, error))
    {
        setStatus(error, true);
        return;
    }
    setStatus("Deleted", false);
    if (onLibraryChanged)
        onLibraryChanged();
    refresh();
}

void PresetBrowser::setStatus(const juce::String& message, bool isError)
{
    statusLabel.setText(message, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, isError ? dgui::red() : dgui::dim());
}

// MIDI page --------------------------------------------------------------------

MidiPage::MidiPage(DoomgazerAudioProcessor& processor)
    : audioProcessor(processor)
{
    setInterceptsMouseClicks(true, true);

    addAndMakeVisible(channelLabel);
    channelLabel.setText("CHANNEL", juce::dontSendNotification);
    channelLabel.setFont(dgui::trackedFont(9.0f, 1.4f, true));
    channelLabel.setColour(juce::Label::textColourId, dgui::dim());
    channelLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(programLabel);
    programLabel.setText("PROGRAM CHANGE", juce::dontSendNotification);
    programLabel.setFont(dgui::trackedFont(9.0f, 1.4f, true));
    programLabel.setColour(juce::Label::textColourId, dgui::dim());
    programLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(channelBox);
    channelBox.addItem("Any", 1);
    for (int channel = 1; channel <= 16; ++channel)
        channelBox.addItem(juce::String(channel), channel + 1);
    channelBox.setSelectedId(1, juce::dontSendNotification);
    channelBox.onChange = [this] {
        audioProcessor.setMidiChannel(channelBox.getSelectedId() - 1);
        refresh();
    };

    struct SimpleButton
    {
        juce::TextButton* button;
        juce::Colour accent;
        std::function<void()> action;
    };

    const SimpleButton buttons[] = {
        { &programChange, dgui::green(), [this] {
              audioProcessor.setProgramChangeEnabled(programChange.getToggleState());
              refresh();
          } },
        { &clearAll, dgui::red(), [this] {
              audioProcessor.clearAllMappings();
              for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
                  audioProcessor.clearStompBinding(stomp);
              refresh();
          } },
        { &closeButton, dgui::steel(), [this] { if (onClose) onClose(); } },
    };

    for (const auto& spec : buttons)
    {
        addAndMakeVisible(*spec.button);
        styleButton(*spec.button, spec.accent);
        spec.button->onClick = spec.action;
        spec.button->getProperties().set("strong", true);
    }
    programChange.setClickingTogglesState(true);

    // The two controller settings live here rather than on a page: they are part
    // of the MIDI map, not part of the sound.
    addAndMakeVisible(presetCcLabel);
    presetCcLabel.setText("PRESET CC", juce::dontSendNotification);
    presetCcLabel.setFont(dgui::trackedFont(9.0f, 1.4f, true));
    presetCcLabel.setColour(juce::Label::textColourId, dgui::dim());
    presetCcLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(presetCcBar);
    presetCcBar.setSliderStyle(juce::Slider::LinearBar);
    presetCcBar.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 44, 22);
    presetCcAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.stateFor(doomgazer::parameterId(doomgazer::kPresetCcParameter)),
        doomgazer::parameterId(doomgazer::kPresetCcParameter),
        presetCcBar);

    addAndMakeVisible(selectByCc);
    styleButton(selectByCc, dgui::amber());
    selectByCc.setClickingTogglesState(false);
    selectByCc.getProperties().set("strong", true);
    selectByCc.onClick = [this] {
        const float plain = audioProcessor.findRawValue("controllerMode")->load();
        setPlain(audioProcessor, "controllerMode", plain > 0.5f ? 0.0f : 1.0f);
        refresh();
    };

    // One row per host parameter, then one per action-row button.
    rows.reserve(static_cast<std::size_t>(doomgazer::kParameterCount + doomgazer::kStompCount));
    for (int parameter = 1; parameter <= doomgazer::kParameterCount; ++parameter)
    {
        Row row;
        row.parameter = parameter;
        rows.push_back(row);
    }
    for (const auto& spec : kStripSpecs)
    {
        Row row;
        row.stomp = static_cast<int>(spec.stomp);
        rows.push_back(row);
    }

    // The preset steps are commands rather than actions, but they are bound the
    // same way and shown in the same list.
    for (const auto stomp : { doomgazer::Stomp::presetPrev, doomgazer::Stomp::presetNext })
    {
        Row row;
        row.stomp = static_cast<int>(stomp);
        rows.push_back(row);
    }

    refresh();
}

void MidiPage::layoutRows()
{
    const int count = static_cast<int>(rows.size());
    const int columns = 5;
    const int columnRows = (count + columns - 1) / columns;
    if (gridArea.getWidth() < 60 || gridArea.getHeight() < 24)
        return;
    const int columnWidth = (gridArea.getWidth() - 14 * (columns - 1)) / columns;
    const int rowHeight = juce::jmax(16, gridArea.getHeight() / juce::jmax(1, columnRows));

    for (int index = 0; index < count; ++index)
    {
        const int column = index / columnRows;
        const int row = index % columnRows;
        auto cell = juce::Rectangle<int>(gridArea.getX() + column * (columnWidth + 14),
                                         gridArea.getY() + row * rowHeight, columnWidth,
                                         juce::jmin(rowHeight, gridArea.getBottom()
                                                         - (gridArea.getY() + row * rowHeight)));
        auto line = cell.reduced(0, 1);
        auto& entry = rows[static_cast<std::size_t>(index)];
        entry.clear = line.removeFromRight(18);
        line.removeFromRight(4);
        entry.learn = line.removeFromRight(48);
        line.removeFromRight(6);
        entry.area = line;
    }
}

juce::String MidiPage::rowName(const Row& row) const
{
    if (row.stomp >= 0)
        return juce::String(doomgazer::kStompName[row.stomp])
            + (row.stomp >= static_cast<int>(doomgazer::Stomp::presetPrev) ? " (PRESET)"
                                                                          : " (ACTION)");
    return juce::String(doomgazer::parameterName(row.parameter));
}

juce::String MidiPage::rowMapping(const Row& row) const
{
    if (row.stomp >= 0)
        return audioProcessor.stompBindingLabel(row.stomp);
    return audioProcessor.mappingLabel(row.parameter);
}

void MidiPage::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    dgui::drawFloatingCard(g, area);

    auto inner = area.reduced(16);
    auto title = inner.removeFromTop(20);
    g.setColour(dgui::text());
    g.setFont(dgui::trackedFont(13.0f, 2.6f, true));
    g.drawText("MIDI CONTROL", title, juce::Justification::centredLeft);

    int mapped = 0;
    for (int parameter = 1; parameter <= doomgazer::kParameterCount; ++parameter)
        if (audioProcessor.ccForParameter(parameter) >= 0)
            ++mapped;
    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
        if (audioProcessor.stompIsBound(stomp))
            ++mapped;

    g.setColour(dgui::faint());
    g.setFont(dgui::trackedFont(9.0f, 1.2f));
    g.drawText(juce::String(mapped) + " MAPPED", title, juce::Justification::centredRight);

    const int learning = audioProcessor.learnTarget();
    const int armedStomp = audioProcessor.stompLearnTarget();

    for (int index = 0; index < static_cast<int>(rows.size()); ++index)
    {
        const auto& row = rows[static_cast<std::size_t>(index)];
        const bool stompRow = row.stomp >= 0;
        const bool bound = stompRow ? audioProcessor.stompIsBound(row.stomp)
                                    : audioProcessor.ccForParameter(row.parameter) >= 0;
        const bool isLearning = stompRow ? armedStomp == row.stomp
                                         : learning == row.parameter;

        if (index == hoverRow)
        {
            g.setColour(dgui::text().withAlpha(0.05f));
            g.fillRoundedRectangle(row.area.toFloat().expanded(2.0f, 0.0f), 3.0f);
        }

        g.setColour(isLearning ? dgui::amber() : (stompRow ? dgui::dim() : dgui::text()));
        g.setFont(dgui::font(10.5f));
        g.drawFittedText(rowName(row), row.area, juce::Justification::centredLeft, 1);

        const juce::String mapping = rowMapping(row);
        g.setColour(bound ? dgui::amber() : dgui::faint());
        g.setFont(dgui::font(10.0f));
        g.drawText(mapping.isEmpty() ? "--" : mapping, row.learn.getX() - 56, row.area.getY(),
                   52, row.area.getHeight(), juce::Justification::centredRight);

        g.setColour(isLearning ? dgui::amber() : (bound ? dgui::text() : dgui::dim()));
        g.setFont(dgui::trackedFont(9.0f, 1.0f, true));
        g.drawText(isLearning ? "WAIT" : "LEARN", row.learn, juce::Justification::centred);

        g.setColour(bound ? dgui::red() : dgui::faint().withAlpha(0.4f));
        g.setFont(dgui::font(14.0f, true));
        g.drawText(juce::String::charToString(0x00d7), row.clear,
                   juce::Justification::centred);
    }

    g.setColour(dgui::dim());
    g.setFont(dgui::font(11.0f));
    g.drawFittedText(hoverText, hintRowBounds, juce::Justification::centredLeft, 1);
}

void MidiPage::pollHoverHint()
{
    const auto mouse = getMouseXYRelative();
    juce::String next = "Hover a control to see what it does.";

    if (channelBox.getBounds().contains(mouse))
        next = "Only respond on this MIDI channel. Any listens to all of them.";
    else if (programChange.getBounds().contains(mouse) || programLabel.getBounds().contains(mouse))
        next = "When on, a MIDI program change loads the preset assigned that PC. Blank skips it.";
    else if (clearAll.getBounds().contains(mouse))
        next = "Remove every mapping and every action-button binding.";
    else if (closeButton.getBounds().contains(mouse))
        next = "Close the MIDI page.";
    else if (presetCcBar.getBounds().contains(mouse) || presetCcLabel.getBounds().contains(mouse))
        next = "The controller number that selects a preset when SELECT BY CC is on.";
    else if (selectByCc.getBounds().contains(mouse))
        next = "When on, the preset CC selects a preset by its message value.";
    else
    {
        for (const auto& row : rows)
        {
            if (row.learn.contains(mouse))
            {
                next = row.stomp >= 0 ? "Press, then move a knob or move a controller."
                                      : "Press, then move a knob or pedal to bind it.";
                break;
            }
            if (row.clear.contains(mouse))
            {
                next = "Forget this mapping.";
                break;
            }
            if (row.area.contains(mouse))
            {
                next = row.stomp >= 0
                    ? "The MIDI binding that toggles this action."
                    : "The parameter this row maps.";
                break;
            }
        }
    }

    if (next == hoverText)
        return;
    hoverText = next;
    repaint(hintRowBounds);
}

void MidiPage::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto title = area.removeFromTop(20);
    closeButton.setBounds(title.removeFromRight(66).withSizeKeepingCentre(66, 20));

    area.removeFromTop(8);
    auto top = area.removeFromTop(26);
    channelLabel.setBounds(top.removeFromLeft(58));
    channelBox.setBounds(top.removeFromLeft(78));
    top.removeFromLeft(12);
    programLabel.setBounds(top.removeFromLeft(112));
    top.removeFromLeft(2);
    programChange.setBounds(top.removeFromLeft(46));
    top.removeFromLeft(10);
    clearAll.setBounds(top.removeFromLeft(84));

    top.removeFromLeft(16);
    selectByCc.setBounds(top.removeFromRight(100).withSizeKeepingCentre(100, 24));
    top.removeFromRight(8);
    presetCcBar.setBounds(top.removeFromRight(92).withSizeKeepingCentre(92, 22));
    top.removeFromRight(6);
    presetCcLabel.setBounds(top.removeFromRight(70));

    area.removeFromTop(8);
    hintRowBounds = area.removeFromBottom(20);
    area.removeFromBottom(2);
    gridArea = area;

    layoutRows();
}

void MidiPage::lookAndFeelChanged()
{
    for (auto* button : { &programChange, &clearAll, &closeButton, &selectByCc })
        styleButton(*button, dgui::amber());
    channelLabel.setColour(juce::Label::textColourId, dgui::dim());
    programLabel.setColour(juce::Label::textColourId, dgui::dim());
    presetCcLabel.setColour(juce::Label::textColourId, dgui::dim());
    presetCcBar.setColour(juce::Slider::textBoxTextColourId, dgui::text());
    presetCcBar.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    presetCcBar.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    repaint();
}

void MidiPage::refresh()
{
    channelBox.setSelectedId(audioProcessor.midiChannel() + 1, juce::dontSendNotification);

    // The label names the setting, the pill carries its value.
    const bool programOn = audioProcessor.programChangeEnabled();
    programChange.setToggleState(programOn, juce::dontSendNotification);
    programChange.setButtonText(programOn ? "ON" : "OFF");

    const float byCc = audioProcessor.findRawValue("controllerMode")->load();
    selectByCc.setToggleState(byCc > 0.5f, juce::dontSendNotification);
    selectByCc.setButtonText(byCc > 0.5f ? "BY CC" : "SELECT BY CC");

    repaint();
}

int MidiPage::rowAt(juce::Point<int> position) const
{
    for (int index = 0; index < static_cast<int>(rows.size()); ++index)
        if (rows[static_cast<std::size_t>(index)].area.expanded(4, 0).contains(position)
            || rows[static_cast<std::size_t>(index)].learn.contains(position)
            || rows[static_cast<std::size_t>(index)].clear.contains(position))
            return index;
    return -1;
}

void MidiPage::mouseMove(const juce::MouseEvent& event)
{
    const int row = rowAt(event.getPosition());
    if (row == hoverRow)
        return;
    hoverRow = row;
    repaint();
}


void MidiPage::mouseDown(const juce::MouseEvent& event)
{
    const int index = rowAt(event.getPosition());
    if (index < 0)
        return;

    const auto& row = rows[static_cast<std::size_t>(index)];
    const auto position = event.getPosition();

    if (row.clear.contains(position))
    {
        if (row.stomp >= 0)
            audioProcessor.clearStompBinding(row.stomp);
        else
            audioProcessor.clearMapping(row.parameter);
        refresh();
        return;
    }

    if (row.learn.contains(position))
    {
        if (row.stomp >= 0)
            audioProcessor.beginStompLearn(row.stomp);
        else
            audioProcessor.beginLearn(row.parameter);
        refresh();
    }
}

// Editor -----------------------------------------------------------------------

DoomgazerAudioProcessorEditor::DoomgazerAudioProcessorEditor(DoomgazerAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor),
      scope(processor),
      spectrum(processor),
      tuner(processor),
      levelMeter(processor),
      browser(processor),
      midiPage(processor)
{
    // A fresh project has no saved theme and opens dark. An explicit ui.theme,
    // or the older ui.dark flag, still reopens on the choice stored in the project.
    {
        const int saved = static_cast<int>(
            audioProcessor.apvts.state.getProperty("ui.theme", -1));
        if (saved >= 0)
            dgui::setTheme(saved >= 1 ? dgui::Theme::dark : dgui::Theme::light);
        else if (audioProcessor.apvts.state.hasProperty("ui.dark"))
            dgui::setTheme(static_cast<bool>(audioProcessor.apvts.state.getProperty("ui.dark"))
                               ? dgui::Theme::dark
                               : dgui::Theme::light);
        else
            dgui::setTheme(dgui::Theme::dark);
    }

    setLookAndFeel(&lookAndFeel);
    lookAndFeel.updateColours();
    sendLookAndFeelChange();
    setResizable(true, true);
    setResizeLimits(1080, 540, 2000, 1400);
    setOpaque(true);
    setWantsKeyboardFocus(true);

    for (const auto& spec : kKnobSpecs)
    {
        auto cell = std::make_unique<dgui::KnobCell>(
            audioProcessor, spec.parameter, spec.caption,
            kBandRole[kModules[spec.module].band]);
        cell->onHintLock = [this](int parameter) {
            hintLock = parameter;
            updateHint();
        };
        addChildComponent(*cell);
        knobCells.push_back(cell.get());
        cells.push_back(std::move(cell));
    }

    for (const auto& spec : kSwitchSpecs)
    {
        auto cell = std::make_unique<dgui::SwitchCell>(audioProcessor, spec.parameter,
                                                       spec.caption);
        cell->setAccent(dgui::accentFor(kBandRole[kModules[spec.module].band]));
        cell->setInlineCaption(spec.inlineCaption);
        cell->setCaptionAbove(!spec.inlineCaption);
        addChildComponent(*cell);
        switchCells.push_back(cell.get());
        cells.push_back(std::move(cell));
    }

    for (const auto& spec : kSelectorSpecs)
    {
        auto cell = std::make_unique<dgui::SelectorCell>(audioProcessor, spec.parameter,
                                                         spec.caption, spec.first, spec.second);
        cell->setAccent(dgui::accentFor(kBandRole[kModules[spec.module].band]));
        addChildComponent(*cell);
        selectorCells.push_back(cell.get());
        cells.push_back(std::move(cell));
    }

    for (const auto& spec : kOrderSpecs)
    {
        auto cell = std::make_unique<dgui::OrderCell>(audioProcessor, spec.parameter, spec.first,
                                                     spec.second);
        cell->setAccent(dgui::accentFor(kBandRole[kModules[spec.module].band]));
        addChildComponent(*cell);
        orderCells.push_back(cell.get());
        cells.push_back(std::move(cell));
    }

    for (const auto& spec : kComboSpecs)
    {
        auto box = std::make_unique<ModeBox>();
        const juce::StringArray items = juce::StringArray::fromTokens(spec.items, "|", "");
        box->addItemList(items, 1);
        const int parameter = spec.parameter;
        const int firstValue = spec.firstValue;
        box->onChange = [this, box = box.get(), parameter, firstValue] {
            if (box->getSelectedId() <= 0)
                return;
            setPlain(audioProcessor, doomgazer::parameterId(parameter),
                     static_cast<float>(box->getSelectedId() - 1 + firstValue));
        };
        box->onContextMenu = [this, box = box.get(), parameter] {
            juce::PopupMenu menu;
            menu.addSectionHeader(juce::String(doomgazer::parameterName(parameter)));
            menu.addItem(1, "MIDI Learn");
            const int cc = audioProcessor.ccForParameter(parameter);
            menu.addItem(2, cc >= 0 ? "Clear MIDI (" + juce::String(cc) + ")" : "Clear MIDI",
                         cc >= 0);
            menu.addSeparator();
            menu.addItem(3, "Reset to default");
            juce::Component::SafePointer<ModeBox> safe(box);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(box),
                               [this, safe, parameter](int result) {
                                   if (safe == nullptr || result == 0)
                                       return;
                                   if (result == 1)
                                       audioProcessor.beginLearn(parameter);
                                   else if (result == 2)
                                       audioProcessor.clearMapping(parameter);
                                   else if (result == 3)
                                   {
                                       if (auto* p = audioProcessor.findParameter(
                                               doomgazer::parameterId(parameter)))
                                           p->setValueNotifyingHost(p->getDefaultValue());
                                   }
                               });
        };
        addChildComponent(*box);
        modeBoxes.push_back(std::move(box));
        modeBoxParameter.push_back(spec.parameter);
        modeBoxFirstValue.push_back(spec.firstValue);
        modeBoxLayout.push_back(spec.caption);
        modeBoxModule.push_back(spec.module);
    }

    for (const auto& spec : kStripSpecs)
    {
        auto button = std::make_unique<StompButton>(spec.label);
        const int stomp = static_cast<int>(spec.stomp);
        button->setAccent(dgui::accentFor(kBandRole[spec.band]));
        button->onClick = [this, stomp] { audioProcessor.toggleStomp(stomp); };
        button->onContextMenu = [this, button = button.get(), stomp] {
            juce::PopupMenu menu;
            menu.addSectionHeader(juce::String(doomgazer::kStompName[stomp]));
            menu.addItem(1, "MIDI Learn");
            const bool bound = audioProcessor.stompIsBound(stomp);
            menu.addItem(2, bound ? "Clear MIDI (" + audioProcessor.stompBindingLabel(stomp) + ")"
                                  : "Clear MIDI",
                         bound);
            juce::Component::SafePointer<StompButton> safe(button);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(button),
                               [this, safe, stomp](int result) {
                                   if (safe == nullptr || result == 0)
                                       return;
                                   if (result == 1)
                                       audioProcessor.beginStompLearn(stomp);
                                   else if (result == 2)
                                       audioProcessor.clearStompBinding(stomp);
                                   refreshStomps();
                               });
        };
        addAndMakeVisible(*button);
        stripButtons.push_back(std::move(button));
    }

    engineSelector = std::make_unique<dgui::SelectorCell>(audioProcessor, 1, "", "CORE", "TWIN");
    engineSelector->setAccents(dgui::accentFor(kBandRole[0]), dgui::accentFor(kBandRole[1]));
    addAndMakeVisible(*engineSelector);

    masterCell = std::make_unique<dgui::KnobCell>(audioProcessor, 24, "MASTER",
                                                  dgui::AccentRole::gold);
    addAndMakeVisible(*masterCell);
    limitCell = std::make_unique<dgui::SwitchCell>(audioProcessor, 36, "LIMIT");
    limitCell->setAccent(dgui::red());
    addAndMakeVisible(*limitCell);
    muteCell = std::make_unique<dgui::SwitchCell>(audioProcessor, 55, "MUTE");
    muteCell->setAccent(dgui::red());
    addAndMakeVisible(*muteCell);

    for (int band = 0; band < kBandCount; ++band)
    {
        auto button = std::make_unique<juce::TextButton>(kBandName[band]);
        styleButton(*button, dgui::accentFor(kBandRole[band]));
        button->setColour(juce::TextButton::textColourOffId,
                          dgui::accentFor(kBandRole[band]).withAlpha(0.95f));
        button->getProperties().set("strong", true);
        button->onClick = [this, band] { showBand(band); };
        addAndMakeVisible(*button);
        bandButtons.push_back(std::move(button));
    }

    for (int module = 0; module < kModuleCount; ++module)
    {
        auto button = std::make_unique<juce::TextButton>(kModules[module].name);
        // The chip wears its band's colour, and the same colour frames the
        // module it opens.
        const auto accent = dgui::accentFor(kBandRole[kModules[module].band]);
        button->setColour(juce::TextButton::buttonColourId, dgui::panelHigh());
        button->setColour(juce::TextButton::buttonOnColourId, accent);
        button->setColour(juce::TextButton::textColourOffId, accent.withAlpha(0.95f));
        button->removeColour(juce::TextButton::textColourOnId);
        button->getProperties().set("chip", true);
        button->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        button->onClick = [this, module] { showModule(module); };
        addChildComponent(*button);
        moduleButtons.push_back(std::move(button));
    }

    addAndMakeVisible(scope);
    addAndMakeVisible(spectrum);
    addAndMakeVisible(tuner);
    addAndMakeVisible(levelMeter);
    addAndMakeVisible(hint);
    hint.setCompact(true);
    addAndMakeVisible(midiLed);
    addAndMakeVisible(presetBar);
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(revertButton);
    addAndMakeVisible(themeButton);
    addAndMakeVisible(compareButton);
    addAndMakeVisible(midiButton);
    addChildComponent(paOutputButton);
    addChildComponent(paInvertRButton);

    styleButton(prevButton, dgui::steel());
    styleButton(nextButton, dgui::steel());
    styleButton(saveButton, dgui::amber());
    styleButton(revertButton, dgui::steel());
    styleButton(compareButton, dgui::green());
    styleButton(midiButton, dgui::blue());
    styleButton(presetBar, dgui::amber());
    styleButton(paOutputButton, dgui::steel());
    styleButton(paInvertRButton, dgui::steel());
    paOutputButton.setClickingTogglesState(false);
    paInvertRButton.setClickingTogglesState(true);
    paOutputButton.onClick = [this] {
        audioProcessor.setOutputMono(!audioProcessor.outputMono());
        refreshPresetBar();
    };
    paInvertRButton.onClick = [this] {
        audioProcessor.setMonoPaInvertR(paInvertRButton.getToggleState());
    };

    developerLink.setButtonText("distantnebula.com");
    developerLink.setURL(juce::URL("https://distantnebula.com/"));
    developerLink.setColour(juce::HyperlinkButton::textColourId,
                            dgui::text().withAlpha(0.72f));
    developerLink.setFont(dgui::trackedFont(9.0f, 1.5f), false);
    developerLink.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(developerLink);
    updateLink.setFont(dgui::trackedFont(9.0f, 1.2f), false);
    updateLink.setJustificationType(juce::Justification::centredRight);
    updateLink.setColour(juce::HyperlinkButton::textColourId, dgui::amber());
    addChildComponent(updateLink);

    midiLed.setLedColour(dgui::amber());

    prevButton.onClick = [this] {
        audioProcessor.stepPreset(-1);
        refreshPresetBar();
    };
    nextButton.onClick = [this] {
        audioProcessor.stepPreset(1);
        refreshPresetBar();
    };

    const auto presetStepMenu = [this](juce::Component& target, int stomp) {
        juce::PopupMenu menu;
        menu.addSectionHeader(juce::String(doomgazer::kStompName[stomp]));
        menu.addItem(1, "MIDI Learn");
        const bool bound = audioProcessor.stompIsBound(stomp);
        menu.addItem(2,
                     bound ? "Clear MIDI (" + audioProcessor.stompBindingLabel(stomp) + ")"
                           : "Clear MIDI",
                     bound);
        juce::Component::SafePointer<juce::Component> safe(&target);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),
                           [this, safe, stomp](int result) {
                               if (safe == nullptr || result == 0)
                                   return;
                               if (result == 1)
                                   audioProcessor.beginStompLearn(stomp);
                               else if (result == 2)
                                   audioProcessor.clearStompBinding(stomp);
                               refreshPresetBar();
                           });
    };
    prevButton.onContextMenu = [this, presetStepMenu] {
        presetStepMenu(prevButton, static_cast<int>(doomgazer::Stomp::presetPrev));
    };
    nextButton.onContextMenu = [this, presetStepMenu] {
        presetStepMenu(nextButton, static_cast<int>(doomgazer::Stomp::presetNext));
    };
    presetBar.onClick = [this] {
        browser.refresh();
        openOverlay(browser);
    };
    presetBar.onStep = [this](int direction) {
        audioProcessor.stepPreset(direction);
        refreshPresetBar();
    };
    saveButton.onClick = [this] {
        if (audioProcessor.currentPresetIsFactory())
        {
            browser.showSaveAs();
            openOverlay(browser);
            return;
        }
        juce::String error;
        if (!audioProcessor.saveCurrentPreset(audioProcessor.currentPresetNotes(), error))
        {
            browser.showSaveAs();
            openOverlay(browser);
        }
        refreshPresetBar();
    };
    revertButton.onClick = [this] {
        audioProcessor.revertPreset();
        refreshPresetBar();
    };
    themeButton.onClick = [this] {
        applyTheme(dgui::theme() == dgui::Theme::light ? dgui::Theme::dark
                                                       : dgui::Theme::light);
    };
    compareButton.onClick = [this] {
        audioProcessor.toggleCompare();
        refreshPresetBar();
    };
    midiButton.onClick = [this] {
        midiPage.refresh();
        openOverlay(midiPage);
    };

    browser.onClose = [this] { closeOverlay(); };
    browser.onLibraryChanged = [this] { refreshPresetBar(); };
    midiPage.onClose = [this] { closeOverlay(); };

    addChildComponent(browser);
    addChildComponent(midiPage);

    // The overlays were built before the saved theme was restored, and the first
    // broadcast ran before they were children. Repeat it now that they are, so
    // their labels are not left with the light palette's ink.
    sendLookAndFeelChange();

    activeBand = juce::jlimit(0, kBandCount - 1,
                              static_cast<int>(audioProcessor.apvts.state.getProperty("ui.band", 0)));
    activeModule = static_cast<int>(audioProcessor.apvts.state.getProperty("ui.module", -1));
    showBand(activeBand, activeModule);


    // A 1536x864 screen at 125% scaling leaves about 1220x583 of logical room
    // once the taskbar and the standalone's chrome are taken out. 1200x580 is
    // the largest default that fits whole; the window stays resizable.
    setSize(1200, 580);
    resized();
    refreshPresetBar();
    refreshStomps();
    refreshModeBoxes();
    refreshButtonColours();
    startTimerHz(30);

    // Owner, repo, and version: DOOMGAZER_GITHUB_* and DOOMGAZER_VERSION_STRING
    // in CMakeLists.txt. The version definition follows project() VERSION.
    juce::Component::SafePointer<DoomgazerAudioProcessorEditor> self(this);
    releaseChecker.check(
        { DOOMGAZER_GITHUB_OWNER, DOOMGAZER_GITHUB_REPO, DOOMGAZER_VERSION_STRING,
          "Doomgazer/" DOOMGAZER_VERSION_STRING },
        juce::Component::SafePointer<juce::Component>(this),
        [self](const ReleaseInfo& info) {
            if (self != nullptr)
                self->showAvailableUpdate(info);
        });
}

DoomgazerAudioProcessorEditor::~DoomgazerAudioProcessorEditor()
{
    releaseChecker.cancel();
    stopTimer();
    knobCells.clear();
    switchCells.clear();
    selectorCells.clear();
    cells.clear();
    setLookAndFeel(nullptr);
}

void DoomgazerAudioProcessorEditor::showBand(int band, int module)
{
    activeBand = juce::jlimit(0, kBandCount - 1, band);
    if (module < 0 || module >= kModuleCount || kModules[module].band != activeBand)
    {
        module = 0;
        for (int index = 0; index < kModuleCount; ++index)
        {
            if (kModules[index].band == activeBand)
            {
                module = index;
                break;
            }
        }
    }
    activeModule = module;

    for (int index = 0; index < static_cast<int>(bandButtons.size()); ++index)
        bandButtons[static_cast<std::size_t>(index)]->setToggleState(
            index == activeBand, juce::dontSendNotification);

    audioProcessor.apvts.state.setProperty("ui.band", activeBand, nullptr);
    audioProcessor.apvts.state.setProperty("ui.module", activeModule, nullptr);

    refreshModuleChips();
    layoutModule();
    layoutChipRow();
    buildBackdrop();
    repaint();
}

void DoomgazerAudioProcessorEditor::showModule(int module)
{
    if (module < 0 || module >= kModuleCount)
        return;
    showBand(kModules[module].band, module);
}

void DoomgazerAudioProcessorEditor::refreshModuleChips()
{
    for (int index = 0; index < static_cast<int>(moduleButtons.size()); ++index)
    {
        auto& button = *moduleButtons[static_cast<std::size_t>(index)];
        button.setVisible(kModules[index].band == activeBand);
        button.setToggleState(index == activeModule, juce::dontSendNotification);
    }
}

void DoomgazerAudioProcessorEditor::layoutChipRow()
{
    std::vector<juce::TextButton*> chips;
    for (int index = 0; index < static_cast<int>(moduleButtons.size()); ++index)
        if (kModules[index].band == activeBand)
            chips.push_back(moduleButtons[static_cast<std::size_t>(index)].get());
    if (chips.empty() || chipArea.getWidth() <= 0)
        return;

    const int chipGap = 6;
    const int count = static_cast<int>(chips.size());
    const int chipWidth = juce::jmin(104, (chipArea.getWidth() - chipGap * (count - 1)) / count);
    const int rowWidth = chipWidth * count + chipGap * (count - 1);
    const int x0 = chipArea.getX() + (chipArea.getWidth() - rowWidth) / 2;
    for (int index = 0; index < count; ++index)
        chips[static_cast<std::size_t>(index)]->setBounds(
            x0 + index * (chipWidth + chipGap), chipArea.getY(), chipWidth,
            chipArea.getHeight());
}

void DoomgazerAudioProcessorEditor::layoutModule()
{
    modulePanel = {};
    for (auto* cell : knobCells)
        cell->setVisible(false);
    for (auto* cell : switchCells)
        cell->setVisible(false);
    for (auto* cell : selectorCells)
        cell->setVisible(false);
    for (auto* cell : orderCells)
        cell->setVisible(false);
    for (auto& box : modeBoxes)
        box->setVisible(false);
    const bool rig = activeModule == kRigModule;
    paOutputButton.setVisible(rig);
    paInvertRButton.setVisible(rig);

    std::vector<dgui::KnobCell*> moduleKnobs;
    std::vector<dgui::SwitchCell*> moduleSwitches;
    std::vector<int> moduleSwitchWidths;
    std::vector<dgui::SelectorCell*> moduleSelectors;
    std::vector<dgui::OrderCell*> moduleOrders;
    std::vector<juce::ComboBox*> moduleModes;
    for (std::size_t i = 0; i < knobCells.size(); ++i)
        if (kKnobSpecs[i].module == activeModule)
            moduleKnobs.push_back(knobCells[i]);
    for (std::size_t i = 0; i < switchCells.size(); ++i)
        if (kSwitchSpecs[i].module == activeModule)
        {
            moduleSwitches.push_back(switchCells[i]);
            moduleSwitchWidths.push_back(kSwitchSpecs[i].width);
        }
    for (std::size_t i = 0; i < selectorCells.size(); ++i)
        if (kSelectorSpecs[i].module == activeModule)
            moduleSelectors.push_back(selectorCells[i]);
    for (std::size_t i = 0; i < orderCells.size(); ++i)
        if (kOrderSpecs[i].module == activeModule)
            moduleOrders.push_back(orderCells[i]);
    std::vector<int> moduleModeWidths;
    std::vector<bool> moduleModeLabelled;
    for (std::size_t i = 0; i < modeBoxes.size(); ++i)
        if (kComboSpecs[i].module == activeModule)
        {
            moduleModes.push_back(modeBoxes[i].get());
            moduleModeWidths.push_back(kComboSpecs[i].caption[0] == 0 ? 140 : 168);
            moduleModeLabelled.push_back(kComboSpecs[i].caption[0] != 0);
        }

    auto area = pageArea;
    if (area.getWidth() < 40 || area.getHeight() < 40)
        return;

    const int knobCount = static_cast<int>(moduleKnobs.size());
    const int comboCount = static_cast<int>(moduleModes.size());
    const int selectorCount = static_cast<int>(moduleSelectors.size());
    const int orderCount = static_cast<int>(moduleOrders.size());
    const int switchCount = static_cast<int>(moduleSwitches.size());
    const int gap = 6;

    // The row's contents and widths, in reading order: mode, selectors, order,
    // switches. If they do not fit, the row wraps to a second one.
    struct RowEntry
    {
        int type;
        int index;
        int width;
    };
    std::vector<RowEntry> entries;
    for (int index = 0; index < comboCount; ++index)
        entries.push_back({ 0, index, moduleModeWidths[static_cast<std::size_t>(index)] });
    for (int index = 0; index < selectorCount; ++index)
        entries.push_back({ 1, index, 104 });
    for (int index = 0; index < orderCount; ++index)
        entries.push_back({ 2, index, 200 });
    for (int index = 0; index < switchCount; ++index)
        entries.push_back({ 3, index, moduleSwitchWidths[static_cast<std::size_t>(index)] });

    const auto widthOf = [&entries, gap](std::size_t from, std::size_t to) {
        int total = 0;
        for (std::size_t i = from; i < to; ++i)
            total += entries[i].width;
        return total + gap * static_cast<int>(to - from > 0 ? to - from - 1 : 0);
    };

    std::size_t split = entries.size();
    bool twoRows = false;
    if (!entries.empty() && widthOf(0, entries.size()) > area.getWidth())
    {
        twoRows = true;
        split = 1;
        while (split < entries.size() && widthOf(0, split + 1) <= area.getWidth())
            ++split;
        if (split >= entries.size() - 1)
            split = entries.size() / 2;
    }

    const int controlHeight =
        entries.empty() ? 0 : (twoRows ? 118 : (knobCount == 0 ? 112 : 72));
    const int rigBlock = rig ? 34 : 0;
    const int stackGap = (controlHeight > 0 || rigBlock > 0) ? 10 : 0;

    const int columns = juce::jmax(1, juce::jmin(knobCount, 8));
    const int rows = knobCount > 0 ? (knobCount + columns - 1) / columns : 0;
    const int room = juce::jmax(
        64, (area.getHeight() - controlHeight - rigBlock - stackGap) / juce::jmax(1, rows));
    const int cellHeight = juce::jmin(132, room);
    const int cellWidth = juce::jmin(126, (area.getWidth() - gap * (columns - 1)) / columns);
    const int rowsHeight = rows > 0 ? rows * cellHeight + gap * (rows - 1) : 0;
    const int blockHeight = rowsHeight + (rowsHeight > 0 ? stackGap : 0) + controlHeight
        + (rigBlock > 0 ? rigBlock + gap : 0);
    area.removeFromTop(juce::jmax(0, (area.getHeight() - blockHeight) / 2));

    if (knobCount > 0)
    {
        auto knobArea = area.removeFromTop(rowsHeight);
        const int columnsUsed = juce::jmin(knobCount, columns);
        const int rowWidth = columnsUsed * cellWidth + gap * (columnsUsed - 1);
        const int x0 = knobArea.getX() + (knobArea.getWidth() - rowWidth) / 2;
        for (int index = 0; index < knobCount; ++index)
        {
            const int row = index / columnsUsed;
            const int column = index % columnsUsed;
            moduleKnobs[static_cast<std::size_t>(index)]->setBounds(
                x0 + column * (cellWidth + gap), knobArea.getY() + row * (cellHeight + gap),
                cellWidth, cellHeight);
            moduleKnobs[static_cast<std::size_t>(index)]->setVisible(true);
        }
        area.removeFromTop(stackGap);
    }

    if (!entries.empty())
    {
        auto row = area.removeFromTop(controlHeight);
        const auto placeRow = [&](std::size_t from, std::size_t to, int y, int height) {
            const int total = widthOf(from, to);
            int x = row.getX() + (row.getWidth() - total) / 2;
            for (std::size_t i = from; i < to; ++i)
            {
                const auto& item = entries[i];
                switch (item.type)
                {
                    case 0:
                    {
                        // A caption such as PITCH sits inline at the left; the
                        // rest of the pickers need none.
                        const int labelPad =
                            moduleModeLabelled[static_cast<std::size_t>(item.index)] ? 50 : 0;
                        moduleModes[static_cast<std::size_t>(item.index)]->setBounds(
                            x + labelPad, y + (height - 26) / 2, item.width - labelPad, 26);
                        moduleModes[static_cast<std::size_t>(item.index)]->setVisible(true);
                        break;
                    }
                    case 1:
                        moduleSelectors[static_cast<std::size_t>(item.index)]->setBounds(
                            x, y, item.width, height);
                        moduleSelectors[static_cast<std::size_t>(item.index)]->setVisible(true);
                        break;
                    case 2:
                        moduleOrders[static_cast<std::size_t>(item.index)]->setBounds(
                            x, y, item.width, height);
                        moduleOrders[static_cast<std::size_t>(item.index)]->setVisible(true);
                        break;
                    default:
                        moduleSwitches[static_cast<std::size_t>(item.index)]->setBounds(
                            x, y, item.width, height);
                        moduleSwitches[static_cast<std::size_t>(item.index)]->setVisible(true);
                        break;
                }
                x += item.width + gap;
            }
        };

        if (twoRows)
        {
            placeRow(0, split, row.getY(), 56);
            placeRow(split, entries.size(), row.getY() + 62, 56);
        }
        else
        {
            placeRow(0, entries.size(), row.getY(), 72);
        }
    }

    if (rig)
    {
        auto rigRow = area.removeFromTop(rigBlock);
        const int width = 92;
        const int gapBetween = 6;
        const int totalWidth = width * 2 + gapBetween;
        const int x0 = rigRow.getX() + (rigRow.getWidth() - totalWidth) / 2;
        paOutputButton.setBounds(x0, rigRow.getY(), width, 26);
        paInvertRButton.setBounds(x0 + width + gapBetween, rigRow.getY(), width, 26);
        const juce::Rectangle<int> rigBounds(x0, rigRow.getY(), totalWidth, 26);
        modulePanel = rigBounds.expanded(16, 12);
        return;
    }

    // The panel wraps only what the module shows, so a small module is a small
    // card and the card background does the framing.
    juce::Rectangle<int> content;
    for (int index = 0; index < knobCount; ++index)
        content = content.getUnion(moduleKnobs[static_cast<std::size_t>(index)]->getBounds());
    for (int index = 0; index < comboCount; ++index)
    {
        const auto bounds = moduleModes[static_cast<std::size_t>(index)]->getBounds();
        content = content.getUnion(moduleModeLabelled[static_cast<std::size_t>(index)]
                                       ? bounds.withTrimmedLeft(-46)
                                       : bounds);
    }
    for (int index = 0; index < selectorCount; ++index)
        content = content.getUnion(moduleSelectors[static_cast<std::size_t>(index)]->getBounds());
    for (int index = 0; index < orderCount; ++index)
        content = content.getUnion(moduleOrders[static_cast<std::size_t>(index)]->getBounds());
    for (int index = 0; index < switchCount; ++index)
        content = content.getUnion(moduleSwitches[static_cast<std::size_t>(index)]->getBounds());
    if (!content.isEmpty())
    {
        const int padX = knobCount == 0 ? 26 : 16;
        const int padY = knobCount == 0 ? 26 : 12;
        modulePanel = content.expanded(padX, padY).getIntersection(pageArea.expanded(4, 2));
    }
}

void DoomgazerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);
    headerArea = area.removeFromTop(52);
    area.removeFromTop(10);
    stripArea = area.removeFromBottom(58);
    area.removeFromBottom(8);
    bodyArea = area;

    const int gap = 12;
    const int rightWidth = juce::jlimit(330, 440, bodyArea.getWidth() * 38 / 100);
    rightArea = bodyArea.removeFromRight(rightWidth);
    bodyArea.removeFromRight(gap);
    controlsArea = bodyArea;

    auto inner = controlsArea.reduced(14, 12);
    tabsArea = inner.removeFromTop(24);
    inner.removeFromTop(24);
    chipArea = inner.removeFromTop(22);
    inner.removeFromTop(10);
    outputArea = inner.removeFromBottom(92);
    inner.removeFromBottom(6);
    pageArea = inner;

    // The right column is one card: tuner, equal meters, a tall spectrum, then
    // the scope.
    meterArea = rightArea;
    auto column = meterArea;
    tunerArea = column.removeFromTop(104);
    column.removeFromTop(8);
    levelsArea = column.removeFromTop(41).reduced(18, 0);
    column.removeFromTop(8);
    spectrumArea = column.removeFromTop(column.getHeight() * 60 / 100);
    column.removeFromTop(8);
    scopeArea = column;

    tuner.setBounds(tunerArea);
    levelMeter.setBounds(levelsArea);
    spectrum.setBounds(spectrumArea);
    scope.setBounds(scopeArea);

    // Header: wordmark, then the engine switch centred in the run between the
    // wordmark and the preset group, then the centred preset group, then the
    // utilities on the right.
    auto header = headerArea;
    wordmarkArea = header.removeFromLeft(updateLink.isVisible() ? 252 : 196);
    // Tucked right under the wordmark rather than pinned to the header's bottom.
    const int makerX = wordmarkArea.getX() + 23;
    const int makerY = wordmarkArea.getY() + 27;
    const int makerRight = wordmarkArea.getRight();
    if (updateLink.isVisible())
    {
        constexpr int updateWidth = 52;
        updateLink.setBounds(makerRight - updateWidth, makerY, updateWidth, 13);
        developerLink.setBounds(makerX, makerY,
                                juce::jmax(0, updateLink.getX() - 6 - makerX), 13);
    }
    else
    {
        developerLink.setBounds(makerX, makerY, juce::jmax(0, makerRight - makerX), 13);
    }

    midiButton.setBounds(header.removeFromRight(56).withSizeKeepingCentre(56, 26));
    header.removeFromRight(6);
    midiLed.setBounds(header.removeFromRight(14).withSizeKeepingCentre(10, 10));
    header.removeFromRight(10);
    compareButton.setBounds(header.removeFromRight(38).withSizeKeepingCentre(38, 26));
    header.removeFromRight(8);
    themeButton.setBounds(header.removeFromRight(28).withSizeKeepingCentre(28, 26));
    header.removeFromRight(20);

    // The three sections — engine switch, preset bar, save pair — are one block,
    // centred in the run between the wordmark and the utilities.
    const int saveWidth = 52;
    const int revertWidth = 62;
    const int stepWidth = 28;
    const int engineWidth = 120;
    const int fixedWidth = engineWidth + 24 + stepWidth * 2 + 6 * 2 + 12 + saveWidth + 6
        + revertWidth;
    const int barWidth = juce::jlimit(120, 300, header.getWidth() - fixedWidth - 32);
    const int blockWidth = fixedWidth + barWidth;
    auto block = header.withSizeKeepingCentre(juce::jmin(blockWidth, header.getWidth()), 28);

    if (engineSelector != nullptr)
        engineSelector->setBounds(block.removeFromLeft(engineWidth).withHeight(26).withY(
            headerArea.getCentreY() - 13));
    block.removeFromLeft(24);
    prevButton.setBounds(block.removeFromLeft(stepWidth).withSizeKeepingCentre(stepWidth, 26));
    block.removeFromLeft(6);
    presetBar.setBounds(block.removeFromLeft(barWidth).withSizeKeepingCentre(barWidth, 28));
    block.removeFromLeft(6);
    nextButton.setBounds(block.removeFromLeft(stepWidth).withSizeKeepingCentre(stepWidth, 26));
    block.removeFromLeft(12);
    saveButton.setBounds(block.removeFromLeft(saveWidth).withSizeKeepingCentre(saveWidth, 26));
    block.removeFromLeft(6);
    revertButton.setBounds(block.removeFromLeft(revertWidth).withSizeKeepingCentre(revertWidth, 26));

    // Band tabs, then the buttons for the modules of the active band.
    auto tabs = tabsArea;
    const int bandGap = 4;
    const int bandCount = static_cast<int>(bandButtons.size());
    if (bandCount > 0 && tabs.getWidth() > 0)
    {
        const int bandWidth = (tabs.getWidth() - bandGap * (bandCount - 1)) / bandCount;
        for (int index = 0; index < bandCount; ++index)
            bandButtons[static_cast<std::size_t>(index)]->setBounds(
                tabs.getX() + index * (bandWidth + bandGap), tabs.getY(), bandWidth,
                tabs.getHeight());
    }

    layoutChipRow();

    // The output group, always visible: master, limiter, mute.
    if (masterCell != nullptr && limitCell != nullptr && muteCell != nullptr)
    {
        auto output = outputArea;
        masterCell->setBounds(output.removeFromRight(88));
        output.removeFromRight(8);
        muteCell->setBounds(output.removeFromRight(70));
        output.removeFromRight(6);
        limitCell->setBounds(output.removeFromRight(70));
        output.removeFromRight(12);
        hintArea = output;
        hint.setBounds(hintArea);
    }

    // The action rows: two lines so every effect fits without shrinking a button.
    const int stripCount = static_cast<int>(stripButtons.size());
    if (stripCount > 0 && stripArea.getWidth() > 0)
    {
        const int stripGap = 4;
        const int rowGap = 4;
        const int stripRows = stripCount > 10 ? 2 : 1;
        const int stripColumns = (stripCount + stripRows - 1) / stripRows;
        const int width =
            (stripArea.getWidth() - stripGap * (stripColumns - 1)) / stripColumns;
        const int height = (stripArea.getHeight() - rowGap * (stripRows - 1)) / stripRows;
        for (int index = 0; index < stripCount; ++index)
        {
            const int row = index / stripColumns;
            const int stripColumn = index % stripColumns;
            const int rowCount = juce::jmin(stripColumns, stripCount - row * stripColumns);
            const int rowWidth = rowCount * width + stripGap * (rowCount - 1);
            const int rowX = stripArea.getX() + (stripArea.getWidth() - rowWidth) / 2;
            stripButtons[static_cast<std::size_t>(index)]->setBounds(
                rowX + stripColumn * (width + stripGap),
                stripArea.getY() + row * (height + rowGap), width, height);
        }
    }

    layoutModule();

    const int browserWidth = juce::jmin(760, getWidth() - 48);
    const int browserHeight = juce::jmin(500, getHeight() - 48);
    browser.setBounds(juce::Rectangle<int>(browserWidth, browserHeight)
                          .withCentre(getLocalBounds().getCentre()));

    const int midiWidthPage = juce::jmin(980, getWidth() - 48);
    const int midiHeightPage = juce::jmin(556, getHeight() - 24);
    midiPage.setBounds(juce::Rectangle<int>(midiWidthPage, midiHeightPage)
                           .withCentre(getLocalBounds().getCentre()));

    buildBackdrop();
}

void DoomgazerAudioProcessorEditor::showAvailableUpdate(const ReleaseInfo& info)
{
    if (!info.updateAvailable || !info.htmlUrl.startsWith("https://"))
        return;

    availableRelease = info.tag;
    updateLink.setButtonText("Update");
    updateLink.setURL(juce::URL(info.htmlUrl));
    updateLink.setFont(dgui::trackedFont(9.0f, 1.2f), false);
    updateLink.setJustificationType(juce::Justification::centredRight);
    updateLink.setColour(juce::HyperlinkButton::textColourId, dgui::amber());
    updateLink.setVisible(true);
    resized();
}

void DoomgazerAudioProcessorEditor::applyTheme(dgui::Theme newTheme)
{
    if (dgui::theme() == newTheme)
        return;

    dgui::setTheme(newTheme);
    audioProcessor.apvts.state.setProperty("ui.theme", static_cast<int>(newTheme), nullptr);
    audioProcessor.apvts.state.removeProperty("ui.dark", nullptr);

    midiLed.setLedColour(dgui::amber());
    lookAndFeel.updateColours();
    sendLookAndFeelChange();
    refreshButtonColours();

    buildBackdrop();
    repaint();
}

void DoomgazerAudioProcessorEditor::updateHint()
{
    if (hintLock > 0)
    {
        hint.setHint({}, juce::String(doomgazer::parameterName(hintLock)), helpFor(hintLock));
        return;
    }

    if (overlay != nullptr)
    {
        hint.setHint({}, {}, "Hover over a control for more details.");
        return;
    }

    const auto mouse = getMouseXYRelative();
    if (!getLocalBounds().contains(mouse))
    {
        hint.setHint({}, {}, "Hover over a control for more details.");
        return;
    }

    const auto over = [this, mouse](const juce::Component& component) {
        return component.isVisible() && component.getBounds().contains(mouse);
    };

    if (over(presetBar))
    {
        const juce::String notes = audioProcessor.currentPresetNotes();
        hint.setHint({}, audioProcessor.currentPresetName(),
                     notes.isEmpty()
                         ? "Click to browse presets. Scroll to step through them."
                         : notes);
        return;
    }
    if (over(prevButton))
    {
        hint.setHint({}, "Previous preset", "Steps back one preset; wraps at the start.");
        return;
    }
    if (over(nextButton))
    {
        hint.setHint({}, "Next preset", "Steps forward one preset; wraps at the end.");
        return;
    }
    if (over(saveButton))
    {
        hint.setHint({}, "Save",
                     "Writes your edits into the current user preset. On a factory preset it "
                     "opens Save As.");
        return;
    }
    if (over(revertButton))
    {
        hint.setHint({}, "Revert", "Discards edits and reloads the preset you had.");
        return;
    }
    if (over(themeButton))
    {
        const char* name = dgui::theme() == dgui::Theme::light ? "Light" : "Dark";
        hint.setHint({}, juce::String("Theme: ") + name,
                     "Click to switch between Light and Dark. The choice is stored in the project.");
        return;
    }
    if (over(compareButton))
    {
        hint.setHint({}, "A / B",
                     "Stores the current sound and flips to the other slot, so you can compare "
                     "two settings.");
        return;
    }
    if (over(midiButton))
    {
        hint.setHint({}, "MIDI",
                     "Opens the MIDI map: channel, program change, learn, and the action row "
                     "bindings.");
        return;
    }
    if (over(updateLink))
    {
        hint.setHint({}, "Update",
                     "Version " + availableRelease
                         + " is available. Opens the release page in your browser.");
        return;
    }
    if (over(developerLink))
    {
        hint.setHint({}, "Distant Nebula", "Opens the maker's site in your browser.");
        return;
    }
    if (engineSelector != nullptr && over(*engineSelector))
    {
        hint.setHint({}, juce::String(doomgazer::parameterName(1)), helpFor(1));
        return;
    }

    if (over(tuner))
    {
        hint.setHint({}, "Tuner",
                     "The note being played. Green when it is in tune, red when sharp or flat.");
        return;
    }
    if (over(spectrum))
    {
        hint.setHint({}, "Spectrum",
                     "Energy in each band, 50 Hz to 10 kHz, taken from the dry input.");
        return;
    }
    if (over(scope))
    {
        hint.setHint({}, "Scope",
                     "The output waveform. The faint trace behind it is the input, so you can "
                     "see what the plugin did. While Mute is on it shows the dry input.");
        return;
    }
    if (over(levelMeter))
    {
        hint.setHint({}, "Levels",
                     "Three bars on a 60 dB scale. INPUT is the held peak after Input Trim. The two "
                     "marks are the window, -18 to -8 dB, where the factory sounds were made. Its "
                     "lamp is a clip on the way in, before the trim. PEAK and RMS are the output, "
                     "and their lamp lights when the output clips.");
        return;
    }

    for (auto* cell : knobCells)
    {
        if (!cell->isVisible())
            continue;
        if (!cell->hitsHint(cell->getLocalPoint(this, mouse)))
            continue;
        hint.setHint({}, juce::String(doomgazer::parameterName(cell->parameterIndex())),
                     helpFor(cell->parameterIndex()));
        return;
    }
    for (auto* cell : switchCells)
    {
        if (!cell->isVisible())
            continue;
        if (!cell->hitsHint(cell->getLocalPoint(this, mouse)))
            continue;
        hint.setHint({}, juce::String(doomgazer::parameterName(cell->parameterIndex())),
                     helpFor(cell->parameterIndex()));
        return;
    }
    for (auto* cell : selectorCells)
    {
        if (!cell->isVisible())
            continue;
        if (!cell->hitsHint(cell->getLocalPoint(this, mouse)))
            continue;
        hint.setHint({}, juce::String(doomgazer::parameterName(cell->parameterIndex())),
                     helpFor(cell->parameterIndex()));
        return;
    }
    for (auto* cell : orderCells)
    {
        if (!cell->isVisible())
            continue;
        if (!cell->hitsHint(cell->getLocalPoint(this, mouse)))
            continue;
        hint.setHint({}, juce::String(doomgazer::parameterName(cell->parameterIndex())),
                     helpFor(cell->parameterIndex()));
        return;
    }
    for (std::size_t index = 0; index < modeBoxes.size(); ++index)
    {
        if (!modeBoxes[index]->isVisible() || !over(*modeBoxes[index]))
            continue;
        const int parameter = modeBoxParameter[index];
        hint.setHint({}, juce::String(doomgazer::parameterName(parameter)),
                     helpFor(parameter));
        return;
    }
    if (masterCell != nullptr && over(*masterCell))
    {
        hint.setHint({}, juce::String(doomgazer::parameterName(24)), helpFor(24));
        return;
    }
    if (limitCell != nullptr && over(*limitCell))
    {
        hint.setHint({}, juce::String(doomgazer::parameterName(36)), helpFor(36));
        return;
    }
    if (muteCell != nullptr && over(*muteCell))
    {
        hint.setHint({}, juce::String(doomgazer::parameterName(55)), helpFor(55));
        return;
    }

    for (int band = 0; band < static_cast<int>(bandButtons.size()); ++band)
    {
        if (bandButtons[static_cast<std::size_t>(band)]->isVisible()
            && over(*bandButtons[static_cast<std::size_t>(band)]))
        {
            hint.setHint({}, juce::String("Band: ") + kBandName[band], kBandHelp[band]);
            return;
        }
    }
    for (int module = 0; module < static_cast<int>(moduleButtons.size()); ++module)
    {
        auto& button = *moduleButtons[static_cast<std::size_t>(module)];
        if (!button.isVisible() || !over(button))
            continue;
        hint.setHint({}, kModules[module].name, kModules[module].help);
        return;
    }

    for (std::size_t index = 0; index < stripButtons.size(); ++index)
    {
        if (!over(*stripButtons[index]))
            continue;
        const auto& spec = kStripSpecs[index];
        juce::String body = "Toggles " + juce::String(doomgazer::kStompName[static_cast<int>(spec.stomp)])
            + ". Right-click to learn a MIDI control.";
        const juce::String bound = audioProcessor.stompBindingLabel(static_cast<int>(spec.stomp));
        if (bound.isNotEmpty())
            body << " Bound: " << bound << ".";
        hint.setHint({}, juce::String(spec.label), body);
        return;
    }

    if (controlsArea.contains(mouse))
    {
        hint.setHint({}, kModules[activeModule].name, kModules[activeModule].help);
        return;
    }

    hint.setHint({}, {}, "Hover over a control for more details.");
}

void DoomgazerAudioProcessorEditor::buildBackdrop()
{
    const int width = juce::jmax(1, getWidth());
    const int height = juce::jmax(1, getHeight());
    const int themeValue = static_cast<int>(dgui::theme());
    if (backdrop.isValid() && width == backdropWidth && height == backdropHeight
        && activeBand == backdropBand && activeModule == backdropModule
        && themeValue == backdropTheme)
        return;

    backdrop = juce::Image(juce::Image::RGB, width, height, true);

    const int lowWidth = juce::jmax(4, width / 5);
    const int lowHeight = juce::jmax(4, height / 5);
    juce::Image blobs(juce::Image::RGB, lowWidth, lowHeight, true);
    {
        juce::Graphics b(blobs);
        b.fillAll(dgui::ink());

        const auto blob = [&b](float cx, float cy, float radius, juce::Colour colour) {
            juce::ColourGradient gradient(colour, cx, cy, colour.withAlpha(0.0f),
                                          cx + radius, cy, true);
            b.setGradientFill(gradient);
            b.fillAll();
        };

        const auto w = static_cast<float>(lowWidth);
        const auto h = static_cast<float>(lowHeight);
        const float strength = dgui::darkMode() ? 0.92f : 1.0f;
        blob(w * 0.10f, h * 0.06f, w * 0.62f, dgui::amber().withAlpha(0.42f * strength));
        blob(w * 0.52f, h * 1.04f, w * 0.60f, dgui::green().withAlpha(0.36f * strength));
        blob(w * 1.02f, h * 0.40f, w * 0.56f, dgui::blue().withAlpha(0.32f * strength));
        blob(w * 0.45f, h * 0.48f, w * 0.85f, dgui::steel().withAlpha(0.10f * strength));
    }

    juce::Graphics g(backdrop);
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    g.drawImage(blobs, 0, 0, width, height, 0, 0, lowWidth, lowHeight);

    dgui::drawCard(g, controlsArea);

    // A thin rule separates the band row from the module row.
    if (!tabsArea.isEmpty() && !chipArea.isEmpty())
    {
        const int y = (tabsArea.getBottom() + chipArea.getY()) / 2;
        g.setColour(dgui::line().withAlpha(0.85f));
        g.drawHorizontalLine(y, static_cast<float>(tabsArea.getX()),
                             static_cast<float>(tabsArea.getRight()));
    }
    // The module sits in a panel of its own, so a small module reads as padding
    // inside a surface rather than as a hole in the card.
    if (!modulePanel.isEmpty())
    {
        const auto panel = modulePanel.toFloat();
        g.setColour(dgui::panel().withAlpha(dgui::darkMode() ? 0.38f : 0.30f));
        g.fillRoundedRectangle(panel, 8.0f);
        // The card's outline is the module chip's colour, so the button and the
        // surface it opens read as one thing.
        const auto accent = dgui::accentFor(kBandRole[kModules[activeModule].band]);
        g.setColour(accent.withAlpha(dgui::darkMode() ? 0.62f : 0.48f));
        g.drawRoundedRectangle(panel.reduced(0.5f), 8.0f, 1.5f);
    }
    dgui::drawCard(g, meterArea);

    // The hint is its own surface, so it needs an edge: a soft well with a
    // hairline, inside the card's footer.
    if (!hintArea.isEmpty())
    {
        const auto box = hintArea.toFloat().reduced(0.5f);
        g.setColour(dgui::panel().withAlpha(dgui::darkMode() ? 0.45f : 0.35f));
        g.fillRoundedRectangle(box, 7.0f);
        g.setColour(dgui::line());
        g.drawRoundedRectangle(box.reduced(0.5f), 7.0f, 1.0f);
    }

    // Wordmark.
    const int mark = 13;
    g.setColour(dgui::amber());
    g.fillRect(wordmarkArea.getX(), wordmarkArea.getY() + 8, mark, mark);
    g.setColour(dgui::text());
    g.setFont(dgui::trackedFont(21.0f, 1.0f, true));
    g.drawText("DOOMGAZER", wordmarkArea.getX() + mark + 10, wordmarkArea.getY() + 1,
               wordmarkArea.getWidth() - mark, 28, juce::Justification::centredLeft);
    // The footer is the hint strip: it explains whatever is under the pointer,
    // and the idle text explains nothing. No rule above it: whitespace does the
    // separating, which keeps the card free of stray lines.

    backdropWidth = width;
    backdropHeight = height;
    backdropBand = activeBand;
    backdropModule = activeModule;
    backdropTheme = themeValue;
}

void DoomgazerAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (backdrop.isValid())
        g.drawImageAt(backdrop, 0, 0);
    else
        g.fillAll(dgui::ink());

    for (std::size_t index = 0; index < modeBoxes.size(); ++index)
    {
        if (!modeBoxes[index]->isVisible() || modeBoxLayout[index].isEmpty())
            continue;
        const auto bounds = modeBoxes[index]->getBounds();
        g.setColour(dgui::dim());
        g.setFont(dgui::trackedFont(9.0f, 1.4f, true));
        g.drawText(modeBoxLayout[index], bounds.getX() - 46, bounds.getY(), 42,
                   bounds.getHeight(), juce::Justification::centredRight);
    }

    if (overlay != nullptr)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(getLocalBounds());
    }
}

void DoomgazerAudioProcessorEditor::openOverlay(juce::Component& overlayToShow)
{
    if (overlay == &overlayToShow)
        return;
    if (overlay != nullptr)
        overlay->setVisible(false);
    overlay = &overlayToShow;
    overlay->setAlpha(1.0f);
    overlay->setVisible(true);
    overlay->toFront(false);
    repaint();
}

void DoomgazerAudioProcessorEditor::closeOverlay()
{
    if (overlay == nullptr)
        return;
    overlay->setVisible(false);
    overlay = nullptr;
    repaint();
}

void DoomgazerAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    if (overlay != nullptr && !overlay->getBounds().contains(event.getPosition()))
        closeOverlay();
}

bool DoomgazerAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (overlay == &browser && browser.handleOverlayKey(key))
        return true;

    if (overlay != nullptr
        && (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey))
    {
        closeOverlay();
        return true;
    }
    return false;
}

void DoomgazerAudioProcessorEditor::refreshPresetBar()
{
    const juce::String name = audioProcessor.currentPresetName();
    const bool dirty = audioProcessor.presetIsDirty();
    const bool factory = audioProcessor.currentPresetIsFactory();

    if (name != shownPresetName || dirty != shownDirty || factory != shownFactory)
    {
        shownPresetName = name;
        shownDirty = dirty;
        shownFactory = factory;
        presetBar.setButtonText(dirty ? name + "  *" : name);
        saveButton.setEnabled(factory || dirty);
        revertButton.setEnabled(dirty);
    }

    const bool compareB = audioProcessor.compareSlot() == 1;
    if (compareB != shownCompare)
    {
        shownCompare = compareB;
        compareButton.setButtonText(compareB ? "B" : "A");
        compareButton.setToggleState(compareB, juce::dontSendNotification);
    }

    const bool paMono = audioProcessor.outputMono();
    if (paMono != shownPaMono)
    {
        shownPaMono = paMono;
        paOutputButton.setToggleState(paMono, juce::dontSendNotification);
    }
    paOutputButton.setButtonText(paMono ? "PA MONO" : "PA STEREO");

    const bool paInvert = audioProcessor.monoPaInvertR();
    if (paInvert != shownPaInvert)
    {
        shownPaInvert = paInvert;
        paInvertRButton.setToggleState(paInvert, juce::dontSendNotification);
    }

    midiButton.setToggleState(audioProcessor.learnTarget() >= 0
                                  || audioProcessor.stompLearnTarget() >= 0,
                              juce::dontSendNotification);
}

void DoomgazerAudioProcessorEditor::refreshButtonColours()
{
    // Text colours are intentionally not pinned per button: they follow the look
    // and feel, so switching theme cannot leave a dark label on a dark pill.
    developerLink.setColour(juce::HyperlinkButton::textColourId,
                            dgui::text().withAlpha(0.72f));
    updateLink.setColour(juce::HyperlinkButton::textColourId, dgui::amber());
    styleButton(prevButton, dgui::steel());
    styleButton(nextButton, dgui::steel());
    styleButton(saveButton, dgui::amber());
    styleButton(revertButton, dgui::steel());
    styleButton(compareButton, dgui::green());
    styleButton(midiButton, dgui::blue());
    styleButton(presetBar, dgui::amber());
    styleButton(paOutputButton, dgui::steel());
    styleButton(paInvertRButton, dgui::steel());

    for (int band = 0; band < static_cast<int>(bandButtons.size()); ++band)
    {
        auto& button = *bandButtons[static_cast<std::size_t>(band)];
        const auto accent = dgui::accentFor(kBandRole[band]);
        styleButton(button, accent);
        button.setColour(juce::TextButton::textColourOffId, accent.withAlpha(0.95f));
    }

    for (int module = 0; module < static_cast<int>(moduleButtons.size()); ++module)
    {
        auto& button = *moduleButtons[static_cast<std::size_t>(module)];
        const auto accent = dgui::accentFor(kBandRole[kModules[module].band]);
        button.setColour(juce::TextButton::buttonColourId, dgui::panelHigh());
        button.setColour(juce::TextButton::buttonOnColourId, accent);
        button.setColour(juce::TextButton::textColourOffId, accent.withAlpha(0.95f));
        button.removeColour(juce::TextButton::textColourOnId);
    }

    for (std::size_t index = 0; index < stripButtons.size(); ++index)
        stripButtons[index]->setAccent(dgui::accentFor(kBandRole[kStripSpecs[index].band]));

    if (engineSelector != nullptr)
        engineSelector->setAccents(dgui::accentFor(kBandRole[0]), dgui::accentFor(kBandRole[1]));
    if (masterCell != nullptr)
        masterCell->slider.setColour(juce::Slider::rotarySliderFillColourId, dgui::gold());
    if (limitCell != nullptr)
        limitCell->setAccent(dgui::red());
    if (muteCell != nullptr)
        muteCell->setAccent(dgui::red());

    repaint();
}

void DoomgazerAudioProcessorEditor::refreshStomps(){
    for (std::size_t index = 0; index < stripButtons.size(); ++index)
    {
        const int stomp = static_cast<int>(kStripSpecs[index].stomp);
        stripButtons[index]->setToggleState(audioProcessor.stompIsOn(stomp),
                                           juce::dontSendNotification);
        stripButtons[index]->setBound(audioProcessor.stompIsBound(stomp));
        stripButtons[index]->setLearnArmed(audioProcessor.stompLearnTarget() == stomp);
    }
}

void DoomgazerAudioProcessorEditor::refreshModeBoxes()
{
    for (std::size_t index = 0; index < modeBoxes.size(); ++index)
    {
        const int parameter = modeBoxParameter[index];
        auto* value = audioProcessor.findRawValue(doomgazer::parameterId(parameter));
        if (value == nullptr)
            continue;
        int numeric = static_cast<int>(std::lround(value->load()));
        if (numeric == 0 && modeBoxFirstValue[index] > 0)
        {
            // The mode picker no longer offers Off; while the module is off it
            // still shows the mode it will come back to.
            const int stomp = parameter == 37 ? static_cast<int>(doomgazer::Stomp::delay)
                : parameter == 43          ? static_cast<int>(doomgazer::Stomp::reverb)
                : parameter == 59          ? static_cast<int>(doomgazer::Stomp::glide)
                                           : -1;
            if (stomp >= 0)
                numeric = audioProcessor.stompHeldMode(stomp);
        }
        const int id = numeric - modeBoxFirstValue[index] + 1;
        if (modeBoxes[index]->getSelectedId() != id)
            modeBoxes[index]->setSelectedId(id, juce::dontSendNotification);
    }
}

void DoomgazerAudioProcessorEditor::timerCallback()
{
    audioProcessor.servicePresetHold();
    if (!isShowing())
        return;

    audioProcessor.engine().pollTuner();

    const int outputWrite = audioProcessor.engine().scopeWriteIndex();
    if (outputWrite != shownScopeWrite)
    {
        shownScopeWrite = outputWrite;
        scope.repaint();
    }

    const int inputWrite = audioProcessor.engine().inputScopeWriteIndex();
    if (inputWrite != shownSpectrumWrite)
    {
        shownSpectrumWrite = inputWrite;
        spectrum.repaint();
    }

    const auto tunerSerial = audioProcessor.engine().tunerSerial();
    if (tunerSerial != shownTunerSerial)
    {
        shownTunerSerial = tunerSerial;
        tuner.repaint();
    }
    levelMeter.tick();

    for (auto* knob : knobCells)
        knob->tick();
    for (auto* toggle : switchCells)
        toggle->tick();
    for (auto* selector : selectorCells)
        selector->tick();
    for (auto* swap : orderCells)
        swap->tick();
    if (engineSelector != nullptr)
        engineSelector->tick();
    if (masterCell != nullptr)
        masterCell->tick();
    if (limitCell != nullptr)
        limitCell->tick();
    if (muteCell != nullptr)
        muteCell->tick();

    refreshStomps();
    refreshModeBoxes();

    const int activity = audioProcessor.midiActivity();
    if (activity != shownActivity)
    {
        shownActivity = activity;
        midiLed.flash();
    }
    midiLed.tick();

    refreshPresetBar();
    updateHint();
    if (browser.isVisible())
        browser.pollHoverHint();
    if (midiPage.isVisible())
        midiPage.pollHoverHint();
}
