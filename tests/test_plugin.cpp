#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

struct JuceGuard
{
    juce::ScopedJuceInitialiser_GUI initialiser;
};

void dispatchMessages()
{
    if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
        manager->runDispatchLoopUntil(25);
}

void processBlock(DoomgazerAudioProcessor& processor, juce::MidiBuffer& midi, int samples = 512)
{
    juce::AudioBuffer<float> buffer(2, samples);
    buffer.clear();
    processor.processBlock(buffer, midi);
}

void sendController(DoomgazerAudioProcessor& processor, int cc, int value)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, value), 0);
    processBlock(processor, midi);
    dispatchMessages();
}

void sendProgram(DoomgazerAudioProcessor& processor, int program)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::programChange(1, program), 0);
    processBlock(processor, midi);
    dispatchMessages();
}

void sendNote(DoomgazerAudioProcessor& processor, int note, int velocity)
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(velocity)), 0);
    processBlock(processor, midi);
    dispatchMessages();
}

float rawValue(DoomgazerAudioProcessor& processor, const char* id)
{
    const auto* value = processor.apvts.getRawParameterValue(id);
    return value != nullptr ? value->load() : 0.0f;
}

float parameterValue(DoomgazerAudioProcessor& processor, const char* id)
{
    const auto* parameter = processor.apvts.getParameter(id);
    return parameter != nullptr ? parameter->getValue() : 0.0f;
}

void setPlain(DoomgazerAudioProcessor& processor, const char* id, float plain)
{
    if (auto* parameter = processor.apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}

void fillSine(std::vector<double>& left, std::vector<double>& right, double hz, double phase)
{
    const double step = 2.0 * doomgazer::kPi * hz / 48000.0;
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        const double sample = 0.4 * std::sin(phase + step * static_cast<double>(i));
        left[i] = sample;
        right[i] = sample;
    }
}

} // namespace

TEST_CASE("state restore reaches the DSP through the preset mailbox", "[plugin][batch-a]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor source;
    DoomgazerAudioProcessor target;
    source.prepareToPlay(48000.0, 512);
    target.prepareToPlay(48000.0, 512);

    setPlain(target, "master", -20.0f);
    juce::MidiBuffer empty;
    processBlock(target, empty);
    REQUIRE(target.engine().parameters().masterDb == Catch::Approx(-20.0).margin(0.01));

    setPlain(source, "master", 6.0f);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE(state.getSize() > 0);

    target.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    // The message thread must not write the DSP; the old code installed this
    // synchronously before processBlock could run.
    REQUIRE(target.engine().parameters().masterDb == Catch::Approx(-20.0).margin(0.01));

    for (int block = 0; block < 8; ++block)
        processBlock(target, empty);

    REQUIRE(target.engine().parameters().masterDb == Catch::Approx(6.0).margin(0.1));
}

TEST_CASE("tuner analysis runs on the message thread, not the audio thread", "[plugin][batch-a]")
{
    doomgazer::Processor processor;
    processor.prepare(48000.0, 256);
    processor.reset();

    std::vector<double> left(256);
    std::vector<double> right(256);
    for (int block = 0; block < 200; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const double n = static_cast<double>(block * 256 + i);
            const double t = n / 48000.0;
            const double sample = 0.22 * std::sin(2.0 * doomgazer::kPi * 58.27 * t)
                                + 0.40 * std::sin(2.0 * doomgazer::kPi * 116.54 * t);
            left[static_cast<std::size_t>(i)] = sample;
            right[static_cast<std::size_t>(i)] = sample;
        }
        processor.process(left.data(), right.data(), 256, nullptr);
    }

    REQUIRE_FALSE(processor.tuner().active);

    processor.pollTuner();
    const auto reading = processor.tuner();
    REQUIRE(reading.active);
    REQUIRE(reading.hz > 50.0);
    REQUIRE(reading.hz < 70.0);
}

TEST_CASE("non-finite input does not latch the leveler or sag", "[plugin][batch-a]")
{
    doomgazer::Processor processor;
    processor.prepare(48000.0, 256);
    processor.reset();

    std::vector<double> left(256);
    std::vector<double> right(256);
    fillSine(left, right, 220.0, 0.0);
    left[10] = std::numeric_limits<double>::quiet_NaN();
    right[20] = std::numeric_limits<double>::infinity();
    processor.process(left.data(), right.data(), 256, nullptr);

    double peak = 0.0;
    double phase = 0.0;
    for (int block = 0; block < 12; ++block)
    {
        fillSine(left, right, 220.0, phase);
        phase += 256.0 * 2.0 * doomgazer::kPi * 220.0 / 48000.0;
        processor.process(left.data(), right.data(), 256, nullptr);
        for (int i = 0; i < 256; ++i)
        {
            REQUIRE(std::isfinite(left[static_cast<std::size_t>(i)]));
            REQUIRE(std::isfinite(right[static_cast<std::size_t>(i)]));
            peak = std::max(peak, std::abs(left[static_cast<std::size_t>(i)]));
        }
    }
    REQUIRE(peak > 1.0e-3);
}

TEST_CASE("action-row MIDI toggles wait for the message thread", "[plugin][batch-a]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    const int grit = static_cast<int>(doomgazer::Stomp::grit);
    processor.beginStompLearn(grit);
    sendNote(processor, 36, 100);
    REQUIRE(processor.stompIsBound(grit));

    const float before = rawValue(processor, "gritAmount");
    const float beforeParameter = parameterValue(processor, "gritAmount");
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    processBlock(processor, midi);

    // The old code wrote the parameter from processBlock; the toggle must now
    // still be queued at this point. The parameter's own value moves in setValue,
    // while the APVTS raw value only follows the async listener notification.
    REQUIRE(parameterValue(processor, "gritAmount") == Catch::Approx(beforeParameter).margin(1.0e-6));

    dispatchMessages();
    REQUIRE(rawValue(processor, "gritAmount") > before + 0.01f);
}

TEST_CASE("all-notes-off clears the held CC edge state", "[plugin][batch-a]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    const int grit = static_cast<int>(doomgazer::Stomp::grit);
    processor.beginStompLearn(grit);
    sendController(processor, 20, 100);
    REQUIRE(processor.stompIsBound(grit));

    sendController(processor, 20, 0);
    sendController(processor, 20, 100);
    REQUIRE(processor.stompIsOn(grit));

    sendController(processor, 123, 0);

    // The old all-notes-off branch was unreachable because CC 123 is itself a
    // controller message, so the high previous value suppressed this edge.
    sendController(processor, 20, 100);
    REQUIRE_FALSE(processor.stompIsOn(grit));
}
namespace {

struct TempPresetFolder
{
    TempPresetFolder()
        : folder(juce::File::getSpecialLocation(juce::File::tempDirectory)
                     .getChildFile("doomgazer-test-"
                                   + juce::String(static_cast<juce::int64>(
                                         juce::Time::getMillisecondCounterHiRes()))
                                   + "-"
                                   + juce::String(juce::Random::getSystemRandom().nextInt(1000000))))
    {
        folder.deleteRecursively();
        folder.createDirectory();
    }

    ~TempPresetFolder() { folder.deleteRecursively(); }

    juce::File folder;
};

struct ProgramChangeListener : juce::AudioProcessorListener
{
    int programChanges = 0;

    void audioProcessorChanged(juce::AudioProcessor*,
                               const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (details.programChanged)
            ++programChanges;
    }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) override {}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) override {}
};

bool saveCopy(DoomgazerAudioProcessor& processor, const juce::String& name, juce::String& error)
{
    return processor.savePresetCopy(name, "User", {}, false, error);
}

juce::TextButton* findButton(juce::Component& root, const juce::String& text)
{
    if (auto* button = dynamic_cast<juce::TextButton*>(&root))
        if (button->getButtonText() == text)
            return button;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* found = findButton(*root.getChildComponent(index), text))
            return found;
    return nullptr;
}

juce::TextButton* findButtonWithSibling(juce::Component& root, const juce::String& text,
                                        const juce::String& siblingText)
{
    for (int index = 0; index < root.getNumChildComponents(); ++index)
    {
        auto* child = root.getChildComponent(index);
        if (auto* button = dynamic_cast<juce::TextButton*>(child))
        {
            if (button->getButtonText() == text)
            {
                if (auto* parent = button->getParentComponent())
                {
                    for (int sibling = 0; sibling < parent->getNumChildComponents(); ++sibling)
                    {
                        if (auto* other = dynamic_cast<juce::TextButton*>(
                                parent->getChildComponent(sibling)))
                        {
                            if (other->getButtonText() == siblingText)
                                return button;
                        }
                    }
                }
            }
        }
        if (auto* found = findButtonWithSibling(*child, text, siblingText))
            return found;
    }
    return nullptr;
}

juce::TextEditor* findEditor(juce::Component& root, const juce::String& placeholder)
{
    if (auto* editor = dynamic_cast<juce::TextEditor*>(&root))
        if (editor->getTextToShowWhenEmpty() == placeholder)
            return editor;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* found = findEditor(*root.getChildComponent(index), placeholder))
            return found;
    return nullptr;
}

juce::ComboBox* findComboStarting(juce::Component& root, const juce::String& firstItem)
{
    if (auto* box = dynamic_cast<juce::ComboBox*>(&root))
        if (box->getNumItems() > 0 && box->getItemText(0) == firstItem)
            return box;

    for (int index = 0; index < root.getNumChildComponents(); ++index)
        if (auto* found = findComboStarting(*root.getChildComponent(index), firstItem))
            return found;
    return nullptr;
}

juce::String findLabelContaining(juce::Component& root, const juce::String& needle)
{
    if (auto* label = dynamic_cast<juce::Label*>(&root))
        if (label->getText().contains(needle))
            return label->getText();

    for (int index = 0; index < root.getNumChildComponents(); ++index)
    {
        const juce::String found = findLabelContaining(*root.getChildComponent(index), needle);
        if (found.isNotEmpty())
            return found;
    }
    return {};
}

int presetInCategory(const DoomgazerAudioProcessor& processor, const juce::String& category)
{
    for (int index = 0; index < processor.presets().size(); ++index)
        if (processor.presets().entry(index).category == category)
            return index;
    return -1;
}

} // namespace

TEST_CASE("renaming another user preset keeps the current preset selected", "[plugin][batch-b]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    juce::String error;

    REQUIRE(saveCopy(processor, "A", error));
    REQUIRE(saveCopy(processor, "B", error));
    REQUIRE(saveCopy(processor, "C", error));
    processor.loadPreset(processor.presets().indexOfName("B", false));

    const int a = processor.presets().indexOfName("A", false);
    REQUIRE(a >= 0);
    REQUIRE(processor.renamePreset(a, "ZZZ", error));

    REQUIRE(processor.currentPresetName() == "B");
    REQUIRE(processor.currentPresetIndex() == processor.presets().indexOfName("B", false));

    setPlain(processor, "master", -3.0f);
    REQUIRE(processor.saveCurrentPreset({}, error));

    PresetLibrary::Entry saved;
    REQUIRE(PresetLibrary::readFile(processor.presets().fileForName("B"), saved, error));
    REQUIRE(saved.params.masterDb == Catch::Approx(-3.0).margin(0.11));

    PresetLibrary::Entry untouched;
    REQUIRE(PresetLibrary::readFile(processor.presets().fileForName("C"), untouched, error));
    REQUIRE(untouched.params.masterDb != Catch::Approx(-3.0).margin(0.11));
}

TEST_CASE("a long preset name keeps its stored identity", "[plugin][batch-b]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    juce::String error;

    juce::String longName;
    for (int i = 0; i < 80; ++i)
        longName << "X";

    REQUIRE(saveCopy(processor, longName, error));
    REQUIRE(processor.currentPresetIndex() >= 0);
    REQUIRE(processor.currentPresetName().length() <= 64);
    REQUIRE(processor.currentPresetIndex()
            == processor.presets().indexOfName(processor.currentPresetName(), false));
    REQUIRE(processor.saveCurrentPreset({}, error));

    PresetLibrary::Entry stored;
    REQUIRE(PresetLibrary::readFile(
        processor.presets().fileForName(processor.currentPresetName()), stored, error));
    REQUIRE(juce::String(stored.name) == processor.currentPresetName());
}

TEST_CASE("unicode preset names get distinct files", "[plugin][batch-b]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    juce::String error;

    REQUIRE(saveCopy(processor, juce::String::fromUTF8("\xE3\x83\x89\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\xB3"), error));
    REQUIRE(saveCopy(processor, juce::String::fromUTF8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"), error));

    const int drones = processor.presets().indexOfName(
        juce::String::fromUTF8("\xE3\x83\x89\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\xB3"), false);
    const int japanese = processor.presets().indexOfName(
        juce::String::fromUTF8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"), false);
    REQUIRE(drones >= 0);
    REQUIRE(japanese >= 0);
    REQUIRE(processor.presets().fileForName(
                juce::String::fromUTF8("\xE3\x83\x89\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\xB3"))
            != processor.presets().fileForName(juce::String::fromUTF8("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E")));
}

TEST_CASE("a failed rename leaves the old preset intact", "[plugin][batch-b]")
{
    TempPresetFolder temp;
    PresetLibrary library(temp.folder);
    juce::String stored;
    juce::String error;

    REQUIRE(library.saveUser("Old", "User", {}, doomgazer::Parameters {}, false, stored, error));
    const juce::File destination = library.fileForName("New");
    REQUIRE(destination.createDirectory().wasOk());

    const int index = library.indexOfName("Old", false);
    REQUIRE(index >= 0);
    REQUIRE_FALSE(library.renameUser(index, "New", stored, error));

    PresetLibrary::Entry entry;
    REQUIRE(library.readFile(library.fileForName("Old"), entry, error));
    REQUIRE(entry.name == "Old");
}

TEST_CASE("the host program index is the PC code", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    juce::String error;

    REQUIRE(processor.getNumPrograms() == 128);
    REQUIRE(processor.getCurrentProgram() == 0);
    REQUIRE(processor.getProgramName(0) == "TWIN CORE");
    REQUIRE(processor.getProgramName(120) == "---");

    REQUIRE(saveCopy(processor, "Unsaved Candidate", error));
    const int index = processor.presets().indexOfName("Unsaved Candidate", false);
    REQUIRE(index >= 0);
    processor.loadPreset(index);
    const juce::String loaded = processor.currentPresetName();

    // Before audio runs, the host's stored program is not a preset load.
    processor.setCurrentProgram(index);
    processor.changeProgramName(index, "Renamed By Host");
    dispatchMessages();
    REQUIRE(processor.currentPresetName() == loaded);
    REQUIRE(processor.getNumPrograms() == 128);

    REQUIRE(processor.deletePreset(index, error));
    REQUIRE(processor.currentPresetIndex() == -1);
    REQUIRE(processor.currentPresetName() == "UNSAVED");
    processor.setCurrentProgram(0);
    dispatchMessages();
    REQUIRE(processor.currentPresetName() == "UNSAVED");
}

TEST_CASE("saving a preset does not tell the host its program list changed", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    ProgramChangeListener listener;
    processor.addListener(&listener);
    juce::String error;

    REQUIRE(saveCopy(processor, "Notified Program", error));
    const int index = processor.presets().indexOfName("Notified Program", false);
    REQUIRE(index >= 0);
    REQUIRE(processor.renamePreset(index, "Notified Program Renamed", error));
    REQUIRE(processor.deletePreset(processor.presets().indexOfName("Notified Program Renamed", false),
                                   error));
    REQUIRE(listener.programChanges == 0);
    REQUIRE(processor.getNumPrograms() == 128);

    processor.removeListener(&listener);
}

TEST_CASE("presets recall sag, anti-fizz and sub-oct level", "[plugin][batch-c1]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    processor.prepareToPlay(48000.0, 512);

    const int dynamic = processor.presets().indexOfName(
        juce::String(doomgazer::presetName(1)), true);
    REQUIRE(dynamic >= 0);
    const doomgazer::Parameters expected = doomgazer::factoryPreset(1);
    processor.loadPreset(dynamic);

    REQUIRE(processor.apvts.getRawParameterValue("antiFizz")->load()
            == Catch::Approx(expected.antiFizzHz).margin(0.5));
    REQUIRE(processor.apvts.getRawParameterValue("sag")->load()
            == Catch::Approx(expected.sag).margin(0.001));
    REQUIRE(processor.apvts.getRawParameterValue("subOctLevel")->load()
            == Catch::Approx(expected.subOctLevel).margin(0.001));

    juce::MidiBuffer empty;
    for (int block = 0; block < 8; ++block)
        processBlock(processor, empty);

    REQUIRE(processor.engine().parameters().antiFizzHz
            == Catch::Approx(expected.antiFizzHz).margin(0.5));
    REQUIRE(processor.engine().parameters().sag == Catch::Approx(expected.sag).margin(0.001));
    REQUIRE(processor.engine().parameters().subOctLevel
            == Catch::Approx(expected.subOctLevel).margin(0.001));
    REQUIRE_FALSE(processor.presetIsDirty());

    setPlain(processor, "antiFizz", 1230.0f);
    REQUIRE(processor.presetIsDirty());
}

TEST_CASE("a saved user preset loads the sound that was written", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;

    {
        DoomgazerAudioProcessor processor(temp.folder);
        processor.prepareToPlay(48000.0, 512);
        setPlain(processor, "master", -9.0f);
        setPlain(processor, "sag", 0.80f);

        juce::String error;
        REQUIRE(saveCopy(processor, "CELLAR", error));
        const int saved = processor.currentPresetIndex();
        REQUIRE(saved >= 0);
        REQUIRE_FALSE(processor.presets().entry(saved).factory);
        REQUIRE(processor.presets().entry(saved).params.masterDb == Catch::Approx(-9.0).margin(0.11));
        REQUIRE(processor.presets().entry(saved).params.sag == Catch::Approx(0.80).margin(0.011));

        setPlain(processor, "master", 0.0f);
        processor.loadPreset(saved);
        juce::MidiBuffer empty;
        for (int block = 0; block < 8; ++block)
            processBlock(processor, empty);

        REQUIRE(processor.engine().parameters().masterDb == Catch::Approx(-9.0).margin(0.11));
        REQUIRE(processor.engine().parameters().sag == Catch::Approx(0.80).margin(0.011));
        REQUIRE(processor.currentPresetName() == "CELLAR");
        REQUIRE_FALSE(processor.currentPresetIsFactory());
        REQUIRE_FALSE(processor.presetIsDirty());

        setPlain(processor, "master", -3.0f);
        REQUIRE(processor.saveCurrentPreset({}, error));
        setPlain(processor, "master", 1.0f);
        processor.loadPreset(processor.currentPresetIndex());
        for (int block = 0; block < 8; ++block)
            processBlock(processor, empty);
        REQUIRE(processor.engine().parameters().masterDb == Catch::Approx(-3.0).margin(0.11));
    }

    DoomgazerAudioProcessor reopened(temp.folder);
    reopened.prepareToPlay(48000.0, 512);
    const int index = reopened.presets().indexOfName("CELLAR", false);
    REQUIRE(index >= 0);
    REQUIRE_FALSE(reopened.presets().entry(index).factory);
    reopened.loadPreset(index);
    juce::MidiBuffer empty;
    for (int block = 0; block < 8; ++block)
        processBlock(reopened, empty);
    REQUIRE(reopened.engine().parameters().masterDb == Catch::Approx(-3.0).margin(0.11));
    REQUIRE(reopened.engine().parameters().sag == Catch::Approx(0.80).margin(0.011));
    REQUIRE(reopened.currentPresetName() == "CELLAR");
    REQUIRE_FALSE(reopened.currentPresetIsFactory());
}

TEST_CASE("a restored user preset survives the host program that follows it", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor source(temp.folder);
    source.prepareToPlay(48000.0, 512);

    juce::String error;
    REQUIRE(saveCopy(source, "CELLAR", error));
    setPlain(source, "master", 4.0f);

    juce::MemoryBlock state;
    source.getStateInformation(state);

    DoomgazerAudioProcessor target(temp.folder);
    target.prepareToPlay(48000.0, 512);
    target.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    // The host follows a restored chunk with the program it stored. That happens
    // before audio, and it must not replace the project sound.
    target.setCurrentProgram(0);
    target.setCurrentProgram(3);
    dispatchMessages();
    juce::MidiBuffer empty;
    for (int block = 0; block < 8; ++block)
        processBlock(target, empty);
    REQUIRE(target.engine().parameters().masterDb == Catch::Approx(4.0).margin(0.11));
    REQUIRE(target.currentPresetName() == "CELLAR");
}

TEST_CASE("the preset browser loads the category it shows and the preset it just saved",
          "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    processor.prepareToPlay(48000.0, 512);

    const juce::StringArray categories = processor.presets().categories();
    REQUIRE(categories.size() >= 2);
    const int inFirst = presetInCategory(processor, categories[0]);
    REQUIRE(inFirst >= 0);
    processor.loadPreset(inFirst);

    DoomgazerAudioProcessorEditor editor(processor);
    editor.setSize(1200, 580);

    auto* categoryBox = findComboStarting(editor, "All categories");
    REQUIRE(categoryBox != nullptr);
    auto* load = findButton(editor, "LOAD");
    REQUIRE(load != nullptr);

    // The second category used to filter as the first one, so LOAD recalled the
    // row that was already selected instead of the category on the menu.
    categoryBox->setSelectedId(3, juce::sendNotificationSync);
    load->triggerClick();
    dispatchMessages();
    REQUIRE(findLabelContaining(editor, "Select a preset first") == "Select a preset first");
    REQUIRE(processor.currentPresetName() == processor.presets().entry(inFirst).name);

    categoryBox->setSelectedId(2, juce::sendNotificationSync);
    load->triggerClick();
    dispatchMessages();
    REQUIRE(findLabelContaining(editor, "Loaded")
                .startsWith("Loaded " + processor.presets().entry(inFirst).name));
    REQUIRE(processor.currentPresetName() == processor.presets().entry(inFirst).name);

    setPlain(processor, "master", 4.0f);
    auto* headerSave = findButton(editor, "SAVE");
    REQUIRE(headerSave != nullptr);
    headerSave->triggerClick();
    dispatchMessages();

    auto* name = findEditor(editor, "Preset name");
    REQUIRE(name != nullptr);
    name->setText("CELLAR", juce::dontSendNotification);

    auto* commit = findButtonWithSibling(editor, "SAVE", "CANCEL");
    REQUIRE(commit != nullptr);
    REQUIRE(commit != headerSave);
    commit->triggerClick();
    dispatchMessages();

    // LOAD used to keep the factory row that was selected before the save.
    load->triggerClick();
    dispatchMessages();
    REQUIRE(processor.currentPresetName() == "CELLAR");

    juce::MidiBuffer empty;
    for (int block = 0; block < 8; ++block)
        processBlock(processor, empty);
    REQUIRE(processor.engine().parameters().masterDb == Catch::Approx(4.0).margin(0.11));
}

TEST_CASE("preset stepping wraps both ways", "[plugin][preset]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor processor;
    const int last = processor.presets().size() - 1;
    REQUIRE(last > 0);

    processor.loadPreset(last);
    dispatchMessages();
    REQUIRE(processor.currentPresetIndex() == last);

    processor.stepPreset(1);
    REQUIRE(processor.currentPresetIndex() == 0);
    REQUIRE(processor.currentPresetName() == processor.presets().entry(0).name);

    // The host's program slot is not the browser row.
    processor.setCurrentProgram(last);
    REQUIRE(processor.currentPresetIndex() == 0);

    processor.loadPreset(0);
    dispatchMessages();
    processor.stepPreset(-1);
    REQUIRE(processor.currentPresetIndex() == last);
    REQUIRE(processor.currentPresetName() == processor.presets().entry(last).name);
}

TEST_CASE("leaving a dirty preset keeps the edit", "[plugin][preset]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor processor;
    processor.loadPreset(0);
    const float factoryMaster = rawValue(processor, "master");

    setPlain(processor, "master", 4.0f);
    REQUIRE(processor.presetIsDirty());

    processor.stepPreset(1);
    REQUIRE(processor.currentPresetIndex() == 1);
    REQUIRE_FALSE(processor.presetIsDirty());

    processor.stepPreset(-1);
    REQUIRE(processor.currentPresetIndex() == 0);
    REQUIRE(rawValue(processor, "master") == Catch::Approx(4.0f).margin(0.11f));
    REQUIRE(processor.presetIsDirty());

    processor.revertPreset();
    REQUIRE_FALSE(processor.presetIsDirty());
    REQUIRE(rawValue(processor, "master") == Catch::Approx(factoryMaster).margin(0.11f));

    processor.stepPreset(1);
    processor.stepPreset(-1);
    REQUIRE(processor.currentPresetIndex() == 0);
    REQUIRE_FALSE(processor.presetIsDirty());
    REQUIRE(rawValue(processor, "master") == Catch::Approx(factoryMaster).margin(0.11f));
}

TEST_CASE("program changes follow the assigned PC, including a blank", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    processor.prepareToPlay(48000.0, 512);

    const int twin = processor.presets().indexOfName("TWIN CORE", true);
    const int shields = processor.presets().indexOfName("MAXIMUM SHIELDS", true);
    const int slowdive = processor.presets().indexOfName("SLOWDIVE SOFT FOCUS", true);
    const int wool = processor.presets().indexOfName("WOOL VEIL", true);
    REQUIRE(twin >= 0);
    REQUIRE(shields >= 0);
    REQUIRE(slowdive >= 0);
    REQUIRE(wool >= 0);
    REQUIRE(processor.presets().programCode(twin) == 0);
    REQUIRE(processor.presets().programCode(shields) == 4);

    juce::String error;
    REQUIRE(processor.setPresetProgram(twin, -1, error));
    REQUIRE(error.isEmpty());
    REQUIRE(processor.presets().programCode(twin) == -1);

    processor.loadPreset(wool);
    dispatchMessages();
    sendProgram(processor, 0);
    REQUIRE(processor.currentPresetIndex() == wool);

    REQUIRE(processor.setPresetProgram(shields, 40, error));
    dispatchMessages();
    sendProgram(processor, 40);
    REQUIRE(processor.currentPresetIndex() == shields);

    REQUIRE(saveCopy(processor, "CELLAR", error));
    const int cellar = processor.currentPresetIndex();
    REQUIRE_FALSE(processor.presets().entry(cellar).factory);
    REQUIRE(processor.presets().programCode(cellar) == -1);
    REQUIRE(processor.setPresetProgram(cellar, 70, error));
    REQUIRE(processor.presets().programCode(cellar) == 70);
    REQUIRE(processor.presets().entry(cellar).file.loadFileAsString().contains("pc=70"));

    dispatchMessages();
    sendProgram(processor, 70);
    REQUIRE(processor.currentPresetName() == "CELLAR");

    const int slowdiveNow = processor.presets().indexOfName("SLOWDIVE SOFT FOCUS", true);
    REQUIRE(slowdiveNow >= 0);
    REQUIRE(processor.setPresetProgram(slowdiveNow, 70, error));
    REQUIRE(processor.presets().programCode(
                processor.presets().indexOfName("CELLAR", false)) == -1);
    REQUIRE(processor.presets().programCode(slowdiveNow) == 70);
    dispatchMessages();
    sendProgram(processor, 70);
    REQUIRE(processor.currentPresetIndex() == slowdiveNow);

    DoomgazerAudioProcessor again(temp.folder);
    const int againCellar = again.presets().indexOfName("CELLAR", false);
    REQUIRE(again.presets().programCode(again.presets().indexOfName("TWIN CORE", true)) == -1);
    REQUIRE(again.presets().programCode(again.presets().indexOfName("MAXIMUM SHIELDS", true)) == 40);
    REQUIRE(again.presets().programCode(again.presets().indexOfName("SLOWDIVE SOFT FOCUS", true)) == 70);
    REQUIRE(againCellar >= 0);
    REQUIRE(again.presets().programCode(againCellar) == -1);
}

TEST_CASE("a repeated program change stays on the preset that owns that PC", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    processor.prepareToPlay(48000.0, 512);
    juce::String error;

    // Two user presets push the list past PC 34, so list slot 34 is not the
    // factory preset that owns that code.
    REQUIRE(saveCopy(processor, "AAA", error));
    REQUIRE(saveCopy(processor, "ZZZ", error));
    REQUIRE(processor.presets().size() > 34);

    const int twin = processor.presets().indexOfName("TWIN CORE", true);
    REQUIRE(twin >= 0);
    REQUIRE(twin != 34);
    REQUIRE(processor.setPresetProgram(twin, 34, error));

    const int other = processor.presets().indexOfName("WOOL VEIL", true);
    REQUIRE(other >= 0);
    processor.loadPreset(other);
    dispatchMessages();

    // Before the first block the host's program is not a load. After audio, the
    // same call is PC 34.
    processor.setCurrentProgram(34);
    dispatchMessages();
    REQUIRE(processor.currentPresetName() == "WOOL VEIL");

    juce::MidiBuffer empty;
    processBlock(processor, empty);
    processor.setCurrentProgram(34);
    dispatchMessages();
    REQUIRE(processor.currentPresetName() == "TWIN CORE");
    REQUIRE(processor.presets().programCode(processor.currentPresetIndex()) == 34);

    setPlain(processor, "master", 4.0f);
    processor.setCurrentProgram(34);
    dispatchMessages();
    REQUIRE(processor.currentPresetName() == "TWIN CORE");
    REQUIRE(processor.presets().programCode(processor.currentPresetIndex()) == 34);
    REQUIRE(rawValue(processor, "master") == Catch::Approx(4.0f).margin(0.11f));
}

TEST_CASE("the preset list is alphabetical and factory PCs stay with their sounds", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    juce::String error;

    REQUIRE(processor.currentPresetName() == "TWIN CORE");
    REQUIRE(processor.currentPresetIndex()
            == processor.presets().indexOfName("TWIN CORE", true));

    REQUIRE(saveCopy(processor, "AAA USER", error));
    REQUIRE(saveCopy(processor, "ZZZ USER", error));

    juce::String previous;
    for (int index = 0; index < processor.presets().size(); ++index)
    {
        const juce::String name = processor.presets().entry(index).name;
        if (previous.isNotEmpty())
            REQUIRE(previous.compareIgnoreCase(name) <= 0);
        previous = name;
    }

    REQUIRE(processor.presets().entry(0).name == "AAA USER");
    REQUIRE(processor.presets().entry(processor.presets().size() - 1).name == "ZZZ USER");

    const int twin = processor.presets().indexOfName("TWIN CORE", true);
    REQUIRE(twin > 0);
    REQUIRE(processor.presets().programCode(twin) == 0);
    REQUIRE(processor.presets().entry(twin).factory);
}

TEST_CASE("preset CC selects only while controller mode is on, and a repeat stays", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    processor.prepareToPlay(48000.0, 512);

    const int shields = processor.presets().indexOfName("MAXIMUM SHIELDS", true);
    REQUIRE(shields >= 0);
    REQUIRE(processor.presets().programCode(shields) == 4);

    processor.loadPreset(processor.presets().indexOfName("WOOL VEIL", true));
    dispatchMessages();

    setPlain(processor, "controllerMode", 0.0f);
    setPlain(processor, "presetCc", 32.0f);
    sendController(processor, 32, 4);
    REQUIRE(processor.currentPresetName() == "WOOL VEIL");

    setPlain(processor, "controllerMode", 1.0f);
    sendController(processor, 32, 4);
    REQUIRE(processor.currentPresetName() == "MAXIMUM SHIELDS");
    sendController(processor, 32, 4);
    REQUIRE(processor.currentPresetName() == "MAXIMUM SHIELDS");
}

TEST_CASE("saving, renaming, and deleting keep a program code with its preset", "[plugin][preset]")
{
    JuceGuard guard;
    TempPresetFolder temp;
    DoomgazerAudioProcessor processor(temp.folder);
    processor.prepareToPlay(48000.0, 512);
    juce::String error;

    setPlain(processor, "master", -6.0f);
    REQUIRE(processor.savePresetCopy("CELLAR", "User", "damp", false, error));
    int cellar = processor.presets().indexOfName("CELLAR", false);
    REQUIRE(cellar >= 0);
    REQUIRE_FALSE(processor.presets().entry(cellar).factory);
    REQUIRE(processor.presets().programCode(cellar) == -1);
    REQUIRE(processor.setPresetProgram(cellar, 34, error));

    REQUIRE(processor.savePresetCopy("CELLAR COPY", "Space", {}, false, error));
    const int copy = processor.presets().indexOfName("CELLAR COPY", false);
    REQUIRE(copy >= 0);
    REQUIRE(processor.presets().programCode(copy) == -1);
    cellar = processor.presets().indexOfName("CELLAR", false);
    REQUIRE(processor.presets().programCode(cellar) == 34);
    REQUIRE(processor.currentPresetName() == "CELLAR COPY");

    setPlain(processor, "master", -2.0f);
    REQUIRE(processor.saveCurrentPreset("drier", error));
    REQUIRE(processor.currentPresetNotes() == "drier");
    REQUIRE(processor.presets().programCode(processor.currentPresetIndex()) == -1);

    REQUIRE(processor.renamePreset(cellar, "CELLAR RENAMED", error));
    const int renamed = processor.presets().indexOfName("CELLAR RENAMED", false);
    REQUIRE(renamed >= 0);
    REQUIRE(processor.presets().indexOfName("CELLAR", false) < 0);
    REQUIRE(processor.presets().programCode(renamed) == 34);

    REQUIRE_FALSE(processor.savePresetCopy("CELLAR RENAMED", "User", {}, false, error));
    REQUIRE(processor.presets().programCode(
                processor.presets().indexOfName("CELLAR RENAMED", false)) == 34);

    processor.loadPreset(renamed);
    setPlain(processor, "master", 2.0f);
    REQUIRE(processor.saveCurrentPreset("wet", error));
    REQUIRE(processor.presets().programCode(processor.currentPresetIndex()) == 34);
    REQUIRE(processor.currentPresetNotes() == "wet");

    {
        DoomgazerAudioProcessor again(temp.folder);
        const int persisted = again.presets().indexOfName("CELLAR RENAMED", false);
        REQUIRE(persisted >= 0);
        REQUIRE(again.presets().programCode(persisted) == 34);
        REQUIRE(again.presets().entry(persisted).notes == "wet");
        REQUIRE(again.presets().entry(persisted).params.masterDb == Catch::Approx(2.0).margin(0.11));
        REQUIRE(again.presets().indexOfName("CELLAR COPY", false) >= 0);
    }

    REQUIRE(processor.deletePreset(processor.presets().indexOfName("CELLAR RENAMED", false), error));
    REQUIRE(processor.presets().indexOfName("CELLAR RENAMED", false) < 0);

    processor.loadPreset(processor.presets().indexOfName("TWIN CORE", true));
    REQUIRE_FALSE(processor.saveCurrentPreset({}, error));
    REQUIRE(error == "Factory presets cannot be overwritten. Use Save As.");
}

TEST_CASE("a learned core switch toggles on the next press", "[plugin][midi]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    const float before = rawValue(processor, "engine");
    REQUIRE(before == Catch::Approx(1.0f).margin(0.01f));

    processor.beginLearn(1);
    sendController(processor, 21, 127);
    REQUIRE(processor.ccForParameter(1) == 21);
    REQUIRE(rawValue(processor, "engine") == Catch::Approx(before).margin(0.01f));

    sendController(processor, 21, 127);
    REQUIRE(rawValue(processor, "engine") == Catch::Approx(0.0f).margin(0.01f));

    sendController(processor, 21, 0);
    REQUIRE(rawValue(processor, "engine") == Catch::Approx(0.0f).margin(0.01f));
    sendController(processor, 21, 127);
    REQUIRE(rawValue(processor, "engine") == Catch::Approx(1.0f).margin(0.01f));
}

TEST_CASE("a new project loads the LPD8 map", "[plugin][midi]")
{
    JuceGuard guard;
    DoomgazerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    REQUIRE(processor.ccForParameter(2) == 12);
    REQUIRE(processor.ccForParameter(24) == 43);
    REQUIRE(processor.ccForParameter(44) == 32);
    REQUIRE(processor.ccForParameter(66) == 92);
    REQUIRE(processor.ccForParameter(94) == 26);
    REQUIRE(processor.ccForParameter(95) == 104);
    REQUIRE(rawValue(processor, "presetCc") == Catch::Approx(103.0f).margin(0.01f));
    REQUIRE(processor.ccForParameter(1) == -1);
    REQUIRE(processor.stompBindingLabel(static_cast<int>(doomgazer::Stomp::doubler)) == "CC 70");
    REQUIRE(processor.stompBindingLabel(static_cast<int>(doomgazer::Stomp::core)) == "CC 91");
    REQUIRE(processor.stompBindingLabel(static_cast<int>(doomgazer::Stomp::presetNext)) == "CC 90");

    std::vector<int> used(128, 0);
    int bound = 0;
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const int cc = processor.ccForParameter(index);
        if (cc < 0)
            continue;
        REQUIRE(cc != 103);
        REQUIRE(used[static_cast<std::size_t>(cc)] == 0);
        used[static_cast<std::size_t>(cc)] = 1;
        ++bound;
    }
    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
    {
        const juce::String label = processor.stompBindingLabel(stomp);
        REQUIRE(label.startsWith("CC "));
        const int cc = label.fromFirstOccurrenceOf("CC ", false, false).getIntValue();
        REQUIRE(used[static_cast<std::size_t>(cc)] == 0);
        used[static_cast<std::size_t>(cc)] = 1;
        ++bound;
    }
    REQUIRE(bound == 61);

    const float driveBefore = rawValue(processor, "coreGain");
    sendController(processor, 12, 127);
    REQUIRE(rawValue(processor, "coreGain") > driveBefore + 1.0f);

    sendController(processor, 92, 127);
    REQUIRE(rawValue(processor, "freeze") == Catch::Approx(1.0f).margin(0.01f));
    sendController(processor, 92, 0);
    REQUIRE(rawValue(processor, "freeze") == Catch::Approx(0.0f).margin(0.01f));

    const float driftBefore = rawValue(processor, "doubler");
    sendController(processor, 70, 127);
    REQUIRE(rawValue(processor, "doubler") == Catch::Approx(driftBefore > 0.5f ? 0.0f : 1.0f).margin(0.01f));

    processor.clearAllMappings();
    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
        processor.clearStompBinding(stomp);

    juce::MemoryBlock block;
    processor.getStateInformation(block);
    DoomgazerAudioProcessor again;
    again.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(again.ccForParameter(2) == -1);
    REQUIRE(again.ccForParameter(66) == -1);
    REQUIRE_FALSE(again.stompIsBound(static_cast<int>(doomgazer::Stomp::doubler)));
    REQUIRE_FALSE(again.stompIsBound(static_cast<int>(doomgazer::Stomp::core)));
}

TEST_CASE("release tags compare as dotted numbers", "[release]")
{
    REQUIRE_FALSE(isNewerRelease("v0.1.1", "0.1.1"));
    REQUIRE_FALSE(isNewerRelease("V0.1.1-beta", "0.1.1"));
    REQUIRE_FALSE(isNewerRelease("0.1.1-rc.1", "0.1.1"));
    REQUIRE_FALSE(isNewerRelease("0.1", "0.1.0"));
    REQUIRE_FALSE(isNewerRelease("0.1.0", "0.1.1"));
    REQUIRE_FALSE(isNewerRelease("", "0.1.1"));
    REQUIRE_FALSE(isNewerRelease("not-a-version", "0.1.1"));
    REQUIRE(isNewerRelease("v0.1.2", "0.1.1"));
    REQUIRE(isNewerRelease("0.2.0", "0.1.9"));
    REQUIRE(isNewerRelease("1.0.0", "0.9.9"));
    REQUIRE(isNewerRelease("1.10.0", "1.9.0"));
    REQUIRE(isNewerRelease("0.2.0-rc1", "0.1.1"));
}
