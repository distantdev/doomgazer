#pragma once

#include "HardwareBanks.h"
#include "PresetLibrary.h"
#include "doomgazer/Processor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

class HiddenParameterHost;

class DoomgazerAudioProcessor : public juce::AudioProcessor,
                                private juce::AsyncUpdater
{
public:
    explicit DoomgazerAudioProcessor(juce::File presetFolder = {});
    ~DoomgazerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Doomgazer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    // Host program index is the MIDI PC code, 0-127. The browser list is separate.
    static constexpr int kHostProgramCount = 128;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    juce::RangedAudioParameter* findParameter(const juce::String& parameterID) const;
    std::atomic<float>* findRawValue(const juce::String& parameterID) const;
    juce::AudioProcessorValueTreeState& stateFor(const juce::String& parameterID);

    // Presets -----------------------------------------------------------------
    PresetLibrary& presets() noexcept { return library; }
    const PresetLibrary& presets() const noexcept { return library; }
    static juce::File presetFolder();

    void refreshPresets();
    int currentPresetIndex() const;
    juce::String currentPresetName() const;
    juce::String currentPresetCategory() const;
    juce::String currentPresetNotes() const;
    bool currentPresetIsFactory() const;
    bool presetIsDirty() const;

    // Reads the parameters and installs them through the DSP fade mailbox.
    void loadPreset(int index);
    void stepPreset(int delta);
    void revertPreset();
    // code is 0-127, or -1 for a blank PC that program-change switching skips.
    bool setPresetProgram(int index, int code, juce::String& error);

    bool savePresetCopy(const juce::String& name, const juce::String& category,
                        const juce::String& notes, bool allowOverwrite, juce::String& error);
    bool saveCurrentPreset(const juce::String& notes, juce::String& error);
    bool renamePreset(int index, const juce::String& newName, juce::String& error);
    bool deletePreset(int index, juce::String& error);

    void toggleCompare();
    int compareSlot() const noexcept { return abActive; }

    // MIDI --------------------------------------------------------------------
    int ccForParameter(int index) const noexcept;
    juce::String mappingLabel(int index) const;
    int learnTarget() const noexcept { return learnIndex.load(std::memory_order_relaxed); }
    void beginLearn(int index) noexcept;
    void clearMapping(int index);
    void clearAllMappings();
    int midiChannel() const noexcept { return channel.load(std::memory_order_relaxed); }
    void setMidiChannel(int newChannel);
    bool programChangeEnabled() const noexcept
    {
        return programChangeOn.load(std::memory_order_relaxed);
    }
    void setProgramChangeEnabled(bool enabled);
    int midiActivity() const noexcept { return activity.load(std::memory_order_relaxed); }
    int midiMovedParameter() const noexcept
    {
        return movedParameter.load(std::memory_order_relaxed);
    }
    int midiMoveTick() const noexcept { return moveTick.load(std::memory_order_relaxed); }

    // Stomps ------------------------------------------------------------------
    bool stompIsOn(int stomp) const noexcept;
    void toggleStomp(int stomp) noexcept;
    int stompLearnTarget() const noexcept
    {
        return stompArmed.load(std::memory_order_acquire);
    }
    void beginStompLearn(int stomp) noexcept;
    // Empty when nothing is bound. "CC 64" or "NOTE 36" otherwise.
    juce::String stompBindingLabel(int stomp) const;
    bool stompIsBound(int stomp) const noexcept;
    void clearStompBinding(int stomp);
    // The mode a module will come back to when it is switched on again.
    int stompHeldMode(int stomp) const noexcept;

    // Rig ---------------------------------------------------------------------
    void setSubPitch(int mode) noexcept;
    int subPitch() const noexcept;
    void setOutputMono(bool mono) noexcept;
    bool outputMono() const noexcept;
    void setMonoPaInvertR(bool invert) noexcept;
    bool monoPaInvertR() const noexcept;

    void servicePresetHold();
    doomgazer::Parameters captureSound() const;

    doomgazer::Processor& engine() noexcept { return dsp; }
    const doomgazer::Processor& engine() const noexcept { return dsp; }

private:
    void handleAsyncUpdate() override;
    void handleMidi(const juce::MidiBuffer& midi);
    void queueHostNotification(int parameterIndex) noexcept;
    void queueStomp(int stomp) noexcept;

    void bindMapping(int index, int cc);
    void applyStompBinding(int stomp, int kind, int number);
    void armControllerEdge(int cc) noexcept;
    static juce::String editKey(bool factory, const juce::String& name);
    void storeMidiState();
    void loadMidiState();
    void installLpd8Map() noexcept;
    void storePresetIdentity();
    void restorePresetIdentity();
    void noteReportedProgram();
    void storeCompareState();
    void restoreCompareState();
    void storeRigState();
    void loadRigState();
    void applySound(const doomgazer::Parameters& sound, bool immediate);
    doomgazer::Parameters readParameters() const;
    void writeParameters(const doomgazer::Parameters& params);
    void setPlainValue(const char* id, float plain) noexcept;
    float plainValue(const char* id) const noexcept;
    void rememberStompLevels() noexcept;
    static bool parameterIsProjectSetting(int index) noexcept;
    static doomgazer::Parameters overlayProjectSettings(const doomgazer::Parameters& musical,
                                                        const doomgazer::Parameters& live);

    doomgazer::Processor dsp;
    std::unique_ptr<HiddenParameterHost> hiddenHost;
    juce::AudioProcessorValueTreeState hiddenApvts;
    PresetLibrary library;

    mutable juce::CriticalSection presetLock;
    doomgazer::Parameters baseline;
    // Edits kept for the session when a dirty preset is left. The file stays
    // untouched, so coming back restores the * and the sound.
    std::map<juce::String, doomgazer::Parameters> editedSounds;
    int currentPreset = 0;
    juce::String currentName;
    juce::String currentCategory;
    juce::String currentNotes;
    bool currentFactory = true;

    std::array<doomgazer::Parameters, 2> abSound {};
    int abActive = 0;

    std::atomic<bool> holdHostParams { false };
    int holdMiss = 0;
    doomgazer::Parameters lastParameters;
    bool haveLastParameters = false;
    bool prepared = false;

    std::array<std::atomic<int>, doomgazer::kParameterCount + 1> ccMap;
    std::array<std::atomic<int>, doomgazer::kParameterCount + 1> pendingCc;
    std::array<std::atomic<int>, doomgazer::kParameterCount + 1> lastCc;
    std::atomic<int> learnIndex { -1 };
    std::atomic<int> pendingLearnIndex { -1 };
    std::atomic<int> pendingLearnCc { -1 };
    std::atomic<int> channel { 0 };
    std::atomic<bool> programChangeOn { true };
    std::atomic<int> pendingProgram { -1 };
    // The PC of the preset on screen, or 0 when that preset's code is blank.
    std::atomic<int> reportedProgram { 0 };
    // Host program changes wait until a block has run, and a state restore
    // closes the gate again. MIDI program change does not consult this.
    std::atomic<bool> acceptHostProgram { false };
    std::atomic<int> activity { 0 };
    std::atomic<int> movedParameter { -1 };
    std::atomic<int> moveTick { 0 };

    // Action-button bindings. Kind 0 is nothing, 1 is a CC, 2 is a note.
    std::array<std::atomic<int>, doomgazer::kStompCount> stompKind;
    std::array<std::atomic<int>, doomgazer::kStompCount> stompNumber;
    std::atomic<int> stompArmed { -1 };
    std::atomic<int> pendingPresetStep { 0 };
    std::atomic<std::uint32_t> pendingStomps { 0 };
    std::atomic<int> pendingStompTarget { -1 };
    std::atomic<int> pendingStompKind { 0 };
    std::atomic<int> pendingStompNumber { -1 };
    std::array<int, 128> lastControllerValue {};
    std::atomic<std::uint64_t> resetEdgeLo { 0 };
    std::atomic<std::uint64_t> resetEdgeHi { 0 };
    std::atomic<int> heldStompGlide { 1 };
    std::atomic<int> heldStompDelay { 1 };
    std::atomic<int> heldStompReverb { 1 };
    std::atomic<float> heldSwell { 0.0f };
    std::atomic<float> heldGrit { 0.0f };
    std::atomic<float> heldPhase { 0.0f };
    std::atomic<float> heldChorus { 0.0f };
    std::atomic<float> heldHowl { 0.0f };

    std::atomic<int> subPitchMode { 0 };
    std::atomic<int> outputMonoMode { 0 };
    std::atomic<int> monoPaInvertRMode { 0 };
    std::atomic<std::uint64_t> hostNotifyBits { 0 };
    std::atomic<std::uint64_t> hostNotifyBitsHi { 0 };

    // Resolved once so the audio callback never looks a parameter up by string.
    std::array<std::atomic<float>*, doomgazer::kParameterCount + 1> raw {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoomgazerAudioProcessor)
};
