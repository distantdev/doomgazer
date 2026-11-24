#pragma once

#include "PluginProcessor.h"
#include "ReleaseCheck.h"
#include "UiKit.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

// An action button in the always-visible row: it shows whether its target is
// on and whether a MIDI binding is attached, and wears its section's colour.
class StompButton : public juce::Button
{
public:
    explicit StompButton(const juce::String& label);

    std::function<void()> onContextMenu;

    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown(const juce::MouseEvent& event) override;

    void setBound(bool isBound);
    void setLearnArmed(bool armed);
    void setAccent(juce::Colour newAccent);

private:
    bool bound = false;
    bool armed = false;
    juce::Colour accent = dgui::amber();
};

// A text button that can also open a context menu: used for the preset step
// buttons, which are MIDI-learnable without being host parameters.
class CmdButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    std::function<void()> onContextMenu;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown() && onContextMenu != nullptr)
        {
            onContextMenu();
            return;
        }
        juce::TextButton::mouseDown(event);
    }
};

// A mode picker that also opens the same MIDI menu as the knobs and switches.
class ModeBox : public juce::ComboBox
{
public:
    std::function<void()> onContextMenu;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown() && onContextMenu != nullptr)
        {
            onContextMenu();
            return;
        }
        juce::ComboBox::mouseDown(event);
    }
};

class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel
{
public:
    explicit PresetBrowser(DoomgazerAudioProcessor& processor);
    ~PresetBrowser() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    void lookAndFeelChanged() override;

    std::function<void()> onLibraryChanged;
    std::function<void()> onClose;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height,
                          bool rowIsSelected) override;
    void selectedRowsChanged(int row) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    void showSaveAs();
    void showRename();
    void pollHoverHint();
    bool handleOverlayKey(const juce::KeyPress& key);

private:
    enum class EditorMode { none, saveAs, rename };

    void commitInline();
    void cancelInline();
    void commitProgramCode();
    void syncProgramEditor(bool force);
    void updateEditVisibility();
    void loadSelected();
    void deleteSelected();
    void setStatus(const juce::String& message, bool isError);
    int selectedEntry() const;
    void rebuildCategories();

    DoomgazerAudioProcessor& audioProcessor;
    juce::TextEditor search;
    juce::TextEditor pcEditor;
    juce::ComboBox categoryBox;
    juce::ListBox list;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton saveAsButton { "SAVE AS" };
    juce::TextButton renameButton { "RENAME" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton folderButton { "FOLDER" };
    juce::TextButton rescanButton { "RESCAN" };
    juce::TextButton closeButton { "CLOSE" };
    juce::TextEditor nameEditor;
    juce::TextEditor categoryEditor;
    juce::TextEditor notesEditor;
    juce::TextButton commitButton { "SAVE" };
    juce::TextButton cancelButton { "CANCEL" };
    juce::Label statusLabel;
    juce::String hoverText;
    juce::Rectangle<int> hintRowBounds;
    std::vector<int> visibleEntries;
    juce::StringArray knownCategories;
    EditorMode mode = EditorMode::none;
    int selectedRow = -1;
    int programEditIndex = -1;
    bool fillingProgramEditor = false;
    bool committingProgram = false;
    int preferredSelection = -1;
    bool deleteArmed = false;
    bool overwriteArmed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

// One fixed-size row per parameter, painted rather than composed: a hundred and
// twenty-five rows of four widgets is a lot of components for one page.
class MidiPage : public juce::Component
{
public:
    explicit MidiPage(DoomgazerAudioProcessor& processor);
    ~MidiPage() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    void lookAndFeelChanged() override;
    void pollHoverHint();

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    std::function<void()> onClose;

private:
    struct Row
    {
        int parameter = 0;      // 1..kParameterCount, or 0 for a stomp
        int stomp = -1;
        juce::Rectangle<int> area;
        juce::Rectangle<int> learn;
        juce::Rectangle<int> clear;
    };

    void layoutRows();
    int rowAt(juce::Point<int> position) const;
    juce::String rowName(const Row& row) const;
    juce::String rowMapping(const Row& row) const;

    DoomgazerAudioProcessor& audioProcessor;
    std::vector<Row> rows;
    juce::Label channelLabel;
    juce::ComboBox channelBox;
    juce::Label programLabel;
    juce::TextButton programChange { "ON" };
    juce::TextButton clearAll { "CLEAR ALL" };
    juce::TextButton closeButton { "CLOSE" };
    juce::Label presetCcLabel;
    juce::Slider presetCcBar;
    juce::TextButton selectByCc { "SELECT BY CC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> presetCcAttachment;
    juce::String hoverText;
    juce::Rectangle<int> hintRowBounds;
    juce::Rectangle<int> gridArea;
    int hoverRow = -1;
    int hoverPart = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiPage)
};

class DoomgazerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit DoomgazerAudioProcessorEditor(DoomgazerAudioProcessor&);
    ~DoomgazerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void buildBackdrop();
    void refreshPresetBar();
    void refreshStomps();
    void refreshModeBoxes();
    void refreshButtonColours();
    void openOverlay(juce::Component& overlayToShow);
    void closeOverlay();
    void layoutModule();
    void layoutChipRow();
    void refreshModuleChips();
    void showBand(int band, int module = -1);
    void showModule(int module);
    void applyTheme(dgui::Theme newTheme);
    void updateHint();
    void showAvailableUpdate(const ReleaseInfo& info);

    DoomgazerAudioProcessor& audioProcessor;
    dgui::DoomLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<dgui::ControlCell>> cells;
    std::vector<dgui::KnobCell*> knobCells;
    std::vector<dgui::SwitchCell*> switchCells;
    std::vector<dgui::SelectorCell*> selectorCells;
    std::vector<dgui::OrderCell*> orderCells;
    std::vector<std::unique_ptr<StompButton>> stripButtons;
    std::vector<std::unique_ptr<ModeBox>> modeBoxes;
    std::vector<int> modeBoxParameter;
    std::vector<int> modeBoxFirstValue;
    std::vector<juce::String> modeBoxLayout;
    std::vector<int> modeBoxModule;

    dgui::ScopePanel scope;
    dgui::SpectrumPanel spectrum;
    dgui::TunerPanel tuner;
    dgui::LevelMeter levelMeter;
    dgui::HintCard hint;
    dgui::Led midiLed;
    dgui::PresetBarButton presetBar;
    CmdButton prevButton { "<" };
    CmdButton nextButton { ">" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton revertButton { "REVERT" };
    dgui::ThemeButton themeButton;
    juce::TextButton compareButton { "A" };
    juce::TextButton midiButton { "MIDI" };
    std::vector<std::unique_ptr<juce::TextButton>> bandButtons;
    std::vector<std::unique_ptr<juce::TextButton>> moduleButtons;
    std::unique_ptr<dgui::SelectorCell> engineSelector;
    // Always on screen: the output trim and its two safety switches.
    std::unique_ptr<dgui::KnobCell> masterCell;
    std::unique_ptr<dgui::SwitchCell> limitCell;
    std::unique_ptr<dgui::SwitchCell> muteCell;
    juce::TextButton paOutputButton { "PA MONO" };
    juce::TextButton paInvertRButton { "INV OUT" };
    juce::HyperlinkButton developerLink;
    juce::HyperlinkButton updateLink;
    PresetBrowser browser;
    MidiPage midiPage;

    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> wordmarkArea;
    juce::Rectangle<int> bodyArea;
    juce::Rectangle<int> controlsArea;
    juce::Rectangle<int> tabsArea;
    juce::Rectangle<int> chipArea;
    juce::Rectangle<int> pageArea;
    juce::Rectangle<int> modulePanel;
    juce::Rectangle<int> outputArea;
    juce::Rectangle<int> rightArea;
    juce::Rectangle<int> meterArea;
    juce::Rectangle<int> tunerArea;
    juce::Rectangle<int> spectrumArea;
    juce::Rectangle<int> scopeArea;
    juce::Rectangle<int> levelsArea;
    juce::Rectangle<int> hintArea;
    juce::Rectangle<int> stripArea;

    juce::Image backdrop;
    juce::Component* overlay = nullptr;
    int activeBand = 0;
    int activeModule = 0;
    int hintLock = 0;
    juce::String shownPresetName;
    bool shownDirty = false;
    bool shownFactory = true;
    bool shownCompare = false;
    bool shownPaMono = false;
    bool shownPaInvert = false;
    int shownActivity = 0;
    int shownScopeWrite = -1;
    int shownSpectrumWrite = -1;
    uint32_t shownTunerSerial = 0;
    int backdropWidth = -1;
    int backdropHeight = -1;
    int backdropBand = -1;
    int backdropModule = -1;
    int backdropTheme = -1;
    juce::String availableRelease;
    // Last member, so it is destroyed before the update link. The destructor joins first.
    ReleaseChecker releaseChecker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoomgazerAudioProcessorEditor)
};
