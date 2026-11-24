#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>


namespace {

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.reserve(static_cast<std::size_t>(doomgazer::kParameterCount));
    const doomgazer::Parameters defaults;

    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const doomgazer::ParameterRange range = doomgazer::parameterRange(index);
        juce::NormalisableRange<float> span(
            static_cast<float>(range.min),
            static_cast<float>(range.max),
            static_cast<float>(range.step));

        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { doomgazer::parameterId(index), 1 },
            doomgazer::parameterName(index),
            span,
            static_cast<float>(defaults.at(index)),
            juce::AudioParameterFloatAttributes().withLabel(doomgazer::parameterUnit(index))));
    }

    return { parameters.begin(), parameters.end() };
}

// Parameters travel in the project file as CSV so A/B and preset identity
// survive a DAW reload without a second schema.
juce::String packSound(const doomgazer::Parameters& params)
{
    juce::String text;
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        if (index > 1)
            text << ",";
        text << juce::String(params.at(index), 6);
    }
    return text;
}

bool unpackSound(const juce::String& text, doomgazer::Parameters& params)
{
    const juce::StringArray tokens = juce::StringArray::fromTokens(text, ",", "");
    if (tokens.size() != doomgazer::kParameterCount)
        return false;

    doomgazer::Parameters parsed;
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const juce::String token = tokens[index - 1].trim();
        if (token.isEmpty())
            return false;
        parsed.at(index) = token.getDoubleValue();
    }
    parsed.clampIntoRange();
    params = parsed;
    return true;
}

juce::Identifier ccProperty(int index)
{
    return juce::Identifier(juce::String("midi.cc.") + doomgazer::parameterId(index));
}

juce::Identifier stompProperty(int stomp)
{
    return juce::Identifier("midi.stomp." + juce::String(stomp));
}

bool sameParameters(const doomgazer::Parameters& left, const doomgazer::Parameters& right) noexcept
{
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
        if (left.at(index) != right.at(index))
            return false;
    return left.subPitch == right.subPitch;
}

} // namespace

DoomgazerAudioProcessor::DoomgazerAudioProcessor(juce::File presetFolder)
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout()),
      library(presetFolder)
{
    for (auto& value : ccMap)
        value.store(-1, std::memory_order_relaxed);
    for (auto& value : pendingCc)
        value.store(-1, std::memory_order_relaxed);
    for (auto& value : lastCc)
        value.store(0, std::memory_order_relaxed);
    for (auto& kind : stompKind)
        kind.store(0, std::memory_order_relaxed);
    for (auto& number : stompNumber)
        number.store(-1, std::memory_order_relaxed);
    lastControllerValue.fill(-1);

    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
        raw[static_cast<std::size_t>(index)] = apvts.getRawParameterValue(doomgazer::parameterId(index));

    loadMidiState();
    loadRigState();
    restoreCompareState();

    currentName = juce::String(doomgazer::presetName(0));
    currentCategory = juce::String(doomgazer::presetCategory(0));
    currentFactory = true;
    currentPreset = library.indexOfName(currentName, true);
    noteReportedProgram();
    baseline = doomgazer::factoryPreset(0);

    abSound[0] = baseline;
    abSound[1] = baseline;

    applySound(baseline, true);
    setPlainValue("presetSlot", static_cast<float>(juce::jmax(0, currentPreset)));

    storePresetIdentity();
    storeCompareState();
}

DoomgazerAudioProcessor::~DoomgazerAudioProcessor()
{
    cancelPendingUpdate();
}

void DoomgazerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate, samplesPerBlock);
    dsp.reset();
    dsp.setPaMonoPolicy(outputMono(), monoPaInvertR());
    lastParameters = readParameters();
    haveLastParameters = true;
    dsp.setParameters(lastParameters);
    prepared = true;
}

void DoomgazerAudioProcessor::releaseResources()
{
    dsp.reset();
    haveLastParameters = false;
    prepared = false;
}

bool DoomgazerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    const bool supported = mainOut == juce::AudioChannelSet::mono()
                        || mainOut == juce::AudioChannelSet::stereo();
    return supported && mainOut == layouts.getMainInputChannelSet();
}

void DoomgazerAudioProcessor::queueHostNotification(int parameterIndex) noexcept
{
    if (parameterIndex < 1 || parameterIndex > doomgazer::kParameterCount)
        return;
    const unsigned bit = static_cast<unsigned>(parameterIndex - 1);
    if (bit < 64)
        hostNotifyBits.fetch_or(1ull << bit, std::memory_order_release);
    else
        hostNotifyBitsHi.fetch_or(1ull << (bit - 64), std::memory_order_release);
    triggerAsyncUpdate();
}

juce::String DoomgazerAudioProcessor::editKey(bool factory, const juce::String& name)
{
    return (factory ? "f:" : "u:") + name;
}

void DoomgazerAudioProcessor::handleAsyncUpdate()
{
    const int learnAt = pendingLearnIndex.exchange(-1, std::memory_order_acq_rel);
    const int learnedCc = pendingLearnCc.exchange(-1, std::memory_order_acq_rel);
    if (learnAt >= 1 && learnedCc >= 0 && learnedCc < 128)
        bindMapping(learnAt, learnedCc);

    const int stompTarget = pendingStompTarget.exchange(-1, std::memory_order_acq_rel);
    if (stompTarget >= 0)
    {
        applyStompBinding(stompTarget,
                          pendingStompKind.exchange(0, std::memory_order_acq_rel),
                          pendingStompNumber.exchange(-1, std::memory_order_acq_rel));
    }

    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const int value = pendingCc[static_cast<std::size_t>(index)].exchange(
            -1, std::memory_order_acq_rel);
        if (value < 0)
            continue;
        if (auto* parameter = apvts.getParameter(doomgazer::parameterId(index)))
            parameter->setValueNotifyingHost(
                juce::jlimit(0.0f, 1.0f, static_cast<float>(value) / 1000.0f));
    }

    const int step = pendingPresetStep.exchange(0, std::memory_order_acq_rel);
    if (step != 0)
        stepPreset(step);

    const std::uint32_t stompBits = pendingStomps.exchange(0, std::memory_order_acq_rel);
    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
        if ((stompBits & (1u << static_cast<unsigned>(stomp))) != 0)
            toggleStomp(stomp);

    // PC codes are not host program slots. The message names the preset that
    // owns the code. A repeat of the current code leaves that preset alone.
    const int program = pendingProgram.exchange(-1, std::memory_order_acq_rel);
    if (program >= 0)
    {
        int resolved = -1;
        {
            const juce::ScopedLock lock(presetLock);
            resolved = library.indexForProgram(program);
        }
        if (resolved >= 0 && resolved != currentPresetIndex())
            loadPreset(resolved);
    }

    const std::uint64_t bits = hostNotifyBits.exchange(0, std::memory_order_acq_rel);
    const std::uint64_t bitsHi = hostNotifyBitsHi.exchange(0, std::memory_order_acq_rel);
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const unsigned bit = static_cast<unsigned>(index - 1);
        const bool pending = bit < 64
            ? (bits & (1ull << bit)) != 0
            : (bitsHi & (1ull << (bit - 64))) != 0;
        if (!pending)
            continue;
        if (auto* parameter = apvts.getParameter(doomgazer::parameterId(index)))
            parameter->sendValueChangedMessageToListeners(parameter->getValue());
    }
}

doomgazer::Parameters DoomgazerAudioProcessor::readParameters() const
{
    doomgazer::Parameters params;
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        if (const auto* value = raw[static_cast<std::size_t>(index)])
            params.at(index) = static_cast<double>(value->load(std::memory_order_relaxed));
    }
    params.clampIntoRange();
    params.subPitch = subPitchMode.load(std::memory_order_acquire);
    return params;
}

void DoomgazerAudioProcessor::writeParameters(const doomgazer::Parameters& params)
{
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        if (parameterIsProjectSetting(index))
            continue;
        if (auto* parameter = apvts.getParameter(doomgazer::parameterId(index)))
        {
            const float plain = static_cast<float>(params.at(index));
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
        }
    }
}

doomgazer::Parameters DoomgazerAudioProcessor::captureSound() const
{
    return readParameters();
}

void DoomgazerAudioProcessor::applySound(const doomgazer::Parameters& sound, bool immediate)
{
    if (prepared && !immediate)
    {
        holdHostParams.store(true, std::memory_order_release);
        holdMiss = 0;
        dsp.requestPreset(sound);
    }
    else
    {
        dsp.setParameters(sound);
        dsp.reset();
    }

    writeParameters(sound);
}

void DoomgazerAudioProcessor::servicePresetHold()
{
    if (!holdHostParams.load(std::memory_order_acquire))
        return;

    if (dsp.consumePresetApplied())
    {
        holdHostParams.store(false, std::memory_order_release);
        holdMiss = 0;
        return;
    }
    if (++holdMiss < 8)
        return;

    // The fade mailbox never delivered. Drop the hold and let the next block
    // apply the host values that writeParameters() already set.
    holdHostParams.store(false, std::memory_order_release);
    holdMiss = 0;
}

void DoomgazerAudioProcessor::setSubPitch(int mode) noexcept
{
    subPitchMode.store(juce::jlimit(0, 2, mode), std::memory_order_release);
}

int DoomgazerAudioProcessor::subPitch() const noexcept
{
    return subPitchMode.load(std::memory_order_acquire);
}

void DoomgazerAudioProcessor::setOutputMono(bool mono) noexcept
{
    outputMonoMode.store(mono ? 1 : 0, std::memory_order_release);
}

bool DoomgazerAudioProcessor::outputMono() const noexcept
{
    return outputMonoMode.load(std::memory_order_acquire) != 0;
}

void DoomgazerAudioProcessor::setMonoPaInvertR(bool invert) noexcept
{
    monoPaInvertRMode.store(invert ? 1 : 0, std::memory_order_release);
}

bool DoomgazerAudioProcessor::monoPaInvertR() const noexcept
{
    return monoPaInvertRMode.load(std::memory_order_acquire) != 0;
}

// Presets ---------------------------------------------------------------------

juce::File DoomgazerAudioProcessor::presetFolder()
{
    return PresetLibrary::userFolder();
}

void DoomgazerAudioProcessor::refreshPresets()
{
    {
        const juce::ScopedLock lock(presetLock);
        library.reload();
        currentPreset = library.indexOfName(currentName, currentFactory);
        noteReportedProgram();
    }
}

void DoomgazerAudioProcessor::storePresetIdentity()
{
    apvts.state.setProperty("presetName", currentName, nullptr);
    apvts.state.setProperty("presetCategory", currentCategory, nullptr);
    apvts.state.setProperty("presetNotes", currentNotes, nullptr);
    apvts.state.setProperty("presetFactory", currentFactory, nullptr);
    apvts.state.setProperty("presetIndex", currentPreset, nullptr);
}

void DoomgazerAudioProcessor::restorePresetIdentity()
{
    const juce::ScopedLock lock(presetLock);

    const juce::String name = apvts.state.getProperty("presetName", juce::String()).toString();
    const juce::String category =
        apvts.state.getProperty("presetCategory", juce::String()).toString();
    const juce::String notes = apvts.state.getProperty("presetNotes", juce::String()).toString();
    const bool factory = static_cast<bool>(apvts.state.getProperty("presetFactory", true));

    if (name.isEmpty())
    {
        // State written before presets had an identity: name the sound if it
        // still matches a factory preset, otherwise be honest about it.
        const doomgazer::Parameters live = readParameters();
        int match = -1;
        for (int index = 0; index < doomgazer::kFactoryPresetCount; ++index)
        {
            if (doomgazer::parametersMatch(
                live, overlayProjectSettings(doomgazer::factoryPreset(index), live)))
            {
                match = index;
                break;
            }
        }

        if (match >= 0)
        {
            currentPreset = match;
            currentName = juce::String(doomgazer::presetName(match));
            currentCategory = juce::String(doomgazer::presetCategory(match));
            currentFactory = true;
        }
        else
        {
            currentPreset = -1;
            currentName = "UNSAVED";
            currentCategory = "User";
            currentFactory = false;
        }
        currentNotes.clear();
        noteReportedProgram();
        return;
    }

    currentName = name;
    currentCategory = category.isEmpty() ? "User" : category;
    currentNotes = notes;
    currentFactory = factory;
    currentPreset = library.indexOfName(name, factory);
    noteReportedProgram();
}

int DoomgazerAudioProcessor::currentPresetIndex() const
{
    const juce::ScopedLock lock(presetLock);
    return currentPreset;
}

juce::String DoomgazerAudioProcessor::currentPresetName() const
{
    const juce::ScopedLock lock(presetLock);
    return currentName;
}

juce::String DoomgazerAudioProcessor::currentPresetCategory() const
{
    const juce::ScopedLock lock(presetLock);
    return currentCategory;
}

juce::String DoomgazerAudioProcessor::currentPresetNotes() const
{
    const juce::ScopedLock lock(presetLock);
    return currentNotes;
}

bool DoomgazerAudioProcessor::currentPresetIsFactory() const
{
    const juce::ScopedLock lock(presetLock);
    return currentFactory;
}

bool DoomgazerAudioProcessor::presetIsDirty() const
{
    doomgazer::Parameters live = readParameters();
    doomgazer::Parameters reference;
    {
        const juce::ScopedLock lock(presetLock);
        reference = baseline;
    }
    // Controller settings and the mirrored preset number are project state, so
    // they must not make a freshly loaded preset look edited.
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        if (!parameterIsProjectSetting(index))
            continue;
        live.at(index) = 0.0;
        reference.at(index) = 0.0;
    }
    return !doomgazer::parametersMatch(live, reference);
}

void DoomgazerAudioProcessor::loadPreset(int index)
{
    const bool leavingDirty = presetIsDirty();
    const doomgazer::Parameters live = readParameters();

    doomgazer::Parameters sound;
    {
        const juce::ScopedLock lock(presetLock);
        if (index < 0 || index >= library.size())
            return;

        const int previous = currentPreset;
        const PresetLibrary::Entry entry = library.entry(index);
        const juce::String arriving = editKey(entry.factory, entry.name);

        if (previous >= 0 && previous != index && leavingDirty)
            editedSounds[editKey(currentFactory, currentName)] = live;
        if (previous == index)
            editedSounds.erase(arriving);

        const auto found = editedSounds.find(arriving);
        const bool restoreEdit = previous != index && found != editedSounds.end();
        doomgazer::Parameters liveWithSlot = live;
        liveWithSlot.presetSlot = static_cast<double>(index);
        const doomgazer::Parameters musical = restoreEdit ? found->second : entry.params;
        sound = overlayProjectSettings(musical, liveWithSlot);
        currentPreset = index;
        currentName = entry.name;
        currentCategory = entry.category;
        currentNotes = entry.notes;
        currentFactory = entry.factory;
        baseline = restoreEdit ? overlayProjectSettings(entry.params, liveWithSlot) : sound;
        storePresetIdentity();
        noteReportedProgram();
    }

    applySound(sound, false);
    setPlainValue("presetSlot", static_cast<float>(index));
}

void DoomgazerAudioProcessor::stepPreset(int delta)
{
    const int count = library.size();
    if (count <= 0 || delta == 0)
        return;

    int next = currentPresetIndex();
    if (next < 0)
        next = delta < 0 ? count - 1 : 0;
    else
        next += delta;

    if (next >= count)
        next = 0;
    else if (next < 0)
        next = count - 1;

    loadPreset(next);
}

void DoomgazerAudioProcessor::revertPreset()
{
    doomgazer::Parameters sound;
    {
        const juce::ScopedLock lock(presetLock);
        if (currentPreset >= 0)
            editedSounds.erase(editKey(currentFactory, currentName));
        sound = baseline;
    }
    applySound(sound, false);
}

bool DoomgazerAudioProcessor::setPresetProgram(int index, int code, juce::String& error)
{
    const juce::ScopedLock lock(presetLock);
    const bool wrote = library.setProgramCode(index, code, error);
    if (wrote)
        noteReportedProgram();
    return wrote;
}

bool DoomgazerAudioProcessor::savePresetCopy(const juce::String& name,
                                             const juce::String& category,
                                             const juce::String& notes,
                                             bool allowOverwrite,
                                             juce::String& error)
{
    const bool dirty = presetIsDirty();
    const doomgazer::Parameters sound = readParameters();
    juce::String storedName;
    {
        const juce::ScopedLock lock(presetLock);
        const juce::String previousKey = currentPreset >= 0 ? editKey(currentFactory, currentName)
                                                            : juce::String();

        if (!library.saveUser(name, category, notes, sound, allowOverwrite, storedName, error))
            return false;

        if (dirty && previousKey.isNotEmpty())
            editedSounds[previousKey] = sound;

        currentName = storedName;
        currentCategory = PresetLibrary::storedCategory(
            category.trim().isEmpty() ? juce::String("User") : category);
        currentNotes = PresetLibrary::storedNotes(notes);
        currentFactory = false;
        currentPreset = library.indexOfName(currentName, false);
        baseline = sound;
        editedSounds.erase(editKey(false, storedName));
        storePresetIdentity();
        noteReportedProgram();
    }
    return true;
}

bool DoomgazerAudioProcessor::saveCurrentPreset(const juce::String& notes, juce::String& error)
{
    const doomgazer::Parameters sound = readParameters();
    const juce::ScopedLock lock(presetLock);

    if (currentFactory)
    {
        error = "Factory presets cannot be overwritten. Use Save As.";
        return false;
    }
    if (currentPreset < 0)
    {
        error = "No user preset is selected. Use Save As.";
        return false;
    }
    if (!library.overwrite(currentPreset, notes, sound, error))
        return false;

    editedSounds.erase(editKey(false, currentName));
    baseline = sound;
    currentNotes = PresetLibrary::storedNotes(notes);
    currentPreset = library.indexOfName(currentName, false);
    storePresetIdentity();
    noteReportedProgram();
    return true;
}

bool DoomgazerAudioProcessor::renamePreset(int index, const juce::String& newName,
                                           juce::String& error)
{
    juce::String storedName;
    {
        const juce::ScopedLock lock(presetLock);
        if (index < 0 || index >= library.size())
        {
            error = "No preset selected";
            return false;
        }
        const juce::String oldName = library.entry(index).name;
        if (!library.renameUser(index, newName, storedName, error))
            return false;

        const juce::String oldKey = editKey(false, oldName);
        const auto found = editedSounds.find(oldKey);
        if (found != editedSounds.end())
        {
            const doomgazer::Parameters stashed = found->second;
            editedSounds.erase(found);
            editedSounds[editKey(false, storedName)] = stashed;
        }

        if (index == currentPreset)
            currentName = storedName;

        // The library is sorted by name, so renaming any user preset can move
        // the current one. Recompute the index from the identity, never reuse
        // the old position.
        currentPreset = library.indexOfName(currentName, currentFactory);
        storePresetIdentity();
        noteReportedProgram();
    }
    return true;
}

bool DoomgazerAudioProcessor::deletePreset(int index, juce::String& error)
{
    {
        const juce::ScopedLock lock(presetLock);
        if (index < 0 || index >= library.size())
        {
            error = "No preset selected";
            return false;
        }
        const juce::String name = library.entry(index).name;
        if (!library.removeUser(index, error))
            return false;
        editedSounds.erase(editKey(false, name));

        if (index == currentPreset)
        {
            // The sound stays; its name does not, because the file that named it is gone.
            currentPreset = -1;
            currentName = "UNSAVED";
            currentCategory = "User";
            currentNotes.clear();
            currentFactory = false;
            baseline = readParameters();
            storePresetIdentity();
        }
        else if (index < currentPreset)
        {
            --currentPreset;
            storePresetIdentity();
        }
        noteReportedProgram();
    }
    return true;
}

// A/B -------------------------------------------------------------------------

void DoomgazerAudioProcessor::storeCompareState()
{
    apvts.state.setProperty("abSlot", abActive, nullptr);
    apvts.state.setProperty("abA", packSound(abSound[0]), nullptr);
    apvts.state.setProperty("abB", packSound(abSound[1]), nullptr);
}

void DoomgazerAudioProcessor::restoreCompareState()
{
    abSound[0] = readParameters();
    abSound[1] = abSound[0];
    unpackSound(apvts.state.getProperty("abA", juce::String()).toString(), abSound[0]);
    unpackSound(apvts.state.getProperty("abB", juce::String()).toString(), abSound[1]);
    abActive = juce::jlimit(0, 1, static_cast<int>(apvts.state.getProperty("abSlot", 0)));
}

void DoomgazerAudioProcessor::toggleCompare()
{
    const doomgazer::Parameters live = readParameters();
    abSound[static_cast<std::size_t>(abActive)] = live;
    abActive = 1 - abActive;
    storeCompareState();
    applySound(abSound[static_cast<std::size_t>(abActive)], false);
}

// MIDI ------------------------------------------------------------------------

int DoomgazerAudioProcessor::ccForParameter(int index) const noexcept
{
    if (index < 1 || index > doomgazer::kParameterCount)
        return -1;
    return ccMap[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
}

juce::String DoomgazerAudioProcessor::mappingLabel(int index) const
{
    const int cc = ccForParameter(index);
    return cc < 0 ? juce::String("--") : ("CC " + juce::String(cc));
}

void DoomgazerAudioProcessor::beginLearn(int index) noexcept
{
    if (index < 1 || index > doomgazer::kParameterCount)
        return;
    stompArmed.store(-1, std::memory_order_release);
    learnIndex.store(index, std::memory_order_release);
}

void DoomgazerAudioProcessor::clearMapping(int index)
{
    if (index < 1 || index > doomgazer::kParameterCount)
        return;
    ccMap[static_cast<std::size_t>(index)].store(-1, std::memory_order_relaxed);
    lastCc[static_cast<std::size_t>(index)].store(0, std::memory_order_relaxed);
    apvts.state.removeProperty(ccProperty(index), nullptr);
}

void DoomgazerAudioProcessor::clearAllMappings()
{
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
        clearMapping(index);
}

void DoomgazerAudioProcessor::setMidiChannel(int newChannel)
{
    channel.store(juce::jlimit(0, 16, newChannel), std::memory_order_relaxed);
    storeMidiState();
}

void DoomgazerAudioProcessor::setProgramChangeEnabled(bool enabled)
{
    programChangeOn.store(enabled, std::memory_order_relaxed);
    storeMidiState();
}

void DoomgazerAudioProcessor::armControllerEdge(int cc) noexcept
{
    if (cc < 0 || cc > 127)
        return;
    const unsigned bit = static_cast<unsigned>(cc < 64 ? cc : cc - 64);
    auto& mask = cc < 64 ? resetEdgeLo : resetEdgeHi;
    mask.fetch_or(1ull << bit, std::memory_order_release);
}

void DoomgazerAudioProcessor::bindMapping(int index, int cc)
{
    if (index < 1 || index > doomgazer::kParameterCount || cc < 0 || cc >= 128)
        return;

    for (int other = 1; other <= doomgazer::kParameterCount; ++other)
    {
        if (other == index)
            continue;
        if (ccMap[static_cast<std::size_t>(other)].load(std::memory_order_relaxed) == cc)
        {
            ccMap[static_cast<std::size_t>(other)].store(-1, std::memory_order_relaxed);
            lastCc[static_cast<std::size_t>(other)].store(0, std::memory_order_relaxed);
        }
    }

    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
    {
        if (stompKind[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed) == 1
            && stompNumber[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed) == cc)
        {
            stompKind[static_cast<std::size_t>(stomp)].store(0, std::memory_order_relaxed);
            stompNumber[static_cast<std::size_t>(stomp)].store(-1, std::memory_order_relaxed);
        }
    }

    ccMap[static_cast<std::size_t>(index)].store(cc, std::memory_order_relaxed);
    lastCc[static_cast<std::size_t>(index)].store(0, std::memory_order_relaxed);
    armControllerEdge(cc);
    storeMidiState();

    movedParameter.store(index, std::memory_order_relaxed);
    moveTick.fetch_add(1, std::memory_order_relaxed);
    activity.fetch_add(1, std::memory_order_relaxed);
}

void DoomgazerAudioProcessor::storeMidiState()
{
    apvts.state.setProperty("midi.initialised", true, nullptr);
    apvts.state.setProperty("midi.channel", channel.load(std::memory_order_relaxed), nullptr);
    apvts.state.setProperty("midi.programChange", programChangeOn.load(std::memory_order_relaxed),
                            nullptr);

    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const int cc = ccMap[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
        if (cc >= 0)
            apvts.state.setProperty(ccProperty(index), cc, nullptr);
        else
            apvts.state.removeProperty(ccProperty(index), nullptr);
    }

    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
    {
        const int kind = stompKind[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed);
        const int number = stompNumber[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed);
        if (kind > 0 && number >= 0)
            apvts.state.setProperty(stompProperty(stomp),
                                    (kind == 2 ? "n" : "c") + juce::String(number), nullptr);
        else
            apvts.state.removeProperty(stompProperty(stomp), nullptr);
    }
}

void DoomgazerAudioProcessor::loadMidiState()
{
    channel.store(juce::jlimit(0, 16, static_cast<int>(apvts.state.getProperty("midi.channel", 0))),
                  std::memory_order_relaxed);
    programChangeOn.store(static_cast<bool>(apvts.state.getProperty("midi.programChange", true)),
                          std::memory_order_relaxed);

    bool anyBinding = false;
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        const int cc = static_cast<int>(apvts.state.getProperty(ccProperty(index), -1));
        const int stored = (cc >= 0 && cc < 128) ? cc : -1;
        ccMap[static_cast<std::size_t>(index)].store(stored, std::memory_order_relaxed);
        lastCc[static_cast<std::size_t>(index)].store(0, std::memory_order_relaxed);
        if (stored >= 0)
            anyBinding = true;
    }

    for (int stomp = 0; stomp < doomgazer::kStompCount; ++stomp)
    {
        const juce::String text = apvts.state.getProperty(stompProperty(stomp)).toString();
        int kind = 0;
        int number = -1;
        if (text.startsWithChar('c'))
            kind = 1;
        else if (text.startsWithChar('n'))
            kind = 2;
        if (kind > 0)
        {
            number = text.substring(1).getIntValue();
            if (number < 0 || number > 127)
            {
                kind = 0;
                number = -1;
            }
        }
        stompKind[static_cast<std::size_t>(stomp)].store(kind, std::memory_order_relaxed);
        stompNumber[static_cast<std::size_t>(stomp)].store(number, std::memory_order_relaxed);
        if (kind > 0)
            anyBinding = true;
    }

    // Projects saved before the LPD8 map have no version. Install it once when
    // nothing is bound. Clear-all leaves the version in place, so a cleared
    // map stays cleared.
    const int mapVersion = static_cast<int>(apvts.state.getProperty("midi.mapVersion", 0));
    if (mapVersion < 1 && !anyBinding)
        installLpd8Map();

    apvts.state.setProperty("midi.mapVersion", 1, nullptr);
    apvts.state.setProperty("midi.initialised", true, nullptr);
    if (mapVersion < 1 && !anyBinding)
        storeMidiState();
}

void DoomgazerAudioProcessor::installLpd8Map() noexcept
{
    // Knob bank is CC 12-43, four programs of eight, with no hole at 32.
    // Pad bank is CC 70-101, four programs of eight; 73 and 99-101 stay spare.
    // Preset select is CC 103 and the volume/wah pedal switch is CC 104.
    // Pads toggle, except freeze, which follows the pad down and up.
    struct Knob
    {
        int parameter;
        int cc;
    };
    static constexpr Knob knobs[] = {
        { 2, 12 }, { 35, 13 }, { 3, 14 }, { 4, 15 }, { 7, 16 }, { 11, 17 }, { 6, 18 }, { 15, 19 },
        { 21, 20 }, { 22, 21 }, { 23, 22 }, { 33, 23 }, { 70, 24 }, { 71, 25 }, { 94, 26 }, { 18, 27 },
        { 38, 28 }, { 39, 29 }, { 40, 30 }, { 42, 31 }, { 44, 32 }, { 45, 33 }, { 46, 34 }, { 48, 35 },
        { 51, 36 }, { 52, 37 }, { 73, 38 }, { 76, 39 }, { 16, 40 }, { 78, 41 }, { 60, 42 }, { 24, 43 },
        { 66, 92 }, { 79, 93 }, { 67, 94 }, { 41, 95 }, { 47, 96 }, { 49, 97 }, { 55, 98 },
        { 95, 104 },
    };

    for (const Knob& knob : knobs)
    {
        ccMap[static_cast<std::size_t>(knob.parameter)].store(knob.cc, std::memory_order_relaxed);
        lastCc[static_cast<std::size_t>(knob.parameter)].store(0, std::memory_order_relaxed);
    }

    struct Pad
    {
        doomgazer::Stomp stomp;
        int cc;
    };
    static constexpr Pad pads[] = {
        { doomgazer::Stomp::doubler, 70 }, { doomgazer::Stomp::carve, 71 },
        { doomgazer::Stomp::subOctave, 72 },
        { doomgazer::Stomp::invert, 74 }, { doomgazer::Stomp::contour, 75 },
        { doomgazer::Stomp::sustain, 76 }, { doomgazer::Stomp::gate, 77 },
        { doomgazer::Stomp::grit, 78 },
        { doomgazer::Stomp::delay, 79 }, { doomgazer::Stomp::reverb, 80 },
        { doomgazer::Stomp::tremolo, 81 }, { doomgazer::Stomp::glide, 82 },
        { doomgazer::Stomp::swell, 83 }, { doomgazer::Stomp::phaser, 84 },
        { doomgazer::Stomp::chorus, 85 }, { doomgazer::Stomp::howl, 86 },
        { doomgazer::Stomp::pedal, 87 }, { doomgazer::Stomp::limit, 88 },
        { doomgazer::Stomp::presetPrev, 89 }, { doomgazer::Stomp::presetNext, 90 },
        { doomgazer::Stomp::core, 91 },
    };

    for (const Pad& pad : pads)
    {
        const int stomp = static_cast<int>(pad.stomp);
        stompKind[static_cast<std::size_t>(stomp)].store(1, std::memory_order_relaxed);
        stompNumber[static_cast<std::size_t>(stomp)].store(pad.cc, std::memory_order_relaxed);
    }
}

void DoomgazerAudioProcessor::storeRigState()
{
    apvts.state.setProperty("rig.subPitch", subPitchMode.load(std::memory_order_relaxed), nullptr);
    apvts.state.setProperty("rig.outputMono", outputMonoMode.load(std::memory_order_relaxed), nullptr);
    apvts.state.setProperty("rig.monoPaInvertR", monoPaInvertRMode.load(std::memory_order_relaxed),
                            nullptr);
}

void DoomgazerAudioProcessor::loadRigState()
{
    subPitchMode.store(juce::jlimit(0, 2, static_cast<int>(apvts.state.getProperty("rig.subPitch", 0))),
                       std::memory_order_relaxed);
    outputMonoMode.store(static_cast<int>(apvts.state.getProperty("rig.outputMono", 0)),
                         std::memory_order_relaxed);
    monoPaInvertRMode.store(static_cast<int>(apvts.state.getProperty("rig.monoPaInvertR", 0)),
                            std::memory_order_relaxed);
}

// Stomps ----------------------------------------------------------------------

bool DoomgazerAudioProcessor::stompIsOn(int stomp) const noexcept
{
    const auto above = [this](const char* id, double threshold) {
        if (const auto* value = apvts.getRawParameterValue(id))
            return value->load() > static_cast<float>(threshold);
        return false;
    };

    switch (static_cast<doomgazer::Stomp>(stomp))
    {
        case doomgazer::Stomp::core: return above("engine", 0.5);
        case doomgazer::Stomp::delay: return above("delayMode", 0.5);
        case doomgazer::Stomp::reverb: return above("reverbMode", 0.5);
        case doomgazer::Stomp::carve: return above("melodyCarve", 0.5);
        case doomgazer::Stomp::subOctave: return above("subOctave", 0.05);
        case doomgazer::Stomp::doubler: return above("doubler", 0.5);
        case doomgazer::Stomp::sustain: return above("sustainOn", 0.5);
        case doomgazer::Stomp::contour: return above("shroud", 0.5);
        case doomgazer::Stomp::tremolo: return above("tremolo", 0.5);
        case doomgazer::Stomp::glide: return above("glideMode", 0.5);
        case doomgazer::Stomp::swell: return above("swellAmount", 0.001);
        case doomgazer::Stomp::grit: return above("gritAmount", 0.001);
        case doomgazer::Stomp::phaser: return above("phaseMix", 0.001);
        case doomgazer::Stomp::chorus: return above("chorusMix", 0.001);
        case doomgazer::Stomp::howl: return above("howlAmount", 0.001);
        case doomgazer::Stomp::invert: return above("polarity", 0.5);
        case doomgazer::Stomp::limit: return above("limiter", 0.5);
        case doomgazer::Stomp::gate: return above("inputGate", 0.5);
        case doomgazer::Stomp::pedal: return above("pedal", 0.5);
        default: return false;
    }
}

void DoomgazerAudioProcessor::setPlainValue(const char* id, float plain) noexcept
{
    auto* parameter = apvts.getParameter(id);
    if (parameter == nullptr)
        return;
    parameter->setValue(parameter->convertTo0to1(plain));
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        if (std::strcmp(doomgazer::parameterId(index), id) == 0)
        {
            queueHostNotification(index);
            return;
        }
    }
}

float DoomgazerAudioProcessor::plainValue(const char* id) const noexcept
{
    if (auto* parameter = apvts.getRawParameterValue(id))
        return parameter->load();
    return 0.0f;
}

void DoomgazerAudioProcessor::rememberStompLevels() noexcept
{
    auto keepMode = [this](const char* id, std::atomic<int>& held) {
        const int mode = static_cast<int>(std::lround(plainValue(id)));
        if (mode > 0)
            held.store(mode, std::memory_order_relaxed);
    };
    auto keepAmount = [this](const char* id, std::atomic<float>& held) {
        const float value = plainValue(id);
        if (value > 0.001f)
            held.store(value, std::memory_order_relaxed);
    };
    keepMode("delayMode", heldStompDelay);
    keepMode("reverbMode", heldStompReverb);
    keepMode("glideMode", heldStompGlide);
    keepAmount("swellAmount", heldSwell);
    keepAmount("gritAmount", heldGrit);
    keepAmount("phaseMix", heldPhase);
    keepAmount("chorusMix", heldChorus);
    keepAmount("howlAmount", heldHowl);
}

void DoomgazerAudioProcessor::toggleStomp(int stomp) noexcept
{
    if (stomp < 0 || stomp >= doomgazer::kStompCount)
        return;

    auto flip = [this](const char* id) {
        setPlainValue(id, plainValue(id) > 0.5f ? 0.0f : 1.0f);
    };
    auto toggleMode = [this](const char* id, std::atomic<int>& held, int fallback) {
        const int mode = static_cast<int>(std::lround(plainValue(id)));
        if (mode > 0)
        {
            held.store(mode, std::memory_order_relaxed);
            setPlainValue(id, 0.0f);
            return;
        }
        int restore = held.load(std::memory_order_relaxed);
        if (restore < 1)
            restore = fallback;
        held.store(restore, std::memory_order_relaxed);
        setPlainValue(id, static_cast<float>(restore));
    };
    auto toggleAmount = [this](const char* id, std::atomic<float>& held) {
        const float value = plainValue(id);
        if (value > 0.001f)
        {
            held.store(value, std::memory_order_relaxed);
            setPlainValue(id, 0.0f);
            return;
        }
        float restore = held.load(std::memory_order_relaxed);
        if (restore <= 0.001f)
            restore = 0.5f;
        held.store(restore, std::memory_order_relaxed);
        setPlainValue(id, restore);
    };

    switch (static_cast<doomgazer::Stomp>(stomp))
    {
        case doomgazer::Stomp::core: flip("engine"); break;
        case doomgazer::Stomp::delay: toggleMode("delayMode", heldStompDelay, 1); break;
        case doomgazer::Stomp::reverb: toggleMode("reverbMode", heldStompReverb, 1); break;
        case doomgazer::Stomp::carve: flip("melodyCarve"); break;
        case doomgazer::Stomp::subOctave:
            setPlainValue("subOctave", plainValue("subOctave") > 0.05f ? 0.0f : 0.5f);
            break;
        case doomgazer::Stomp::doubler: flip("doubler"); break;
        case doomgazer::Stomp::sustain: flip("sustainOn"); break;
        case doomgazer::Stomp::contour: flip("shroud"); break;
        case doomgazer::Stomp::tremolo: flip("tremolo"); break;
        case doomgazer::Stomp::glide: toggleMode("glideMode", heldStompGlide, 1); break;
        case doomgazer::Stomp::swell: toggleAmount("swellAmount", heldSwell); break;
        case doomgazer::Stomp::grit: toggleAmount("gritAmount", heldGrit); break;
        case doomgazer::Stomp::phaser: toggleAmount("phaseMix", heldPhase); break;
        case doomgazer::Stomp::chorus: toggleAmount("chorusMix", heldChorus); break;
        case doomgazer::Stomp::howl: toggleAmount("howlAmount", heldHowl); break;
        case doomgazer::Stomp::invert: flip("polarity"); break;
        case doomgazer::Stomp::limit: flip("limiter"); break;
        case doomgazer::Stomp::gate: flip("inputGate"); break;
        case doomgazer::Stomp::pedal: flip("pedal"); break;
        case doomgazer::Stomp::presetPrev:
        case doomgazer::Stomp::presetNext:
            pendingPresetStep.store(
                static_cast<doomgazer::Stomp>(stomp) == doomgazer::Stomp::presetNext ? 1 : -1,
                std::memory_order_relaxed);
            triggerAsyncUpdate();
            break;
        default:
            jassertfalse;
            break;
    }
}

void DoomgazerAudioProcessor::beginStompLearn(int stomp) noexcept
{
    if (stomp < 0 || stomp >= doomgazer::kStompCount)
        return;
    learnIndex.store(-1, std::memory_order_release);
    stompArmed.store(stomp, std::memory_order_release);
}

bool DoomgazerAudioProcessor::stompIsBound(int stomp) const noexcept
{
    if (stomp < 0 || stomp >= doomgazer::kStompCount)
        return false;
    return stompKind[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed) > 0;
}

int DoomgazerAudioProcessor::stompHeldMode(int stomp) const noexcept
{
    switch (static_cast<doomgazer::Stomp>(stomp))
    {
        case doomgazer::Stomp::delay: return heldStompDelay.load(std::memory_order_relaxed);
        case doomgazer::Stomp::reverb: return heldStompReverb.load(std::memory_order_relaxed);
        case doomgazer::Stomp::glide: return heldStompGlide.load(std::memory_order_relaxed);
        default: return 0;
    }
}

juce::String DoomgazerAudioProcessor::stompBindingLabel(int stomp) const
{
    if (!stompIsBound(stomp))
        return {};
    const int kind = stompKind[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed);
    const int number = stompNumber[static_cast<std::size_t>(stomp)].load(std::memory_order_relaxed);
    return (kind == 2 ? "NOTE " : "CC ") + juce::String(number);
}

void DoomgazerAudioProcessor::clearStompBinding(int stomp)
{
    if (stomp < 0 || stomp >= doomgazer::kStompCount)
        return;
    stompKind[static_cast<std::size_t>(stomp)].store(0, std::memory_order_relaxed);
    stompNumber[static_cast<std::size_t>(stomp)].store(-1, std::memory_order_relaxed);
    storeMidiState();
}

void DoomgazerAudioProcessor::applyStompBinding(int stomp, int kind, int number)
{
    if (stomp < 0 || stomp >= doomgazer::kStompCount || kind <= 0 || number < 0 || number > 127)
        return;

    for (int other = 0; other < doomgazer::kStompCount; ++other)
    {
        if (other == stomp)
            continue;
        if (stompKind[static_cast<std::size_t>(other)].load(std::memory_order_relaxed) == kind
            && stompNumber[static_cast<std::size_t>(other)].load(std::memory_order_relaxed) == number)
        {
            stompKind[static_cast<std::size_t>(other)].store(0, std::memory_order_relaxed);
            stompNumber[static_cast<std::size_t>(other)].store(-1, std::memory_order_relaxed);
        }
    }

    if (kind == 1)
    {
        for (int index = 1; index <= doomgazer::kParameterCount; ++index)
        {
            if (ccMap[static_cast<std::size_t>(index)].load(std::memory_order_relaxed) == number)
            {
                ccMap[static_cast<std::size_t>(index)].store(-1, std::memory_order_relaxed);
                lastCc[static_cast<std::size_t>(index)].store(0, std::memory_order_relaxed);
            }
        }
    }

    stompKind[static_cast<std::size_t>(stomp)].store(kind, std::memory_order_relaxed);
    stompNumber[static_cast<std::size_t>(stomp)].store(number, std::memory_order_relaxed);
    if (kind == 1)
        armControllerEdge(number);
    storeMidiState();
    activity.fetch_add(1, std::memory_order_relaxed);
}

namespace {

int stompForBinding(const std::array<std::atomic<int>, doomgazer::kStompCount>& kinds,
                    const std::array<std::atomic<int>, doomgazer::kStompCount>& numbers,
                    int kind, int number) noexcept
{
    for (int index = 0; index < doomgazer::kStompCount; ++index)
    {
        if (kinds[static_cast<std::size_t>(index)].load(std::memory_order_relaxed) == kind
            && numbers[static_cast<std::size_t>(index)].load(std::memory_order_relaxed) == number)
            return index;
    }
    return -1;
}

} // namespace

void DoomgazerAudioProcessor::queueStomp(int stomp) noexcept
{
    if (stomp < 0 || stomp >= doomgazer::kStompCount)
        return;
    pendingStomps.fetch_or(1u << static_cast<unsigned>(stomp), std::memory_order_relaxed);
    triggerAsyncUpdate();
}

void DoomgazerAudioProcessor::handleMidi(const juce::MidiBuffer& midi)
{
    const int channelFilter = channel.load(std::memory_order_relaxed);
    const bool allowProgramChange = programChangeOn.load(std::memory_order_relaxed);
    const auto valueOf = [this](int index) {
        const auto* value = raw[static_cast<std::size_t>(index)];
        return value != nullptr ? value->load(std::memory_order_relaxed) : 0.0f;
    };
    const int presetCc = static_cast<int>(std::lround(static_cast<double>(valueOf(27))));
    const bool presetByCc = valueOf(25) >= 0.5f;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (channelFilter != 0 && message.getChannel() != channelFilter)
            continue;

        if (message.isProgramChange())
        {
            const int program = message.getProgramChangeNumber();
            const int armed = stompArmed.load(std::memory_order_acquire);
            if (armed >= 0)
            {
                pendingStompTarget.store(armed, std::memory_order_relaxed);
                pendingStompKind.store(2, std::memory_order_relaxed);
                pendingStompNumber.store(program, std::memory_order_relaxed);
                stompArmed.store(-1, std::memory_order_release);
                triggerAsyncUpdate();
                continue;
            }
            const int stomp = stompForBinding(stompKind, stompNumber, 2, program);
            if (stomp >= 0)
            {
                queueStomp(stomp);
                continue;
            }
            if (allowProgramChange)
            {
                activity.fetch_add(1, std::memory_order_relaxed);
                pendingProgram.store(program, std::memory_order_relaxed);
                triggerAsyncUpdate();
            }
            continue;
        }

        if (message.isNoteOn())
        {
            const int note = message.getNoteNumber();
            const int armed = stompArmed.load(std::memory_order_acquire);
            if (armed >= 0)
            {
                pendingStompTarget.store(armed, std::memory_order_relaxed);
                pendingStompKind.store(2, std::memory_order_relaxed);
                pendingStompNumber.store(note, std::memory_order_relaxed);
                stompArmed.store(-1, std::memory_order_release);
                triggerAsyncUpdate();
                continue;
            }
            const int stomp = stompForBinding(stompKind, stompNumber, 2, note);
            if (stomp >= 0)
            {
                if (stomp == static_cast<int>(doomgazer::Stomp::presetPrev)
                    || stomp == static_cast<int>(doomgazer::Stomp::presetNext))
                {
                    pendingPresetStep.store(stomp == static_cast<int>(doomgazer::Stomp::presetNext)
                                                ? 1
                                                : -1,
                                            std::memory_order_relaxed);
                    triggerAsyncUpdate();
                }
                else
                {
                    queueStomp(stomp);
                }
            }
            continue;
        }

        if (!message.isController())
            continue;

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            lastControllerValue.fill(-1);
            continue;
        }

        const int cc = message.getControllerNumber();
        const int value = message.getControllerValue();
        const int slot = cc & 127;
        const std::uint64_t edgeBit = 1ull << static_cast<unsigned>(slot < 64 ? slot : slot - 64);
        auto& edgeMask = slot < 64 ? resetEdgeLo : resetEdgeHi;
        if ((edgeMask.load(std::memory_order_acquire) & edgeBit) != 0)
        {
            edgeMask.fetch_and(~edgeBit, std::memory_order_acq_rel);
            lastControllerValue[static_cast<std::size_t>(slot)] = -1;
        }
        const int previous = lastControllerValue[static_cast<std::size_t>(slot)];
        lastControllerValue[static_cast<std::size_t>(slot)] = value;
        activity.fetch_add(1, std::memory_order_relaxed);

        const int learning = learnIndex.load(std::memory_order_acq_rel);
        if (learning >= 1)
        {
            pendingLearnIndex.store(learning, std::memory_order_relaxed);
            pendingLearnCc.store(cc, std::memory_order_relaxed);
            learnIndex.store(-1, std::memory_order_release);
            triggerAsyncUpdate();
            continue;
        }

        const int armed = stompArmed.load(std::memory_order_acquire);
        if (armed >= 0 && value >= 64)
        {
            pendingStompTarget.store(armed, std::memory_order_relaxed);
            pendingStompKind.store(1, std::memory_order_relaxed);
            pendingStompNumber.store(cc, std::memory_order_relaxed);
            stompArmed.store(-1, std::memory_order_release);
            triggerAsyncUpdate();
            continue;
        }

        const int stomp = stompForBinding(stompKind, stompNumber, 1, cc);
        if (stomp >= 0)
        {
            if (previous < 64 && value >= 64)
            {
                if (stomp == static_cast<int>(doomgazer::Stomp::presetPrev)
                    || stomp == static_cast<int>(doomgazer::Stomp::presetNext))
                {
                    pendingPresetStep.store(stomp == static_cast<int>(doomgazer::Stomp::presetNext)
                                                ? 1
                                                : -1,
                                            std::memory_order_relaxed);
                    triggerAsyncUpdate();
                }
                else
                {
                    queueStomp(stomp);
                }
            }
            continue;
        }

        if (presetByCc && cc == presetCc)
        {
            if (allowProgramChange)
            {
                pendingProgram.store(value, std::memory_order_relaxed);
                triggerAsyncUpdate();
            }
            continue;
        }

        for (int index = 1; index <= doomgazer::kParameterCount; ++index)
        {
            if (ccMap[static_cast<std::size_t>(index)].load(std::memory_order_relaxed) != cc)
                continue;

            int target = -1;
            if (index == doomgazer::kFreezeParameter)
            {
                // Freeze is momentary: hold the controller to freeze the tail,
                // release it to let the wash go. Everything else edge-toggles.
                lastCc[static_cast<std::size_t>(index)].store(value,
                                                              std::memory_order_relaxed);
                target = value >= 64 ? 1000 : 0;
            }
            else if (doomgazer::parameterIsSwitch(index))
            {
                lastCc[static_cast<std::size_t>(index)].store(value, std::memory_order_relaxed);
                if (previous >= 64 || value < 64)
                    break;
                const bool on = valueOf(index) >= 0.5f;
                target = on ? 0 : 1000;
            }
            else
            {
                target = value * 1000 / 127;
            }

            pendingCc[static_cast<std::size_t>(index)].store(target, std::memory_order_relaxed);
            movedParameter.store(index, std::memory_order_relaxed);
            moveTick.fetch_add(1, std::memory_order_relaxed);
            triggerAsyncUpdate();
            break;
        }
    }
}

void DoomgazerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Armed for the next host program change. This process() call's parameter
    // changes already ran, so a restore's stored slot does not load a preset.
    acceptHostProgram.store(true, std::memory_order_release);

    handleMidi(midi);

    double hostBpm = 0.0;
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
            if (const auto bpm = position->getBpm())
                hostBpm = *bpm;
    }
    dsp.setHostTempo(hostBpm);
    dsp.setPaMonoPolicy(outputMono(), monoPaInvertR());

    // Release the host-parameter hold here as well as from the editor timer, so a
    // program change with the editor closed still ends up with the engine following
    // the knobs again.
    if (holdHostParams.load(std::memory_order_acquire) && dsp.consumePresetApplied())
        holdHostParams.store(false, std::memory_order_release);

    if (!holdHostParams.load(std::memory_order_acquire))
    {
        const doomgazer::Parameters current = readParameters();
        if (!haveLastParameters || !sameParameters(current, lastParameters))
        {
            dsp.setParameters(current);
            lastParameters = current;
            haveLastParameters = true;
        }
    }

    if (buffer.getNumChannels() <= 0)
        return;

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
    dsp.process(left, right, buffer.getNumSamples(), nullptr);
}

juce::AudioProcessorEditor* DoomgazerAudioProcessor::createEditor()
{
    return new DoomgazerAudioProcessorEditor(*this);
}

int DoomgazerAudioProcessor::getNumPrograms()
{
    // 128 programs so a host can deliver PC 0-127. The index is the code, not
    // the alphabetical browser row. VST3 only creates the program parameter
    // when this is greater than 1, and that parameter is how most hosts send
    // a program change.
    return kHostProgramCount;
}

int DoomgazerAudioProcessor::getCurrentProgram()
{
    const juce::ScopedTryLock lock(presetLock);
    if (lock.isLocked())
        noteReportedProgram();
    return reportedProgram.load(std::memory_order_relaxed);
}

void DoomgazerAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= kHostProgramCount)
        return;
    // A project restore is followed by the program the host stored. Until a
    // block has run, that call is not a preset load. MIDI still selects.
    if (!acceptHostProgram.load(std::memory_order_acquire))
        return;

    pendingProgram.store(index, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

const juce::String DoomgazerAudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= kHostProgramCount)
        return {};

    const juce::ScopedLock lock(presetLock);
    const int owner = library.indexForProgram(index);
    if (owner < 0)
        return "---";
    return library.entry(owner).name;
}

void DoomgazerAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void DoomgazerAudioProcessor::noteReportedProgram()
{
    int code = 0;
    if (currentPreset >= 0 && currentPreset < library.size())
    {
        const int stored = library.programCode(currentPreset);
        if (stored >= 0 && stored < kHostProgramCount)
            code = stored;
    }
    reportedProgram.store(code, std::memory_order_relaxed);
}

void DoomgazerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    storeMidiState();
    storeCompareState();
    storePresetIdentity();
    storeRigState();

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void DoomgazerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
        return;

    const juce::ValueTree state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || !state.hasType(apvts.state.getType()))
        return;

    acceptHostProgram.store(false, std::memory_order_release);
    pendingProgram.store(-1, std::memory_order_relaxed);
    pendingPresetStep.store(0, std::memory_order_relaxed);
    apvts.replaceState(state);
    loadMidiState();
    loadRigState();
    restorePresetIdentity();
    restoreCompareState();

    const doomgazer::Parameters restored = readParameters();
    {
        const juce::ScopedLock lock(presetLock);
        baseline = restored;
    }
    applySound(restored, false);
}

bool DoomgazerAudioProcessor::parameterIsProjectSetting(int index) noexcept
{
    // Controller mode, the preset number mirror, and the preset CC are project
    // state rather than tone: loading a preset must not rebind the controller.
    return index == doomgazer::kControllerModeParameter
        || index == doomgazer::kPresetSlotParameter
        || index == doomgazer::kPresetCcParameter;
}

doomgazer::Parameters DoomgazerAudioProcessor::overlayProjectSettings(
    const doomgazer::Parameters& musical, const doomgazer::Parameters& live)
{
    auto sound = musical;
    for (int index = 1; index <= doomgazer::kParameterCount; ++index)
    {
        if (parameterIsProjectSetting(index))
            sound.at(index) = live.at(index);
    }
    return sound;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoomgazerAudioProcessor();
}