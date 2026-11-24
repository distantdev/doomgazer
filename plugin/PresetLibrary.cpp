#include "PresetLibrary.h"

#include <algorithm>
#include <utility>

namespace {

constexpr juce::int64 kMaxPresetBytes = 256 * 1024;

juce::String describeParse(const doomgazer::PresetParseReport& report)
{
    switch (report.result)
    {
        case doomgazer::PresetParseResult::ok: return {};
        case doomgazer::PresetParseResult::notPreset: return "Not a Doomgazer preset";
        case doomgazer::PresetParseResult::unsupportedVersion:
            return "Preset version is newer than this build";
        case doomgazer::PresetParseResult::empty: return "Preset file is empty";
    }
    return "Unreadable preset";
}

} // namespace

PresetLibrary::PresetLibrary(juce::File folderToUse)
    : folder(std::move(folderToUse))
{
    if (folder.getFullPathName().isEmpty())
        folder = userFolder();
    reload();
}

juce::File PresetLibrary::userFolder()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Doomgazer")
        .getChildFile("Presets");
}

juce::String PresetLibrary::safeFileName(const juce::String& name)
{
    juce::String token = name.trim();

    // Keep non-ASCII names, but turn illegal path characters into separators so
    // "AC/DC" and "ACDC" do not collapse onto the same file.
    for (const auto illegal : { '/', '\\', ':', '*', '?', '"', '<', '>', '|' })
        token = token.replaceCharacter(illegal, '-');

    token = juce::File::createLegalFileName(token);
    token = token.replaceCharacter(' ', '-');

    while (token.contains("--"))
        token = token.replace("--", "-");

    token = token.trimCharactersAtStart("-.");
    token = token.trimCharactersAtEnd("-. ");
    if (token.length() > 64)
        token = token.substring(0, 64).trimCharactersAtEnd("-. ");
    if (token.isEmpty())
        token = "preset";

    static const char* const reserved[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    };
    for (const auto* word : reserved)
        if (token.equalsIgnoreCase(word))
            token = "_" + token;

    return token;
}

juce::String PresetLibrary::storedName(const juce::String& name)
{
    return juce::String(doomgazer::sanitisePresetText(name.toStdString(), 64));
}

juce::String PresetLibrary::storedCategory(const juce::String& category)
{
    return juce::String(doomgazer::sanitisePresetText(category.toStdString(), 32));
}

juce::String PresetLibrary::storedNotes(const juce::String& notes)
{
    return juce::String(doomgazer::sanitisePresetText(notes.toStdString(), 480));
}

juce::File PresetLibrary::fileForName(const juce::String& name) const
{
    return folder.getChildFile(safeFileName(name) + doomgazer::kPresetExtension);
}

void PresetLibrary::reload()
{
    entries.clear();
    entries.reserve(static_cast<std::size_t>(doomgazer::kFactoryPresetCount) + 16);

    for (int index = 0; index < doomgazer::kFactoryPresetCount; ++index)
    {
        Entry entry;
        entry.name = juce::String(doomgazer::presetName(index));
        entry.category = juce::String(doomgazer::presetCategory(index));
        entry.params = doomgazer::factoryPreset(index);
        entry.factory = true;
        entry.factoryIndex = index;
        entry.program = defaultFactoryProgram(index);
        entries.push_back(std::move(entry));
    }

    if (folder.isDirectory())
    {
        juce::Array<juce::File> files;
        folder.findChildFiles(files, juce::File::findFiles, false,
                              juce::String("*") + doomgazer::kPresetExtension);

        for (const auto& file : files)
        {
            Entry entry;
            juce::String error;
            if (readFile(file, entry, error))
                entries.push_back(std::move(entry));
        }
    }

    applyFactoryProgramMap();

    // The menu is a name list. List position is not a PC code.
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        return left.name.compareIgnoreCase(right.name) < 0;
    });
}

int PresetLibrary::defaultFactoryProgram(int factoryIndex) noexcept
{
    return factoryIndex >= 0 && factoryIndex <= 127 ? factoryIndex : -1;
}

int PresetLibrary::indexOfFactory(int factoryIndex) const
{
    if (factoryIndex < 0)
        return -1;
    for (int index = 0; index < size(); ++index)
    {
        const Entry& entry = entries[static_cast<std::size_t>(index)];
        if (entry.factory && entry.factoryIndex == factoryIndex)
            return index;
    }
    return -1;
}

int PresetLibrary::programCode(int index) const
{
    if (index < 0 || index >= size())
        return -1;
    return entries[static_cast<std::size_t>(index)].program;
}

int PresetLibrary::indexForProgram(int code) const
{
    if (code < 0 || code > 127)
        return -1;
    for (int index = 0; index < size(); ++index)
        if (entries[static_cast<std::size_t>(index)].program == code)
            return index;
    return -1;
}

void PresetLibrary::applyFactoryProgramMap()
{
    const juce::File file = folder.getChildFile("program-map.txt");
    if (!file.existsAsFile())
        return;

    const juce::StringArray lines = juce::StringArray::fromLines(file.loadFileAsString());
    for (const auto& raw : lines)
    {
        const juce::String line = raw.trim();
        if (line.isEmpty() || line.startsWithChar('#'))
            continue;
        const int eq = line.indexOfChar('=');
        if (eq <= 0)
            continue;

        const int slot = line.substring(0, eq).trim().getIntValue();
        const int index = indexOfFactory(slot);
        if (index < 0)
            continue;

        const juce::String value = line.substring(eq + 1).trim();
        int code = -1;
        if (value != "-" && value.isNotEmpty())
        {
            if (!value.containsOnly("0123456789"))
                continue;
            code = value.getIntValue();
            if (code > 127)
                continue;
        }
        entries[static_cast<std::size_t>(index)].program = code;
    }
}

bool PresetLibrary::saveFactoryProgramMap(juce::String& error)
{
    juce::String text;
    text << "# Doomgazer factory program changes. index=pc, or index=- for none.\n";
    bool any = false;
    for (const Entry& entry : entries)
    {
        if (!entry.factory || entry.factoryIndex < 0)
            continue;
        const int code = entry.program;
        if (code == defaultFactoryProgram(entry.factoryIndex))
            continue;
        any = true;
        text << entry.factoryIndex << "=" << (code < 0 ? "-" : juce::String(code)) << "\n";
    }

    const juce::File file = folder.getChildFile("program-map.txt");
    if (!any)
    {
        if (file.existsAsFile() && !file.deleteFile())
        {
            error = "Cannot update " + file.getFullPathName();
            return false;
        }
        return true;
    }

    const juce::File parent = file.getParentDirectory();
    if (!parent.isDirectory())
    {
        const juce::Result created = parent.createDirectory();
        if (created.failed())
        {
            error = "Cannot create " + parent.getFullPathName() + ": " + created.getErrorMessage();
            return false;
        }
    }
    if (!file.replaceWithText(text, false, false, "\n"))
    {
        error = "Cannot write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool PresetLibrary::setProgramCode(int index, int code, juce::String& error)
{
    error.clear();
    if (index < 0 || index >= size())
    {
        error = "No preset selected";
        return false;
    }
    if (code < -1 || code > 127)
    {
        error = "PC is 0-127, or blank";
        return false;
    }
    if (entries[static_cast<std::size_t>(index)].program == code)
        return true;

    std::vector<int> changed;
    if (code >= 0)
    {
        for (int other = 0; other < size(); ++other)
        {
            if (other == index)
                continue;
            if (entries[static_cast<std::size_t>(other)].program == code)
            {
                entries[static_cast<std::size_t>(other)].program = -1;
                changed.push_back(other);
            }
        }
    }

    entries[static_cast<std::size_t>(index)].program = code;
    changed.push_back(index);

    for (const int item : changed)
    {
        const Entry& entry = entries[static_cast<std::size_t>(item)];
        if (entry.factory)
            continue;
        if (!writeFile(entry.file, entry.name, entry.category, entry.notes,
                       entry.params, entry.program, error))
            return false;
    }
    return saveFactoryProgramMap(error);
}

const PresetLibrary::Entry& PresetLibrary::entry(int index) const
{
    if (index < 0 || index >= size())
    {
        static const Entry empty;
        return empty;
    }
    return entries[static_cast<std::size_t>(index)];
}

int PresetLibrary::indexOfName(const juce::String& name, bool factory) const
{
    for (int index = 0; index < size(); ++index)
        if (entries[static_cast<std::size_t>(index)].factory == factory
            && entries[static_cast<std::size_t>(index)].name.equalsIgnoreCase(name))
            return index;
    return -1;
}

int PresetLibrary::indexOfFile(const juce::File& file) const
{
    for (int index = 0; index < size(); ++index)
    {
        const auto& entry = entries[static_cast<std::size_t>(index)];
        if (!entry.factory && entry.file == file)
            return index;
    }
    return -1;
}

juce::StringArray PresetLibrary::categories() const
{
    juce::StringArray result;
    for (const auto& entry : entries)
        if (!result.contains(entry.category, true))
            result.add(entry.category);
    result.sort(true);
    return result;
}

bool PresetLibrary::writeFile(const juce::File& file,
                              const juce::String& name,
                              const juce::String& category,
                              const juce::String& notes,
                              const doomgazer::Parameters& params,
                              int program,
                              juce::String& error)
{
    error.clear();
    const auto parent = file.getParentDirectory();
    if (!parent.isDirectory())
    {
        const juce::Result created = parent.createDirectory();
        if (created.failed())
        {
            error = "Cannot create " + parent.getFullPathName() + ": " + created.getErrorMessage();
            return false;
        }
    }

    doomgazer::PresetDocument document;
    document.name = storedName(name).toStdString();
    document.category = storedCategory(category).toStdString();
    document.notes = storedNotes(notes).toStdString();
    document.created = juce::Time::getCurrentTime().toISO8601(true).toStdString();
    document.params = params;
    document.program = program >= 0 && program <= 127 ? program : -1;

    const std::string text = doomgazer::writePresetText(document);
    if (!file.replaceWithText(juce::String(text), false, false, "\n"))
    {
        error = "Cannot write " + file.getFullPathName();
        return false;
    }
    return true;
}

bool PresetLibrary::readFile(const juce::File& file, Entry& entry, juce::String& error)
{
    error.clear();
    if (!file.existsAsFile())
    {
        error = "Missing file " + file.getFullPathName();
        return false;
    }
    if (file.getSize() > kMaxPresetBytes)
    {
        error = file.getFileName() + " is too large for a preset";
        return false;
    }

    doomgazer::PresetDocument document;
    const doomgazer::PresetParseReport report =
        doomgazer::readPresetText(file.loadFileAsString().toStdString(), document);
    if (!report.ok())
    {
        error = file.getFileName() + ": " + describeParse(report);
        return false;
    }

    entry.name = juce::String(document.name);
    entry.category = juce::String(document.category);
    entry.notes = juce::String(document.notes);
    entry.params = document.params;
    entry.program = document.program;
    entry.factory = false;
    entry.file = file;
    entry.modified = file.getLastModificationTime();
    return true;
}

bool PresetLibrary::saveUser(const juce::String& name,
                             const juce::String& category,
                             const juce::String& notes,
                             const doomgazer::Parameters& params,
                             bool allowOverwrite,
                             juce::String& storedNameOut,
                             juce::String& error)
{
    error.clear();
    storedNameOut.clear();
    const juce::String trimmed = name.trim();
    if (trimmed.isEmpty())
    {
        error = "Give the preset a name";
        return false;
    }

    const juce::File file = fileForName(trimmed);
    if (file.existsAsFile() && !allowOverwrite)
    {
        error = "A preset called " + trimmed + " already exists";
        return false;
    }
    int program = -1;
    if (file.existsAsFile())
    {
        Entry existing;
        juce::String ignored;
        if (readFile(file, existing, ignored))
            program = existing.program;
    }
    if (!writeFile(file, trimmed, category.trim().isEmpty() ? "User" : category, notes, params,
                   program, error))
        return false;

    reload();
    const int index = indexOfFile(file);
    storedNameOut = index >= 0 ? entries[static_cast<std::size_t>(index)].name
                               : storedName(trimmed);
    return true;
}

bool PresetLibrary::overwrite(int index, const juce::String& notes,
                              const doomgazer::Parameters& params, juce::String& error)
{
    error.clear();
    if (index < 0 || index >= size())
    {
        error = "No preset selected";
        return false;
    }
    const Entry& target = entries[static_cast<std::size_t>(index)];
    if (target.factory)
    {
        error = "Factory presets cannot be overwritten; save a copy instead";
        return false;
    }
    if (!writeFile(target.file, target.name, target.category, notes, params, target.program, error))
        return false;

    reload();
    return true;
}

bool PresetLibrary::renameUser(int index, const juce::String& newName,
                               juce::String& storedNameOut, juce::String& error)
{
    error.clear();
    storedNameOut.clear();
    if (index < 0 || index >= size())
    {
        error = "No preset selected";
        return false;
    }
    const Entry target = entries[static_cast<std::size_t>(index)];
    if (target.factory)
    {
        error = "Factory presets cannot be renamed";
        return false;
    }

    const juce::String trimmed = newName.trim();
    const juce::String cleaned = storedName(trimmed);
    if (trimmed.isEmpty() || cleaned.isEmpty())
    {
        error = "Give the preset a name";
        return false;
    }
    if (cleaned.equalsIgnoreCase(target.name))
    {
        storedNameOut = target.name;
        return true;
    }

    const juce::File destination = fileForName(trimmed);
    if (destination.exists() && destination != target.file)
    {
        error = "A preset called " + trimmed + " already exists";
        return false;
    }

    // Write the new document first, so a failed write leaves the old preset
    // untouched. Only then remove the old file.
    if (destination == target.file)
    {
        if (!writeFile(target.file, cleaned, target.category, target.notes, target.params,
                       target.program, error))
            return false;
    }
    else
    {
        if (!writeFile(destination, cleaned, target.category, target.notes, target.params,
                       target.program, error))
            return false;

        if (!target.file.deleteFile())
        {
            destination.deleteFile();
            error = "Cannot rename " + target.file.getFileName();
            return false;
        }
    }

    reload();
    const int renamed = indexOfFile(destination == target.file ? target.file : destination);
    storedNameOut = renamed >= 0 ? entries[static_cast<std::size_t>(renamed)].name : cleaned;
    return true;
}

bool PresetLibrary::removeUser(int index, juce::String& error)
{
    error.clear();
    if (index < 0 || index >= size())
    {
        error = "No preset selected";
        return false;
    }
    const Entry& target = entries[static_cast<std::size_t>(index)];
    if (target.factory)
    {
        error = "Factory presets cannot be deleted";
        return false;
    }
    if (!target.file.deleteFile())
    {
        error = "Cannot delete " + target.file.getFileName();
        return false;
    }

    reload();
    return true;
}
