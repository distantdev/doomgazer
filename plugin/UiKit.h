#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

namespace dgui {

// Two palettes share one set of roles. Nothing here is neutral grey: the light
// theme is cool paper, the dark one is deep navy, and both carry a teal accent
// plus one periwinkle.
struct Palette
{
    juce::Colour canvas;
    juce::Colour card;
    juce::Colour high;
    juce::Colour recess;
    juce::Colour hairline;
    juce::Colour ink;
    juce::Colour muted;
    juce::Colour faint;
    juce::Colour rust;
    juce::Colour moss;
    juce::Colour soft;
    juce::Colour slate;
    juce::Colour warn;
    juce::Colour gold;
    juce::Colour violet;
    juce::Colour sage;
};

enum class AccentRole { neutral, rust, moss, slate, warn, gold, violet, sage };

enum class Theme { light, dark };

const Palette& palette();
void setTheme(Theme newTheme);
Theme theme();
bool darkMode();
juce::Colour accentFor(AccentRole role);

inline juce::Colour ink() { return palette().canvas; }
inline juce::Colour panel() { return palette().card; }
inline juce::Colour panelHigh() { return palette().high; }
inline juce::Colour well() { return palette().recess; }
inline juce::Colour line() { return palette().hairline; }
inline juce::Colour text() { return palette().ink; }
inline juce::Colour dim() { return palette().muted; }
inline juce::Colour faint() { return palette().faint; }
inline juce::Colour amber() { return palette().rust; }
inline juce::Colour green() { return palette().moss; }
inline juce::Colour steel() { return palette().soft; }
inline juce::Colour blue() { return palette().slate; }
inline juce::Colour red() { return palette().warn; }
inline juce::Colour gold() { return palette().gold; }
inline juce::Colour violet() { return palette().violet; }
inline juce::Colour sage() { return palette().sage; }

const juce::String& faceName();
juce::Font font(float height, bool strong = false);
juce::Font trackedFont(float height, float extraTracking, bool strong = false);

// Flat section label: no panel fill, no border. Whitespace does the grouping.
void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
               juce::Colour border);
void drawCard(juce::Graphics& g, juce::Rectangle<int> area, float opacity = -1.0f);
// A card with its own colour wash, for surfaces that float above the editor.
// Fully opaque: nothing behind it may read as content.
void drawFloatingCard(juce::Graphics& g, juce::Rectangle<int> area);
void fillWell(juce::Graphics& g, juce::Rectangle<float> area, float corner = 5.0f);
void strokeWell(juce::Graphics& g, juce::Rectangle<float> area, float corner = 5.0f,
                float alpha = 0.6f);
void drawChip(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
              juce::Colour colour, bool filled);

juce::String formatValue(int index, double value);
double parseValue(int index, const juce::String& text);

class DoomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DoomLookAndFeel();

    // Re-reads the active palette into the colour IDs JUCE widgets use.
    void updateColours();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

class KnobSlider : public juce::Slider
{
public:
    std::function<void()> onContextMenu;

    void mouseDown(const juce::MouseEvent& event) override;
};

// A control in the grid. The hint hit area is the drawn control, not the whole
// cell: the space between knobs belongs to the section.
class ControlCell : public juce::Component
{
public:
    ~ControlCell() override = default;

    virtual bool hitsHint(juce::Point<int> positionInCell) const = 0;
    std::function<void(int)> onHintLock;
};

class KnobCell : public ControlCell
{
public:
    KnobCell(DoomgazerAudioProcessor& processor, int parameterIndex,
             const juce::String& caption, dgui::AccentRole role);

    void resized() override;
    void paint(juce::Graphics&) override;
    void lookAndFeelChanged() override;

    bool hitsHint(juce::Point<int> positionInCell) const override;

    void setLearning(bool shouldLearn);
    void tick();

    int parameterIndex() const noexcept { return index; }
    KnobSlider slider;

private:
    void showMenu();
    void applyPalette();

    DoomgazerAudioProcessor& audioProcessor;
    int index = 1;
    juce::String captionText;
    dgui::AccentRole accentRole = dgui::AccentRole::neutral;
    bool learning = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobCell)
};

// On/off parameters are switches, not knobs forced into a boolean. A track with
// a knob that slides is the one form everyone already knows.
class SwitchButton : public juce::Button
{
public:
    SwitchButton();

    std::function<void()> onContextMenu;

    void setAccent(juce::Colour newAccent);

    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    juce::Colour accent = dgui::amber();
};

class SwitchCell : public ControlCell
{
public:
    SwitchCell(DoomgazerAudioProcessor& processor, int parameterIndex,
               const juce::String& caption);

    void resized() override;
    void paint(juce::Graphics&) override;

    bool hitsHint(juce::Point<int> positionInCell) const override;

    void setLearning(bool shouldLearn);
    void tick();

    int parameterIndex() const noexcept { return index; }
    SwitchButton& button() noexcept { return toggle; }

    void setAccent(juce::Colour newAccent);
    // In the module row the caption sits inline at the left, on the same centre
    // line as the mode pickers and the selectors.
    void setInlineCaption(bool shouldInline);
    // Some module switches read better with the caption above the track, like
    // the footer switches, while keeping the track on the row's centre line.
    void setCaptionAbove(bool shouldBeAbove);

private:
    void showMenu();

    DoomgazerAudioProcessor& audioProcessor;
    int index = 1;
    juce::String captionText;
    SwitchButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    bool learning = false;
    bool inlineCaption = false;
    bool captionAbove = false;
    juce::Colour accent = dgui::amber();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SwitchCell)
};

// A choice between two named states where both are "on" (CORE / TWIN, MONO /
// STEREO, PRE / POST, VOL / WAH). Deliberately different from a switch: both
// segments are always visible, so nobody reads it as an on/off toggle.
class SelectorCell : public ControlCell
{
public:
    SelectorCell(DoomgazerAudioProcessor& processor, int parameterIndex,
                 const juce::String& caption, const juce::String& firstLabel,
                 const juce::String& secondLabel);

    void resized() override;
    void paint(juce::Graphics&) override;

    bool hitsHint(juce::Point<int> positionInCell) const override;

    void setAccent(juce::Colour newAccent);
    // Each state can wear its own section colour (CORE teal, TWIN periwinkle).
    void setAccents(juce::Colour firstColour, juce::Colour secondColour);
    void mouseDown(const juce::MouseEvent& event) override;

    void setLearning(bool shouldLearn);
    void tick();

    int parameterIndex() const noexcept { return index; }

private:
    void choose(bool second);
    void showMenu();

    juce::Rectangle<int> trackBounds() const;

    DoomgazerAudioProcessor& audioProcessor;
    int index = 1;
    juce::String captionText;
    juce::String labels[2];
    juce::Colour accent = dgui::amber();
    juce::Colour accents[2] { dgui::amber(), dgui::amber() };
    bool learning = false;
    int shownState = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SelectorCell)
};

// The atmosphere chain: both paths are drawn in order, each with a rule through
// it, and a switch picks which one runs. The switch's up position is the first
// path, so the diagram and the toggle always agree.
class OrderCell : public ControlCell
{
public:
    OrderCell(DoomgazerAudioProcessor& processor, int parameterIndex,
              const juce::String& firstStage, const juce::String& secondStage);

    void resized() override;
    void paint(juce::Graphics&) override;

    bool hitsHint(juce::Point<int> positionInCell) const override;

    void setAccent(juce::Colour newAccent);
    void setLearning(bool shouldLearn);
    void tick();

    int parameterIndex() const noexcept { return index; }

private:
    void showMenu();
    void drawPath(juce::Graphics&, juce::Rectangle<int>, const juce::String& first,
                  const juce::String& second, bool active) const;
    int toggleHeight() const;

    DoomgazerAudioProcessor& audioProcessor;
    int index = 1;
    juce::String stages[2];
    SwitchButton toggle;
    juce::Colour accent = dgui::steel();
    bool learning = false;
    int shownState = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrderCell)
};

class ScopePanel : public juce::Component
{
public:
    explicit ScopePanel(DoomgazerAudioProcessor& processor);

    void paint(juce::Graphics&) override;

private:
    void resample(const std::array<float, doomgazer::Processor::kScopeSize>& data, int write,
                  int span, std::vector<float>& target, int width) const;

    DoomgazerAudioProcessor& audioProcessor;
    std::vector<float> outputWave;
    std::vector<float> inputWave;
};

class SpectrumPanel : public juce::Component
{
public:
    explicit SpectrumPanel(DoomgazerAudioProcessor& processor);

    void paint(juce::Graphics&) override;

private:
    DoomgazerAudioProcessor& audioProcessor;
    std::vector<float> smoothed;
    std::vector<juce::Point<float>> points;
};

class TunerPanel : public juce::Component
{
public:
    explicit TunerPanel(DoomgazerAudioProcessor& processor);

    void paint(juce::Graphics&) override;

private:
    DoomgazerAudioProcessor& audioProcessor;
};

// Peak and RMS as two horizontal tracks, between the tuner and the scope.
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter(DoomgazerAudioProcessor& processor);

    void paint(juce::Graphics&) override;
    void tick();

private:
    DoomgazerAudioProcessor& audioProcessor;
    float peak = 0.0f;
    float rms = 0.0f;
    bool clipped = false;
    juce::uint32 clipUntil = 0;
};

// Post-trim peak for the INPUT module. The mark is blue, green, or red around
// the window the factory sounds were made in. The lamp is a pre-trim clip hold.
class InputLevelMeter : public juce::Component
{
public:
    explicit InputLevelMeter(DoomgazerAudioProcessor& processor);

    void paint(juce::Graphics&) override;
    void tick();

private:
    juce::Colour zoneColour(float peak) const;

    DoomgazerAudioProcessor& audioProcessor;
    float held = 0.0f;
    juce::uint32 holdUntil = 0;
    bool clipped = false;
    juce::uint32 clipUntil = 0;
};

class Led : public juce::Component
{
public:
    void paint(juce::Graphics&) override;
    void setLedColour(juce::Colour newColour);
    void flash();
    void tick();

private:
    juce::Colour colour = dgui::faint();
    float glow = 0.0f;
};

// Half-filled circle: the familiar light/dark glyph.
class ThemeButton : public juce::Button
{
public:
    ThemeButton();

    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
};

// Turns the area under the mouse into a short explanation. Idle text when the
// pointer is over nothing useful.
class HintCard : public juce::Component
{
public:
    void setHint(const juce::String& section, const juce::String& title,
                 const juce::String& body);
    // Compact draws the same text with the footer's smaller type and tighter
    // padding; used when the hint sits inside the controls card.
    void setCompact(bool shouldCompact);
    void paint(juce::Graphics&) override;

private:
    juce::String sectionText { "HINT" };
    juce::String titleText;
    juce::String bodyText { "Hover over a control for more details." };
    bool compact = false;
};

class PresetBarButton : public juce::TextButton
{
public:
    std::function<void(int)> onStep;

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
};

} // namespace dgui
