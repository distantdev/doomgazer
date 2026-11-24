#pragma once

#include "doomgazer/Parameters.h"
#include "doomgazer/PresetIO.h"

#include <juce_core/juce_core.h>

#include <vector>

// Factory presets are compiled in. User presets are text files the plugin owns,
// scanned from the per-user preset folder. Nothing here touches the audio thread:
// the processor treats the list as message-thread state and publishes a sound
// through the DSP fade mailbox.
class PresetLibrary
{
public:
    struct Entry
    {
        juce::String name;
        juce::String category;
        juce::String notes;
        doomgazer::Parameters params;
        bool factory = false;
        // Factory slot this sound was compiled as. -1 for a user preset.
        // Program-map lines use this, not the position in the alphabetical list.
        int factoryIndex = -1;
        juce::File file;
        juce::Time modified;
        // -1 is a blank PC: the preset stays in the list, but MIDI program
        // change and preset-CC selection skip it.
        int program = -1;
    };

    explicit PresetLibrary(juce::File folderToUse = {});

    void reload();

    int size() const noexcept { return static_cast<int>(entries.size()); }
    const Entry& entry(int index) const;

    // -1 when nothing matches.
    int indexOfName(const juce::String& name, bool factory) const;
    int indexOfFile(const juce::File& file) const;
    int factoryCount() const noexcept { return doomgazer::kFactoryPresetCount; }

    // -1 when the preset is blank or the index is out of range.
    int programCode(int index) const;
    // Library index for a MIDI program number, or -1 when nothing is assigned.
    int indexForProgram(int code) const;
    // code is 0-127, or -1 to clear. A code can belong to only one preset.
    bool setProgramCode(int index, int code, juce::String& error);

    juce::StringArray categories() const;

    static juce::File userFolder();
    static juce::String safeFileName(const juce::String& name);
    static juce::String storedName(const juce::String& name);
    static juce::String storedCategory(const juce::String& category);
    static juce::String storedNotes(const juce::String& notes);
    juce::File fileForName(const juce::String& name) const;

    // Writes one preset document. Creates the parent folder first; reports every
    // failure instead of failing silently.
    static bool writeFile(const juce::File& file,
                          const juce::String& name,
                          const juce::String& category,
                          const juce::String& notes,
                          const doomgazer::Parameters& params,
                          int program,
                          juce::String& error);
    static bool readFile(const juce::File& file, Entry& entry, juce::String& error);

    // Saves into the user folder. Refuses to overwrite unless allowOverwrite.
    // storedName receives the name actually written to the document, which can
    // differ from the typed name after sanitising or truncation.
    bool saveUser(const juce::String& name,
                  const juce::String& category,
                  const juce::String& notes,
                  const doomgazer::Parameters& params,
                  bool allowOverwrite,
                  juce::String& storedName,
                  juce::String& error);

    // Overwrites the file behind a user entry, keeping its identity.
    bool overwrite(int index, const juce::String& notes,
                   const doomgazer::Parameters& params, juce::String& error);

    bool renameUser(int index, const juce::String& newName, juce::String& storedName,
                    juce::String& error);
    bool removeUser(int index, juce::String& error);

private:
    static int defaultFactoryProgram(int factoryIndex) noexcept;
    int indexOfFactory(int factoryIndex) const;
    void applyFactoryProgramMap();
    bool saveFactoryProgramMap(juce::String& error);

    juce::File folder;
    std::vector<Entry> entries;
};
