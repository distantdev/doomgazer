#include "UiKit.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dgui {

// Sections are flat: a small corner and a hairline. Only the knobs, the sheet
// trim and the switches carry a shadow.
constexpr float sectionCorner = 4.0f;
constexpr float sheetCorner = 5.0f;
// Every switch track is this wide, whether it sits in a module row or in the
// footer output group.
constexpr int switchTrackWidth = 26;
// Marks on the input bar. The factory sounds sit in this window.
constexpr float kInputWindowLowDb = -18.0f;
constexpr float kInputWindowHighDb = -8.0f;

namespace {

const Palette& lightPalette()
{
    // Cool paper with a teal accent: the same studio language in daylight.
    static const Palette value {
        juce::Colour::fromRGB(0xDD, 0xE5, 0xEC), // canvas
        juce::Colour::fromRGB(0xF7, 0xFA, 0xFC), // card
        juce::Colour::fromRGB(0xFF, 0xFF, 0xFF), // high
        juce::Colour::fromRGB(0xD9, 0xE2, 0xEA), // recess
        juce::Colour::fromRGB(0xBD, 0xCB, 0xD7), // hairline
        juce::Colour::fromRGB(0x16, 0x24, 0x30), // ink
        juce::Colour::fromRGB(0x50, 0x67, 0x79), // muted
        juce::Colour::fromRGB(0x8B, 0x9E, 0xAD), // faint
        juce::Colour::fromRGB(0x0E, 0x9E, 0x9C), // teal accent
        juce::Colour::fromRGB(0x3B, 0x6F, 0xD4), // periwinkle
        juce::Colour::fromRGB(0x22, 0x33, 0x3F), // soft
        juce::Colour::fromRGB(0x2C, 0x6C, 0x8F), // slate
        juce::Colour::fromRGB(0xC7, 0x45, 0x2F), // warn
        juce::Colour::fromRGB(0xC9, 0x8A, 0x2E), // gold
        juce::Colour::fromRGB(0x6C, 0x4F, 0xB8), // violet
        juce::Colour::fromRGB(0x4E, 0x8A, 0x5F), // sage
    };
    return value;
}

const Palette& darkPalette()
{
    // Deep navy with a cyan accent, after the studio-mastering look.
    static const Palette value {
        juce::Colour::fromRGB(0x0B, 0x16, 0x26), // canvas
        juce::Colour::fromRGB(0x13, 0x27, 0x3E), // card
        juce::Colour::fromRGB(0x1C, 0x35, 0x52), // high
        juce::Colour::fromRGB(0x07, 0x13, 0x21), // recess
        juce::Colour::fromRGB(0x27, 0x42, 0x5E), // hairline
        juce::Colour::fromRGB(0xDD, 0xEB, 0xF7), // ink
        juce::Colour::fromRGB(0x93, 0xAB, 0xC2), // muted
        juce::Colour::fromRGB(0x5F, 0x79, 0x93), // faint
        juce::Colour::fromRGB(0x4F, 0xD6, 0xC9), // rust -> cyan
        juce::Colour::fromRGB(0x6E, 0x8B, 0xFF), // moss -> periwinkle
        juce::Colour::fromRGB(0x9F, 0xC4, 0xE8), // soft
        juce::Colour::fromRGB(0x3E, 0x7A, 0xC0), // slate
        juce::Colour::fromRGB(0xFF, 0x6B, 0x6B), // warn
        juce::Colour::fromRGB(0xF2, 0xC1, 0x4E), // gold
        juce::Colour::fromRGB(0xA9, 0x8B, 0xFF), // violet
        juce::Colour::fromRGB(0x7B, 0xD8, 0x9F), // sage
    };
    return value;
}

Theme& themeFlag()
{
    static Theme value = Theme::dark;
    return value;
}

} // namespace

const Palette& palette()
{
    return themeFlag() == Theme::dark ? darkPalette() : lightPalette();
}

void setTheme(Theme newTheme)
{
    themeFlag() = newTheme;
}

Theme theme()
{
    return themeFlag();
}

bool darkMode()
{
    return themeFlag() == Theme::dark;
}

juce::Colour accentFor(AccentRole role)
{
    switch (role)
    {
        case AccentRole::rust: return amber();
        case AccentRole::moss: return green();
        case AccentRole::slate: return blue();
        case AccentRole::warn: return red();
        case AccentRole::gold: return gold();
        case AccentRole::violet: return violet();
        case AccentRole::sage: return sage();
        case AccentRole::neutral: break;
    }
    return text();
}

const juce::String& faceName()
{
    static const juce::String chosen = [] {
        const auto names = juce::Font::findAllTypefaceNames();
        if (names.contains("Bahnschrift"))
            return juce::String("Bahnschrift");
        if (names.contains("Segoe UI"))
            return juce::String("Segoe UI");
        return juce::Font::getDefaultSansSerifFontName();
    }();
    return chosen;
}

juce::Font font(float height, bool strong)
{
    return juce::Font(juce::FontOptions(faceName(), height,
                                        strong ? juce::Font::bold : juce::Font::plain));
}

juce::Font trackedFont(float height, float extraTracking, bool strong)
{
    // JUCE measures extra kerning as a fraction of the font height, so convert
    // from the pixel gap the call sites talk about.
    return font(height, strong).withExtraKerningFactor(
        extraTracking / juce::jmax(1.0f, height));
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
               juce::Colour)
{
    g.setColour(dim());
    g.setFont(trackedFont(9.0f, 1.8f, true));
    g.drawText(title.toUpperCase(), area.removeFromTop(13), juce::Justification::centredLeft);
}

void drawCard(juce::Graphics& g, juce::Rectangle<int> area, float opacity)
{
    const auto card = area.toFloat();

    // The light palette's card is close to its canvas, so it needs to be more
    // transparent for the wash behind to read at all. Dark keeps more body.
    const float fillAlpha = opacity >= 0.0f ? opacity : (darkMode() ? 0.52f : 0.44f);
    const juce::Colour base = panel().withAlpha(fillAlpha);
    juce::ColourGradient fill(base.brighter(0.02f), card.getX(), card.getY(),
                              base.darker(0.06f), card.getX(), card.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(card, sectionCorner);

    // One quiet edge instead of a cast shadow: the surface still has a boundary,
    // but the knobs stay the only raised things on the page.
    g.setColour(line().withAlpha(0.65f));
    g.drawRoundedRectangle(card.reduced(0.5f), sectionCorner, 1.0f);
}

void drawFloatingCard(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto card = area.toFloat();

    // A sheet over a scrim keeps a shadow, but a short offset one: enough to
    // lift it off the page, not enough to halo its corners.
    g.setColour(juce::Colours::black.withAlpha(darkMode() ? 0.35f : 0.14f));
    g.fillRoundedRectangle(card.reduced(1.0f).translated(0.0f, 3.0f), sheetCorner);

    juce::Path clip;
    clip.addRoundedRectangle(card, sheetCorner);
    g.saveState();
    g.reduceClipRegion(clip);

    g.setColour(panel());
    g.fillRect(card);

    // Same wash as the backdrop, painted into the sheet so it is opaque but not
    // flat, and so whatever is behind the sheet never shows through.
    const auto blob = [&g, card](float fx, float fy, float fraction, juce::Colour colour) {
        const float cx = card.getX() + card.getWidth() * fx;
        const float cy = card.getY() + card.getHeight() * fy;
        const float radius = juce::jmax(card.getWidth(), card.getHeight()) * fraction;
        const juce::ColourGradient gradient(colour, cx, cy, colour.withAlpha(0.0f),
                                            cx + radius, cy, true);
        g.setGradientFill(gradient);
        g.fillRect(card);
    };

    const float strength = darkMode() ? 0.9f : 1.0f;
    blob(0.08f, 0.00f, 0.85f, amber().withAlpha(0.26f * strength));
    blob(0.98f, 0.26f, 0.80f, green().withAlpha(0.22f * strength));
    blob(0.22f, 1.05f, 0.85f, blue().withAlpha(0.18f * strength));

    g.restoreState();
    g.setColour(line().withAlpha(0.85f));
    g.drawRoundedRectangle(card.reduced(0.5f), sheetCorner, 1.0f);
}

void fillWell(juce::Graphics& g, juce::Rectangle<float> area, float corner)
{
    // One flat recess, no ramp across the panel. The light card is translucent,
    // so the light well has to be nudged darker to sit behind it at all.
    g.setColour(darkMode() ? well() : well().darker(0.10f));
    g.fillRoundedRectangle(area, corner);

    g.setColour(juce::Colours::black.withAlpha(darkMode() ? 0.20f : 0.025f));
    g.drawHorizontalLine(juce::roundToInt(area.getY()) + 1, area.getX() + corner,
                         area.getRight() - corner);
}

void strokeWell(juce::Graphics& g, juce::Rectangle<float> area, float corner, float alpha)
{
    g.setColour(line().withAlpha(alpha));
    g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
}

void drawChip(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
              juce::Colour colour, bool filled)
{
    if (filled)
    {
        g.setColour(colour.withAlpha(0.14f));
        g.fillRoundedRectangle(area.toFloat(), 3.0f);
    }
    g.setColour(colour.withAlpha(filled ? 0.9f : 0.55f));
    g.drawRoundedRectangle(area.toFloat().reduced(0.5f), 3.0f, 1.0f);
    g.setColour(colour);
    g.setFont(trackedFont(9.0f, 1.0f, true));
    g.drawText(label, area, juce::Justification::centred);
}

juce::String formatValue(int index, double value)
{
    if (doomgazer::parameterIsSwitch(index))
        return value >= 0.5 ? "ON" : "OFF";

    const juce::String unit(doomgazer::parameterUnit(index));
    if (unit == "dB")
        return (value > 0.0 ? "+" : "") + juce::String(value, 1);
    if (unit == "st")
        return (value > 0.0 ? "+" : "") + juce::String(juce::roundToInt(value));
    if (unit == "cents")
        return juce::String(juce::roundToInt(value)) + "c";
    if (unit == "ms")
        return juce::String(juce::roundToInt(value)) + " ms";
    if (unit == "Hz")
    {
        if (value >= 1000.0)
            return juce::String(value / 1000.0, 2) + " kHz";
        return juce::String(juce::roundToInt(value)) + " Hz";
    }

    const doomgazer::ParameterRange range = doomgazer::parameterRange(index);
    if (range.step >= 1.0)
        return juce::String(juce::roundToInt(value));
    if (range.min == 0.0 && range.max == 1.0)
        return juce::String(juce::roundToInt(value * 100.0)) + "%";
    return juce::String(value, 2);
}

double parseValue(int index, const juce::String& text)
{
    const juce::String trimmed = text.trim();
    if (doomgazer::parameterIsSwitch(index))
        return trimmed.startsWithIgnoreCase("on") || trimmed == "1" ? 1.0 : 0.0;

    const double number = trimmed.getDoubleValue();
    const juce::String unit(doomgazer::parameterUnit(index));
    if (unit == "Hz" && trimmed.containsIgnoreCase("k"))
        return number * 1000.0;
    if (trimmed.containsChar('%'))
        return number / 100.0;
    return number;
}

} // namespace dgui

namespace dgui {

DoomLookAndFeel::DoomLookAndFeel()
{
    updateColours();
}

void DoomLookAndFeel::updateColours()
{
    setColour(juce::Slider::textBoxTextColourId, dgui::text());
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::thumbColourId, dgui::amber());
    setColour(juce::Slider::rotarySliderFillColourId, dgui::amber());
    setColour(juce::Label::textColourId, dgui::text());
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, dgui::panel());
    setColour(juce::TextButton::textColourOffId, dgui::text());
    setColour(juce::TextButton::textColourOnId, dgui::ink());
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::outlineColourId, dgui::line());
    setColour(juce::ComboBox::textColourId, dgui::text());
    setColour(juce::ComboBox::arrowColourId, dgui::dim());
    setColour(juce::PopupMenu::backgroundColourId, dgui::panel());
    setColour(juce::PopupMenu::textColourId, dgui::text());
    setColour(juce::PopupMenu::headerTextColourId, dgui::dim());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, dgui::text().withAlpha(0.08f));
    setColour(juce::PopupMenu::highlightedTextColourId, dgui::text());
    setColour(juce::ListBox::backgroundColourId, dgui::well());
    setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
}

void DoomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle,
                                       juce::Slider& slider)
{
    // Flat by design: one disc, one needle. No body, no ticks, no glow.
    const float radius = static_cast<float>(juce::jmin(width, height)) * 0.44f;
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const juce::Colour accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const bool active = slider.isMouseOverOrDragging();
    const bool isSwitch = slider.getProperties().contains("switch");

    const float bodyRadius = radius * 0.88f;
    const juce::Rectangle<float> disc(cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f,
                                      bodyRadius * 2.0f);

    // Long, soft cast shadow. The face itself stays flat: no gradient, no gloss.
    juce::Path discPath;
    discPath.addEllipse(disc);
    juce::DropShadow(juce::Colours::black.withAlpha(dgui::darkMode() ? 0.55f : 0.20f), 16,
                     { 0, 9 })
        .drawForPath(g, discPath);

    g.setColour(dgui::darkMode() ? dgui::panel().brighter(0.11f) : dgui::panel().darker(0.05f));
    g.fillEllipse(disc);

    // Subtle rounded edge: a hairline that catches the light at the top.
    g.setColour(dgui::line().withAlpha(0.85f));
    g.drawEllipse(disc.reduced(0.5f), 1.0f);
    juce::Path topEdge;
    topEdge.addCentredArc(cx, cy, bodyRadius - 0.7f, bodyRadius - 0.7f, 0.0f,
                          -juce::MathConstants<float>::halfPi - 1.15f,
                          -juce::MathConstants<float>::halfPi + 1.15f, true);
    g.setColour(juce::Colours::white.withAlpha(dgui::darkMode() ? 0.12f : 0.40f));
    g.strokePath(topEdge, juce::PathStrokeType(1.0f));

    if (isSwitch)
    {
        if (sliderPos >= 0.5f)
        {
            g.setColour(dgui::amber());
            g.fillEllipse(cx - radius * 0.38f, cy - radius * 0.38f, radius * 0.76f,
                          radius * 0.76f);
        }
        else
        {
            g.setColour(dgui::faint());
            g.drawEllipse(cx - radius * 0.30f, cy - radius * 0.30f, radius * 0.60f,
                          radius * 0.60f, 1.0f);
        }
        return;
    }

    // Value as a single needle from the middle outward, no ring.
    juce::Path pointer;
    pointer.startNewSubPath(0.0f, -radius * 0.30f);
    pointer.lineTo(0.0f, -radius * 0.78f);
    g.setColour(slider.getProperties()["accented"] ? accent : dgui::dim());
    g.strokePath(pointer, juce::PathStrokeType(active ? 2.4f : 2.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded),
                 juce::AffineTransform::rotation(angle).translated(cx, cy));
}

void DoomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour&, bool highlighted, bool down)
{
    // Links carry no body: no pill, no shadow, nothing to look at but their text.
    if (button.getProperties().contains("link"))
        return;

    const bool on = button.getToggleState();
    const auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    const float radius = area.getHeight() * 0.5f;

    juce::Path shape;
    shape.addRoundedRectangle(area, radius);

    // A pill that sits just off the surface: a short shadow, no halo.
    juce::DropShadow(juce::Colours::black.withAlpha(dgui::darkMode() ? 0.30f : 0.08f), 4,
                     { 0, 2 })
        .drawForPath(g, shape);

    if (on)
    {
        // A lit button wears its section colour. The text stays ink-on-accent so the
        // state reads at a glance in both themes.
        g.setColour(button.findColour(juce::TextButton::buttonOnColourId));
    }
    else if (down || highlighted)
        g.setColour(dgui::panelHigh());
    else
        g.setColour(dgui::panel().withAlpha(0.55f));
    g.fillPath(shape);

    if (!on)
    {
        juce::Path inner;
        inner.addRoundedRectangle(area.reduced(1.2f), radius - 1.2f);
        g.setColour(juce::Colours::white.withAlpha(dgui::darkMode() ? 0.05f : 0.22f));
        g.strokePath(inner, juce::PathStrokeType(1.0f));
    }

    g.setColour(on ? button.findColour(juce::TextButton::buttonOnColourId).darker(0.25f)
                   : dgui::line());
    g.drawRoundedRectangle(area, radius, 1.0f);
}

void DoomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                     bool, bool)
{
    if (button.getProperties().contains("link"))
    {
        const bool on = button.getToggleState();
        const bool hot = button.isMouseOverOrDragging();
        const bool icon = button.getProperties().contains("icon");
        const juce::Colour accent = button.findColour(juce::TextButton::buttonOnColourId);

        // Hovering brings the action's own colour up, which is the whole
        // affordance a link needs.
        if (!button.isEnabled())
            g.setColour(dgui::faint().withAlpha(0.35f));
        else if (on || hot)
            g.setColour(accent);
        else
            g.setColour(button.findColour(juce::TextButton::textColourOffId)
                             .withAlpha(icon ? 0.60f : 0.85f));

        g.setFont(icon ? dgui::font(15.0f, true) : dgui::trackedFont(9.0f, 1.2f, true));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred);
        return;
    }

    const bool on = button.getToggleState();
    const juce::Colour foreground = on
        ? dgui::ink()
        : (button.isEnabled() ? button.findColour(juce::TextButton::textColourOffId)
                              : dgui::faint());
    g.setColour(foreground);
    g.setFont(button.getProperties().contains("chip")
                  ? dgui::trackedFont(9.0f, 1.0f, true)
                  : dgui::font(11.5f, button.getProperties().contains("strong")));

    auto area = button.getLocalBounds().reduced(9, 0);
    if (button.getProperties().contains("menu"))
    {
        auto triangleArea = area.removeFromRight(12);
        const float centreX = static_cast<float>(triangleArea.getCentreX());
        const float centreY = static_cast<float>(triangleArea.getCentreY());
        juce::Path arrow;
        arrow.addTriangle(centreX - 3.5f, centreY - 1.8f, centreX + 3.5f, centreY - 1.8f,
                          centreX, centreY + 2.4f);
        g.setColour(on ? dgui::ink().withAlpha(0.7f) : dgui::faint());
        g.fillPath(arrow);
        area.removeFromRight(4);
        g.setColour(foreground);
        g.drawFittedText(button.getButtonText(), area, juce::Justification::centredLeft, 1);
        return;
    }

    g.drawFittedText(button.getButtonText(), area, juce::Justification::centred, 1);
}

void DoomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    const juce::Rectangle<float> area(0.0f, 0.0f, static_cast<float>(width),
                                      static_cast<float>(height));
    // A recessed field, not a bare outline: the field must read as something to
    // open even before the pointer is over it.
    const auto field = area.reduced(0.5f);
    g.setColour(dgui::well());
    g.fillRoundedRectangle(field, 4.0f);
    g.setColour(dgui::line());
    g.drawRoundedRectangle(field, 4.0f, 1.0f);

    juce::Path arrow;
    const float cx = static_cast<float>(width) - 13.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(cx - 3.5f, cy - 1.8f, cx + 3.5f, cy - 1.8f, cx, cy + 2.4f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void DoomLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(9, 0, box.getWidth() - 26, box.getHeight());
    label.setFont(dgui::font(12.0f));
}

juce::Font DoomLookAndFeel::getLabelFont(juce::Label&)
{
    return dgui::font(12.0f);
}

juce::Font DoomLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{
    return dgui::font(11.5f);
}

juce::Font DoomLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return dgui::font(12.0f);
}

juce::Font DoomLookAndFeel::getPopupMenuFont()
{
    return dgui::font(13.0f);
}

juce::Slider::SliderLayout DoomLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    auto bounds = slider.getLocalBounds();
    const int textHeight = juce::jlimit(15, 24, slider.getTextBoxHeight());
    layout.textBoxBounds = bounds.removeFromBottom(textHeight);
    const int side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    layout.sliderBounds = bounds.withSizeKeepingCentre(side, juce::jmax(8, side));
    return layout;
}

juce::Label* DoomLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    juce::Label* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setJustificationType(juce::Justification::centred);
    label->setInterceptsMouseClicks(false, false);
    label->setFont(dgui::font(13.0f, true));
    return label;
}

void KnobSlider::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (onContextMenu)
            onContextMenu();
        return;
    }
    juce::Slider::mouseDown(event);
}

KnobCell::KnobCell(DoomgazerAudioProcessor& processor, int parameterIndex,
                   const juce::String& caption, dgui::AccentRole role)
    : audioProcessor(processor),
      index(parameterIndex),
      captionText(caption),
      accentRole(role)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
    slider.setRepaintsOnMouseActivity(true);
    slider.setScrollWheelEnabled(true);
    slider.setPopupDisplayEnabled(false, false, nullptr);
    slider.onDragStart = [this] { if (onHintLock) onHintLock(index); };
    slider.onDragEnd = [this] { if (onHintLock) onHintLock(0); };

    const char* id = doomgazer::parameterId(index);
    if (auto* parameter = audioProcessor.apvts.getParameter(id))
    {
        const double fallback = parameter->convertFrom0to1(parameter->getDefaultValue());
        slider.setDoubleClickReturnValue(true, fallback);
    }

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, id, slider);

    const int textParameter = index;
    slider.textFromValueFunction = [textParameter](double value) {
        return dgui::formatValue(textParameter, value);
    };
    slider.valueFromTextFunction = [textParameter](const juce::String& valueText) {
        return dgui::parseValue(textParameter, valueText);
    };
    slider.updateText();

    slider.onContextMenu = [this] { showMenu(); };

    applyPalette();
}

void KnobCell::applyPalette()
{
    slider.setColour(juce::Slider::rotarySliderFillColourId, dgui::accentFor(accentRole));
    slider.getProperties().set("accented", accentRole != dgui::AccentRole::neutral);
    slider.setColour(juce::Slider::textBoxTextColourId, dgui::text());
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void KnobCell::lookAndFeelChanged()
{
    applyPalette();
    repaint();
}

bool KnobCell::hitsHint(juce::Point<int> position) const
{
    if (getLocalBounds().removeFromTop(13).contains(position))
        return true;

    auto bounds = slider.getBounds();
    const int textHeight = juce::jlimit(15, 24, slider.getTextBoxHeight());
    const auto textBox = bounds.removeFromBottom(textHeight);
    if (textBox.contains(position))
        return true;

    const int side = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto rotary = bounds.withSizeKeepingCentre(side, juce::jmax(8, side));
    const float radius = static_cast<float>(side) * 0.36f + 4.0f;
    return (position.toFloat() - rotary.getCentre().toFloat()).getDistanceFromOrigin() <= radius;
}

void KnobCell::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(20);
    slider.setBounds(area.reduced(1, 0));
}

void KnobCell::paint(juce::Graphics& g)
{
    g.setColour(learning ? dgui::amber() : dgui::dim());
    g.setFont(dgui::trackedFont(9.0f, 1.5f));
    g.drawFittedText(captionText, getLocalBounds().withTrimmedTop(4).removeFromTop(13),
                     juce::Justification::centred, 1);

    if (learning)
    {
        g.setColour(dgui::amber().withAlpha(0.85f));
        g.drawRoundedRectangle(slider.getBounds().toFloat().reduced(0.5f), 7.0f, 1.2f);
    }
}

void KnobCell::setLearning(bool shouldLearn)
{
    if (learning == shouldLearn)
        return;
    learning = shouldLearn;
    repaint();
}

void KnobCell::tick()
{
    setLearning(audioProcessor.learnTarget() == index);
}

void KnobCell::showMenu()
{
    const int cc = audioProcessor.ccForParameter(index);
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(doomgazer::parameterName(index)));
    menu.addItem(1, "MIDI Learn");
    menu.addItem(2, cc >= 0 ? "Clear MIDI (" + juce::String(cc) + ")" : "Clear MIDI", cc >= 0);
    menu.addSeparator();
    menu.addItem(3, "Reset to default");

    juce::Component::SafePointer<KnobCell> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&slider),
                       [safe](int result) {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result == 1)
                           {
                               safe->audioProcessor.beginLearn(safe->index);
                               safe->setLearning(true);
                           }
                           else if (result == 2)
                           {
                               safe->audioProcessor.clearMapping(safe->index);
                           }
                           else if (result == 3)
                           {
                               if (auto* parameter = safe->audioProcessor.apvts.getParameter(
                                       doomgazer::parameterId(safe->index)))
                                   parameter->setValueNotifyingHost(parameter->getDefaultValue());
                           }
                       });
}

SwitchButton::SwitchButton()
    : juce::Button("switch")
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(false);
}

void SwitchButton::setAccent(juce::Colour newAccent)
{
    if (accent == newAccent)
        return;
    accent = newAccent;
    repaint();
}

void SwitchButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat();
    const float width = area.getWidth();
    const bool on = getToggleState();

    // Recessed trough: darker at the top, with an inner shadow and a bottom lift.
    const bool darkTrough = dgui::darkMode();
    juce::ColourGradient trough(darkTrough ? dgui::well().darker(0.40f)
                                           : dgui::well().darker(0.10f),
                                area.getX(), area.getY(),
                                darkTrough ? dgui::well().brighter(0.10f)
                                           : dgui::well().brighter(0.06f),
                                area.getX(), area.getBottom(), false);
    g.setGradientFill(trough);
    g.fillRoundedRectangle(area, width * 0.5f);

    juce::Path troughShape;
    troughShape.addRoundedRectangle(area, width * 0.5f);
    g.saveState();
    g.reduceClipRegion(troughShape);

    juce::ColourGradient shade(juce::Colours::black.withAlpha(darkTrough ? 0.45f : 0.10f),
                               area.getX(), area.getY(),
                               juce::Colours::transparentBlack, area.getX(),
                               area.getY() + width * 0.55f, false);
    g.setGradientFill(shade);
    g.fillRect(area);

    juce::ColourGradient lift(juce::Colours::white.withAlpha(0.10f), area.getX(),
                              area.getBottom(), juce::Colours::transparentBlack, area.getX(),
                              area.getBottom() - width * 0.45f, false);
    g.setGradientFill(lift);
    g.fillRect(area);
    g.restoreState();

    // Thin dark rim so the slot keeps its edge against the panel.
    g.setColour(darkTrough ? dgui::dim().withAlpha(0.45f)
                           : dgui::faint().withAlpha(0.60f));
    g.drawRoundedRectangle(area.reduced(0.5f), width * 0.5f, 1.0f);

    const float radius = width * 0.5f - 3.0f;
    const float centreY = on ? area.getY() + width * 0.5f : area.getBottom() - width * 0.5f;
    const juce::Rectangle<float> puck(area.getCentreX() - radius, centreY - radius,
                                      radius * 2.0f, radius * 2.0f);

    // Clip to the trough so the glow cannot spill past its rounded end.
    juce::Path trackShape;
    trackShape.addRoundedRectangle(area, width * 0.5f);
    g.saveState();
    g.reduceClipRegion(trackShape);

    juce::Path puckPath;
    puckPath.addEllipse(puck);
    juce::DropShadow(juce::Colours::black.withAlpha(dgui::darkMode() ? 0.45f : 0.18f), 6, { 0, 2 })
        .drawForPath(g, puckPath);

    if (on)
    {
        // Filled puck: the on state is obvious at a glance. Same disc and the
        // same edge as the off state, so the thumb does not read as shrinking.
        juce::ColourGradient lit(accent.brighter(0.35f), puck.getX(), puck.getY(),
                                 accent.darker(0.25f), puck.getX(), puck.getBottom(), false);
        g.setGradientFill(lit);
        g.fillEllipse(puck);

        g.setColour(juce::Colours::white.withAlpha(dgui::darkMode() ? 0.16f : 0.45f));
        g.drawEllipse(puck.reduced(0.8f), 1.0f);
    }
    else
    {
        juce::ColourGradient body(dgui::panelHigh().brighter(0.12f), puck.getX(), puck.getY(),
                                  dgui::panel().darker(0.20f), puck.getX(), puck.getBottom(), false);
        g.setGradientFill(body);
        g.fillEllipse(puck);

        g.setColour(juce::Colours::white.withAlpha(dgui::darkMode() ? 0.16f : 0.45f));
        g.drawEllipse(puck.reduced(0.8f), 1.0f);

        if (highlighted || down)
        {
            g.setColour(dgui::dim().withAlpha(0.5f));
            g.drawEllipse(puck.reduced(0.7f), 1.0f);
        }
    }

    g.restoreState();
}

void SwitchButton::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (onContextMenu)
            onContextMenu();
        return;
    }
    juce::Button::mouseDown(event);
}

SwitchCell::SwitchCell(DoomgazerAudioProcessor& processor, int parameterIndex,
                       const juce::String& caption)
    : audioProcessor(processor),
      index(parameterIndex),
      captionText(caption)
{
    addAndMakeVisible(toggle);
    toggle.onContextMenu = [this] { showMenu(); };
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, doomgazer::parameterId(index), toggle);
}

void SwitchCell::resized()
{
    if (inlineCaption)
    {
        // Module row: caption at the left, track at the right, both on the row's
        // centre line.
        auto area = getLocalBounds();
        area.removeFromLeft(juce::jmax(0, area.getWidth() - switchTrackWidth - 4));
        toggle.setBounds(area.withSizeKeepingCentre(
            switchTrackWidth, juce::jlimit(34, 56, getHeight() - 16)));
        return;
    }

    if (captionAbove)
    {
        // Caption above the track, but the track stays on the row's centre line
        // so it lines up with the order diagram and the other switches.
        const int height = juce::jlimit(34, 74, getHeight() - 36);
        toggle.setBounds(getLocalBounds().withSizeKeepingCentre(switchTrackWidth, height));
        return;
    }

    // Footer: caption above the pill, in the same 4..17 band as every knob caption.
    const bool footer = getHeight() > 80;
    const int pillTop = 22;
    const int bottom = getHeight() - (footer ? 0 : 2);
    const int height = juce::jlimit(30, 74, bottom - pillTop);
    toggle.setBounds(getLocalBounds().withSizeKeepingCentre(switchTrackWidth, 0).withTop(pillTop)
                         .withHeight(height));
}

void SwitchCell::paint(juce::Graphics& g)
{
    g.setColour(learning ? dgui::amber() : dgui::dim());
    g.setFont(dgui::trackedFont(9.0f, 1.4f, true));
    if (inlineCaption)
    {
        auto area = getLocalBounds();
        area.removeFromRight(30);
        g.drawFittedText(captionText, area.withTrimmedRight(4),
                         juce::Justification::centredRight, 1);
    }
    else
    {
        g.drawFittedText(captionText, getLocalBounds().withTrimmedTop(4).removeFromTop(13),
                         juce::Justification::centred, 1);
    }

    if (learning)
    {
        g.setColour(dgui::amber().withAlpha(0.85f));
        g.drawRoundedRectangle(toggle.getBounds().toFloat().expanded(4.0f), 9.0f, 1.2f);
    }
}

void SwitchCell::setLearning(bool shouldLearn)
{
    if (learning == shouldLearn)
        return;
    learning = shouldLearn;
    repaint();
}

void SwitchCell::setAccent(juce::Colour newAccent)
{
    if (accent == newAccent)
        return;
    accent = newAccent;
    toggle.setAccent(newAccent);
    repaint();
}

void SwitchCell::setInlineCaption(bool shouldInline)
{
    if (inlineCaption == shouldInline)
        return;
    inlineCaption = shouldInline;
    resized();
    repaint();
}

void SwitchCell::setCaptionAbove(bool shouldBeAbove)
{
    if (captionAbove == shouldBeAbove)
        return;
    captionAbove = shouldBeAbove;
    resized();
    repaint();
}

void SwitchCell::tick()
{
    setLearning(audioProcessor.learnTarget() == index);
}

bool SwitchCell::hitsHint(juce::Point<int> position) const
{
    if (inlineCaption)
        return getLocalBounds().contains(position);
    if (getLocalBounds().removeFromTop(13).contains(position))
        return true;
    if (toggle.getBounds().expanded(3).contains(position))
        return true;
    return getLocalBounds().removeFromBottom(14).contains(position);
}

void SwitchCell::showMenu()
{
    const int cc = audioProcessor.ccForParameter(index);
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(doomgazer::parameterName(index)));
    menu.addItem(1, "MIDI Learn");
    menu.addItem(2, cc >= 0 ? "Clear MIDI (" + juce::String(cc) + ")" : "Clear MIDI", cc >= 0);
    menu.addSeparator();
    menu.addItem(3, "Reset to default");

    juce::Component::SafePointer<SwitchCell> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&toggle),
                       [safe](int result) {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result == 1)
                           {
                               safe->audioProcessor.beginLearn(safe->index);
                               safe->setLearning(true);
                           }
                           else if (result == 2)
                           {
                               safe->audioProcessor.clearMapping(safe->index);
                           }
                           else if (result == 3)
                           {
                               if (auto* parameter = safe->audioProcessor.apvts.getParameter(
                                       doomgazer::parameterId(safe->index)))
                                   parameter->setValueNotifyingHost(parameter->getDefaultValue());
                           }
                       });
}

SelectorCell::SelectorCell(DoomgazerAudioProcessor& processor, int parameterIndex,
                             const juce::String& caption, const juce::String& firstLabel,
                             const juce::String& secondLabel)
    : audioProcessor(processor),
      index(parameterIndex),
      captionText(caption)
{
    labels[0] = firstLabel;
    labels[1] = secondLabel;
    if (const auto* value = audioProcessor.apvts.getRawParameterValue(doomgazer::parameterId(index)))
        shownState = value->load() > 0.5f ? 1 : 0;
}

void SelectorCell::resized()
{
}

juce::Rectangle<int> SelectorCell::trackBounds() const
{
    // In a module row a captioned selector keeps its label inline at the left, so
    // the whole row reads left to right: label, control, label, control. Small
    // cells (the header engine switch) centre a 26-high track with no label.
    auto area = getLocalBounds();
    if (area.getHeight() < 50)
    {
        // Compact cell (the header engine switch): centre a short track.
        const int width = juce::jmin(area.getWidth() - 2, 148);
        const int height = juce::jmin(26, area.getHeight() - 2);
        return juce::Rectangle<int>(area.getX() + (area.getWidth() - width) / 2,
                                    (area.getHeight() - height) / 2, width, height);
    }

    const bool inlineCaption = captionText.isNotEmpty();
    const int inset = inlineCaption ? 54 : 0;
    const int width = juce::jmin(area.getWidth() - inset - 2, 148);
    const int x = area.getX() + inset + (area.getWidth() - inset - width) / 2;
    // The caption is inline, so the track uses the whole height and shares the
    // row's centre line with the mode pickers and the switches.
    const int height = juce::jmin(34, area.getHeight() - 2);
    return juce::Rectangle<int>(x, area.getY() + (area.getHeight() - height) / 2, width, height);
}

void SelectorCell::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    if (captionText.isNotEmpty())
    {
        const bool inlineCaption = area.getHeight() >= 50;
        auto captionArea = inlineCaption ? area.removeFromLeft(52)
                                         : area.withTrimmedTop(4).removeFromTop(13);
        g.setColour(learning ? dgui::amber() : dgui::dim());
        g.setFont(dgui::trackedFont(9.0f, 1.4f, true));
        g.drawFittedText(captionText, captionArea,
                         inlineCaption ? juce::Justification::centredRight
                                       : juce::Justification::centred,
                         1);
    }

    const auto track = trackBounds();
    const float corner = static_cast<float>(track.getHeight()) * 0.5f;

    g.setColour(dgui::darkMode() ? dgui::well() : dgui::well().darker(0.06f));
    g.fillRoundedRectangle(track.toFloat(), corner);

    const auto half = track.withWidth(track.getWidth() / 2);
    const auto halves = std::array<juce::Rectangle<int>, 2> {
        half,
        track.withTrimmedLeft(half.getWidth())
    };
    for (int state = 0; state < 2; ++state)
    {
        const bool active = shownState == state;
        if (active)
        {
            g.setColour(accents[static_cast<std::size_t>(state)]);
            g.fillRoundedRectangle(halves[static_cast<std::size_t>(state)].toFloat().reduced(1.5f),
                                   corner - 1.5f);
        }
        g.setColour(active ? dgui::ink() : dgui::dim());
        g.setFont(dgui::trackedFont(9.0f, 1.0f, true));
        g.drawFittedText(labels[state], halves[static_cast<std::size_t>(state)],
                         juce::Justification::centred, 1);
    }

    g.setColour(dgui::line());
    g.drawRoundedRectangle(track.toFloat().reduced(0.5f), corner, 1.0f);

    if (learning)
    {
        g.setColour(dgui::amber().withAlpha(0.9f));
        g.drawRoundedRectangle(track.toFloat().expanded(2.5f), corner + 2.5f, 1.4f);
    }
}

bool SelectorCell::hitsHint(juce::Point<int> positionInCell) const
{
    return getLocalBounds().contains(positionInCell);
}

void SelectorCell::setAccent(juce::Colour newAccent)
{
    setAccents(newAccent, newAccent);
}

void SelectorCell::setAccents(juce::Colour firstColour, juce::Colour secondColour)
{
    if (accents[0] == firstColour && accents[1] == secondColour)
        return;
    accents[0] = firstColour;
    accents[1] = secondColour;
    accent = firstColour;
    repaint();
}

void SelectorCell::setLearning(bool shouldLearn)
{
    if (learning == shouldLearn)
        return;
    learning = shouldLearn;
    repaint();
}

void SelectorCell::tick()
{
    const auto* value = audioProcessor.apvts.getRawParameterValue(doomgazer::parameterId(index));
    const int state = value != nullptr && value->load() > 0.5f ? 1 : 0;
    if (state != shownState)
    {
        shownState = state;
        repaint();
    }
    setLearning(audioProcessor.learnTarget() == index);
}

void SelectorCell::choose(bool second)
{
    if (auto* parameter = audioProcessor.apvts.getParameter(doomgazer::parameterId(index)))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(second ? 1.0f : 0.0f));
}

void SelectorCell::showMenu()
{
    const int cc = audioProcessor.ccForParameter(index);
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(doomgazer::parameterName(index)));
    menu.addItem(1, "MIDI Learn");
    menu.addItem(2, cc >= 0 ? "Clear MIDI (" + juce::String(cc) + ")" : "Clear MIDI", cc >= 0);
    menu.addSeparator();
    menu.addItem(3, "Reset to default");

    juce::Component::SafePointer<SelectorCell> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe](int result) {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result == 1)
                           {
                               safe->audioProcessor.beginLearn(safe->index);
                               safe->setLearning(true);
                           }
                           else if (result == 2)
                           {
                               safe->audioProcessor.clearMapping(safe->index);
                           }
                           else if (result == 3)
                           {
                               if (auto* parameter = safe->audioProcessor.apvts.getParameter(
                                       doomgazer::parameterId(safe->index)))
                                   parameter->setValueNotifyingHost(parameter->getDefaultValue());
                           }
                       });
}

void SelectorCell::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        showMenu();
        return;
    }
    choose(event.getPosition().x > trackBounds().getCentreX());
}

OrderCell::OrderCell(DoomgazerAudioProcessor& processor, int parameterIndex,
                     const juce::String& firstStage, const juce::String& secondStage)
    : audioProcessor(processor),
      index(parameterIndex)
{
    stages[0] = firstStage;
    stages[1] = secondStage;
    addAndMakeVisible(toggle);
    toggle.setClickingTogglesState(true);
    toggle.onContextMenu = [this] { showMenu(); };
    toggle.onClick = [this] {
        // Up is the first path. The parameter is 0 for "first stage first".
        if (auto* parameter = audioProcessor.apvts.getParameter(doomgazer::parameterId(index)))
            parameter->setValueNotifyingHost(
                parameter->convertTo0to1(toggle.getToggleState() ? 0.0f : 1.0f));
    };
    if (const auto* value = audioProcessor.apvts.getRawParameterValue(doomgazer::parameterId(index)))
    {
        shownState = value->load() > 0.5f ? 1 : 0;
        toggle.setToggleState(shownState == 0, juce::dontSendNotification);
    }
}

int OrderCell::toggleHeight() const
{
    return juce::jlimit(34, 74, getHeight() - 8);
}

void OrderCell::resized()
{
    const int height = toggleHeight();
    toggle.setBounds(8, (getHeight() - height) / 2, switchTrackWidth, height);
}

void OrderCell::drawPath(juce::Graphics& g, juce::Rectangle<int> row, const juce::String& first,
                         const juce::String& second, bool active) const
{
    const juce::Colour ink = active ? accent : dgui::faint();
    const int wordWidth = juce::jmin(50, row.getWidth() / 3);
    auto line = row.reduced(2, 0);
    auto leftWord = line.removeFromLeft(wordWidth);
    auto rightWord = line.removeFromRight(wordWidth);

    g.setColour(ink);
    g.setFont(dgui::trackedFont(9.0f, 1.0f, active));
    g.drawFittedText(first, leftWord, juce::Justification::centredLeft, 1);
    g.drawFittedText(second, rightWord, juce::Justification::centredRight, 1);

    // The rule runs from one word to the next with an arrowhead, so the order
    // reads left to right.
    const float x1 = static_cast<float>(leftWord.getRight()) + 3.0f;
    const float x2 = static_cast<float>(rightWord.getX()) - 6.0f;
    const float y = static_cast<float>(row.getCentreY());
    g.setColour(ink.withAlpha(active ? 0.95f : 0.55f));
    g.drawLine(x1, y, x2, y, active ? 1.6f : 1.0f);
    juce::Path head;
    head.addTriangle(x2 + 1.0f, y, x2 - 4.0f, y - 3.2f, x2 - 4.0f, y + 3.2f);
    g.fillPath(head);
}

void OrderCell::paint(juce::Graphics& g)
{
    // The two chains already say what the control is, so there is no caption.
    // The extra room goes to the gap between the switch and the diagram.
    auto rows = getLocalBounds().withTrimmedLeft(58).withTrimmedRight(2);
    // The diagram block and the switch use the same height and centre, so their
    // edges line up exactly.
    const int rowsHeight = juce::jmin(toggleHeight(), getHeight());
    rows = rows.withSizeKeepingCentre(rows.getWidth(), rowsHeight);
    auto top = rows.removeFromTop(rows.getHeight() / 2);
    drawPath(g, top, stages[0], stages[1], shownState == 0);
    drawPath(g, rows, stages[1], stages[0], shownState == 1);

    // A bracket joins the switch to both rows.
    const float bracketX = 44.0f;
    const float topY = static_cast<float>(top.getCentreY());
    const float bottomY = static_cast<float>(rows.getCentreY());
    g.setColour(dgui::dim().withAlpha(0.55f));
    g.drawLine(bracketX, topY, bracketX, bottomY, 1.0f);
    g.drawLine(bracketX, topY, bracketX + 12.0f, topY, 1.0f);
    g.drawLine(bracketX, bottomY, bracketX + 12.0f, bottomY, 1.0f);

    if (learning)
    {
        g.setColour(dgui::amber().withAlpha(0.9f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 6.0f, 1.4f);
    }
}

bool OrderCell::hitsHint(juce::Point<int> positionInCell) const
{
    return getLocalBounds().contains(positionInCell);
}

void OrderCell::setAccent(juce::Colour newAccent)
{
    if (accent == newAccent)
        return;
    accent = newAccent;
    toggle.setAccent(newAccent);
    repaint();
}

void OrderCell::setLearning(bool shouldLearn)
{
    if (learning == shouldLearn)
        return;
    learning = shouldLearn;
    repaint();
}

void OrderCell::tick()
{
    const auto* value = audioProcessor.apvts.getRawParameterValue(doomgazer::parameterId(index));
    const int state = value != nullptr && value->load() > 0.5f ? 1 : 0;
    if (state != shownState)
    {
        shownState = state;
        toggle.setToggleState(state == 0, juce::dontSendNotification);
        repaint();
    }
    setLearning(audioProcessor.learnTarget() == index);
}

void OrderCell::showMenu()
{
    const int cc = audioProcessor.ccForParameter(index);
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(doomgazer::parameterName(index)));
    menu.addItem(1, "MIDI Learn");
    menu.addItem(2, cc >= 0 ? "Clear MIDI (" + juce::String(cc) + ")" : "Clear MIDI", cc >= 0);
    juce::Component::SafePointer<OrderCell> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [safe](int result) {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result == 1)
                           {
                               safe->audioProcessor.beginLearn(safe->index);
                               safe->setLearning(true);
                           }
                           else if (result == 2)
                           {
                               safe->audioProcessor.clearMapping(safe->index);
                           }
                       });
}

ThemeButton::ThemeButton()
    : juce::Button("theme")
{
}

namespace {

// A disc with a bite out of its top right, as one closed path. Cutting it as a
// path keeps the crescent independent of the colour the button is filled with.
juce::Path crescentPath(juce::Point<float> centre, float radius)
{
    const juce::Point<float> bite { centre.x + radius * 0.65f, centre.y - radius * 0.45f };
    const float biteRadius = radius * 0.975f;
    const auto offset = bite - centre;
    const float distance = offset.getDistanceFromOrigin();
    const float axis = std::atan2(offset.y, offset.x);
    const float outer = std::acos(juce::jlimit(
        -1.0f, 1.0f,
        (distance * distance + radius * radius - biteRadius * biteRadius)
            / (2.0f * distance * radius)));
    const float inner = std::acos(juce::jlimit(
        -1.0f, 1.0f,
        (distance * distance + biteRadius * biteRadius - radius * radius)
            / (2.0f * distance * biteRadius)));

    constexpr int steps = 24;
    const auto onCircle = [](juce::Point<float> origin, float r, float angle) {
        return origin + juce::Point<float>(std::cos(angle), std::sin(angle)) * r;
    };

    juce::Path path;
    for (int step = 0; step <= steps; ++step)
    {
        // Outer edge: the part of the disc the bite does not reach, the long way.
        const float sweep = juce::MathConstants<float>::twoPi - 2.0f * outer;
        const float angle = axis + outer + sweep * static_cast<float>(step) / steps;
        const auto point = onCircle(centre, radius, angle);
        if (step == 0)
            path.startNewSubPath(point);
        else
            path.lineTo(point);
    }
    for (int step = 0; step <= steps; ++step)
    {
        // Inner edge: the part of the bite that falls inside the disc, from the
        // far intersection back to the near one.
        const float angle = axis + juce::MathConstants<float>::pi + inner
            - 2.0f * inner * static_cast<float>(step) / steps;
        path.lineTo(onCircle(bite, biteRadius, angle));
    }
    path.closeSubPath();
    return path;
}

} // namespace

void ThemeButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);

    // The same fill as the pills beside it. At full card opacity the light theme
    // made this a lighter well sitting in the row rather than a button.
    g.setColour((highlighted || down) ? dgui::panelHigh() : dgui::panel().withAlpha(0.55f));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(dgui::line());
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.22f;
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();

    switch (dgui::theme())
    {
        case dgui::Theme::light:
        {
            // Sun.
            g.setColour(dgui::text());
            g.fillEllipse(cx - radius * 0.62f, cy - radius * 0.62f, radius * 1.24f,
                          radius * 1.24f);
            for (int ray = 0; ray < 8; ++ray)
            {
                const float angle = juce::MathConstants<float>::twoPi
                    * static_cast<float>(ray) / 8.0f;
                const float inner = radius * 1.05f;
                const float outer = radius * 1.55f;
                g.drawLine(cx + std::cos(angle) * inner, cy + std::sin(angle) * inner,
                           cx + std::cos(angle) * outer, cy + std::sin(angle) * outer, 1.2f);
            }
            break;
        }
        case dgui::Theme::dark:
        {
            g.setColour(dgui::text());
            g.fillPath(crescentPath({ cx, cy }, radius));
            break;
        }
    }
}

void HintCard::setHint(const juce::String& section, const juce::String& title,
                       const juce::String& body)
{
    if (section == sectionText && title == titleText && body == bodyText)
        return;
    sectionText = section;
    titleText = title;
    bodyText = body;
    repaint();
}

void HintCard::setCompact(bool shouldCompact)
{
    if (compact == shouldCompact)
        return;
    compact = shouldCompact;
    repaint();
}

void HintCard::paint(juce::Graphics& g)
{
    // The card behind this is drawn in the editor backdrop.
    auto inner = compact ? getLocalBounds().reduced(12, 9) : getLocalBounds().reduced(18, 14);
    if (!compact)
        inner.removeFromTop(6);

    if (titleText.isNotEmpty())
    {
        g.setColour(dgui::text());
        g.setFont(compact ? dgui::trackedFont(9.0f, 1.4f, true) : dgui::font(15.0f, true));
        g.drawFittedText(titleText, inner.removeFromTop(compact ? 14 : 20),
                         juce::Justification::centredLeft, 1);
        inner.removeFromTop(compact ? 3 : 7);
    }

    g.setColour(dgui::dim());
    g.setFont(compact ? dgui::font(10.5f) : dgui::font(11.0f));
    g.drawFittedText(bodyText, inner, juce::Justification::topLeft, compact ? 3 : 4, 1.0f);
}

ScopePanel::ScopePanel(DoomgazerAudioProcessor& processor)
    : audioProcessor(processor)
{
}

void ScopePanel::resample(const std::array<float, doomgazer::Processor::kScopeSize>& data,
                          int write, int span, std::vector<float>& target, int width) const
{
    constexpr int ring = doomgazer::Processor::kScopeSize;
    target.resize(static_cast<std::size_t>(juce::jmax(1, width)));
    const int columns = static_cast<int>(target.size());
    for (int i = 0; i < columns; ++i)
    {
        const float position = static_cast<float>(i) * static_cast<float>(span - 1)
            / static_cast<float>(juce::jmax(1, columns - 1));
        const int base = static_cast<int>(position);
        const float fraction = position - static_cast<float>(base);
        const int read0 = (write - base + ring) % ring;
        const int read1 = (write - base - 1 + ring) % ring;
        target[static_cast<std::size_t>(i)] =
            data[static_cast<std::size_t>(read0)] * (1.0f - fraction)
            + data[static_cast<std::size_t>(read1)] * fraction;
    }
}

void ScopePanel::paint(juce::Graphics& g)
{
    auto inner = getLocalBounds().reduced(18, 12);
    dgui::drawPanel(g, inner, "SCOPE", dgui::line());
    inner.removeFromTop(14);

    const auto plot = inner.toFloat();
    dgui::fillWell(g, plot, 4.0f);

    for (int line = 1; line < 4; ++line)
    {
        const int y = inner.getY() + inner.getHeight() * line / 4;
        g.setColour(dgui::darkMode() ? dgui::line().withAlpha(0.34f)
                                     : dgui::faint().withAlpha(0.26f));
        g.drawHorizontalLine(y, static_cast<float>(inner.getX() + 1),
                             static_cast<float>(inner.getRight() - 1));
    }
    g.setColour(dgui::darkMode() ? dgui::line().withAlpha(0.85f)
                                 : dgui::faint().withAlpha(0.45f));
    g.drawHorizontalLine(inner.getCentreY(), static_cast<float>(inner.getX() + 1),
                         static_cast<float>(inner.getRight() - 1));

    const int width = juce::jmax(2, inner.getWidth());
    constexpr int span = 640;
    const auto& engine = audioProcessor.engine();
    resample(engine.inputScopeBuffer(), engine.inputScopeWriteIndex(), span, inputWave, width);
    resample(engine.scopeBuffer(), engine.scopeWriteIndex(), span, outputWave, width);

    const float midY = static_cast<float>(inner.getCentreY());
    const float top = static_cast<float>(inner.getY() + 1);
    const float scale = static_cast<float>(inner.getHeight()) * 0.42f;
    const float x0 = static_cast<float>(inner.getX());

    juce::Path inputPath;
    juce::Path outputPath;
    for (int i = 0; i < width; ++i)
    {
        const float x = x0 + static_cast<float>(i);
        const float inY = juce::jlimit(top, top + scale * 2.0f,
                                       midY - inputWave[static_cast<std::size_t>(i)] * scale);
        const float outY = juce::jlimit(top, top + scale * 2.0f,
                                        midY - outputWave[static_cast<std::size_t>(i)] * scale);
        if (i == 0)
        {
            inputPath.startNewSubPath(x, inY);
            outputPath.startNewSubPath(x, outY);
        }
        else
        {
            inputPath.lineTo(x, inY);
            outputPath.lineTo(x, outY);
        }
    }

    g.setColour(dgui::faint().withAlpha(0.50f));
    g.strokePath(inputPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour(dgui::amber());
    g.strokePath(outputPath, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    dgui::strokeWell(g, plot, 4.0f);
}

SpectrumPanel::SpectrumPanel(DoomgazerAudioProcessor& processor)
    : audioProcessor(processor)
{
}

void SpectrumPanel::paint(juce::Graphics& g)
{
    auto inner = getLocalBounds().reduced(18, 12);
    dgui::drawPanel(g, inner, "SPECTRUM", dgui::line());
    inner.removeFromTop(14);

    auto axis = inner.removeFromBottom(12);
    const auto plot = inner.toFloat();
    dgui::fillWell(g, plot, 4.0f);

    for (int line = 1; line < 4; ++line)
    {
        const int y = inner.getY() + inner.getHeight() * line / 4;
        g.setColour(dgui::darkMode() ? dgui::line().withAlpha(0.34f)
                                     : dgui::faint().withAlpha(0.26f));
        g.drawHorizontalLine(y, static_cast<float>(inner.getX() + 1),
                             static_cast<float>(inner.getRight() - 1));
    }

    const auto bands = audioProcessor.engine().inputSpectrum();
    const int count = static_cast<int>(bands.size());
    if (count < 2 || inner.getWidth() < 8)
        return;

    smoothed.resize(static_cast<std::size_t>(count));
    float loudest = 0.0f;
    for (int band = 0; band < count; ++band)
    {
        const float amount = juce::jlimit(0.0f, 1.0f, bands[static_cast<std::size_t>(band)]);
        auto& value = smoothed[static_cast<std::size_t>(band)];
        value += (amount - value) * 0.35f;
        loudest = juce::jmax(loudest, value);
    }

    const int steps = juce::jmax(16, count * 6);
    points.clear();
    points.reserve(static_cast<std::size_t>(steps));
    for (int i = 0; i < steps; ++i)
    {
        const float position = static_cast<float>(i) * static_cast<float>(count - 1)
            / static_cast<float>(steps - 1);
        const int low = static_cast<int>(position);
        const int high = juce::jmin(count - 1, low + 1);
        const float fraction = position - static_cast<float>(low);
        const float amount = smoothed[static_cast<std::size_t>(low)] * (1.0f - fraction)
            + smoothed[static_cast<std::size_t>(high)] * fraction;
        points.push_back({ plot.getX() + plot.getWidth() * static_cast<float>(i)
                               / static_cast<float>(steps - 1),
                           plot.getBottom() - 1.5f
                               - amount * (plot.getHeight() - 4.0f) });
    }

    if (loudest > 0.02f)
    {
        juce::Path areaPath;
        areaPath.startNewSubPath(points.front().x, plot.getBottom());
        for (const auto& point : points)
            areaPath.lineTo(point);
        areaPath.lineTo(points.back().x, plot.getBottom());
        areaPath.closeSubPath();

        juce::ColourGradient fill(dgui::amber().withAlpha(0.50f), 0.0f, plot.getY(),
                                  dgui::blue().withAlpha(0.04f), 0.0f, plot.getBottom(), false);
        g.setGradientFill(fill);
        g.fillPath(areaPath);

        juce::Path linePath;
        linePath.startNewSubPath(points.front());
        for (std::size_t i = 1; i < points.size(); ++i)
            linePath.lineTo(points[i]);
        g.setColour(dgui::amber());
        g.strokePath(linePath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    static const char* labels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    g.setFont(dgui::font(8.5f));
    for (int i = 0; i < 8; ++i)
    {
        const float x = plot.getX() + 6.0f
            + (plot.getWidth() - 12.0f) * static_cast<float>(i) / 7.0f;
        g.setColour(dgui::faint());
        g.drawText(labels[i], juce::roundToInt(x) - 20, axis.getY(), 40, axis.getHeight(),
                   juce::Justification::centred);
        g.setColour(dgui::line().withAlpha(0.8f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getBottom() - 4.0f, plot.getBottom());
    }

    dgui::strokeWell(g, plot, 4.0f);
}

TunerPanel::TunerPanel(DoomgazerAudioProcessor& processor)
    : audioProcessor(processor)
{
}

void TunerPanel::paint(juce::Graphics& g)
{
    auto inner = getLocalBounds().reduced(18, 12);
    dgui::drawPanel(g, inner, "TUNER", dgui::line());
    inner.removeFromTop(18);

    const auto well = inner;
    dgui::fillWell(g, well.toFloat(), 4.0f);

    auto content = well.reduced(16, 10);
    auto noteBlock = content.removeFromLeft(40);
    content.removeFromLeft(6);
    auto rulerArea = content;

    const doomgazer::TunerReadout reading = audioProcessor.engine().tuner();
    juce::Colour noteColour = dgui::faint();
    juce::String noteText = "--";

    if (reading.active && reading.midiNote >= 0)
    {
        noteText = juce::String(doomgazer::noteName(reading.noteIndex))
            + juce::String(reading.octave);
        noteColour = std::abs(reading.cents) <= 7.0 ? dgui::green() : dgui::red();
    }

    g.setColour(noteColour);
    g.setFont(dgui::font(30.0f, true));
    g.drawFittedText(noteText, noteBlock, juce::Justification::centredLeft, 1);

    // Ruler to the right of the note block.
    const float left = static_cast<float>(rulerArea.getX());
    const float rightEdge = static_cast<float>(rulerArea.getRight());
    const float centreX = (left + rightEdge) * 0.5f;
    const float y = static_cast<float>(rulerArea.getCentreY()) + 2.0f;

    g.setColour(dgui::darkMode() ? dgui::line().withAlpha(0.85f)
                                 : dgui::faint().withAlpha(0.55f));
    g.drawLine(left, y, rightEdge, y, 2.0f);

    g.setColour(dgui::faint());
    for (int tick = -2; tick <= 2; ++tick)
    {
        const float x = centreX + static_cast<float>(tick) * (rightEdge - left) * 0.22f;
        const float reach = tick == 0 ? 7.0f : 4.0f;
        g.drawLine(x, y - reach, x, y + reach, tick == 0 ? 2.0f : 1.5f);
    }

    if (reading.active)
    {
        const float amount = juce::jlimit(-1.0f, 1.0f, static_cast<float>(reading.cents / 50.0));
        const float x = centreX + amount * (rightEdge - left) * 0.44f;
        g.setColour(noteColour);
        g.drawLine(x, y - 9.0f, x, y + 9.0f, 3.0f);
    }

    dgui::strokeWell(g, well.toFloat(), 4.0f);
}

LevelMeter::LevelMeter(DoomgazerAudioProcessor& processor)
    : audioProcessor(processor)
{
    setInterceptsMouseClicks(false, false);
}

void LevelMeter::tick()
{
    const auto& engine = audioProcessor.engine();
    const auto& buffer = engine.scopeBuffer();
    const int size = static_cast<int>(buffer.size());
    const int write = engine.scopeWriteIndex();
    constexpr int span = 512;
    const auto now = juce::Time::getMillisecondCounter();
    bool changed = false;

    float rawPeak = 0.0f;
    double sumSquares = 0.0;
    for (int i = 0; i < span; ++i)
    {
        const float sample = buffer[static_cast<std::size_t>((write - i + size) % size)];
        rawPeak = juce::jmax(rawPeak, std::abs(sample));
        sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }
    const float rawRms = static_cast<float>(std::sqrt(sumSquares / span));

    if (rawPeak >= 1.0f && !clipped)
    {
        clipped = true;
        clipUntil = now + 1800;
        changed = true;
    }
    else if (clipped && now > clipUntil && rawPeak < 1.0f)
    {
        clipped = false;
        changed = true;
    }

    // A little ballistics so the bars do not flicker at 30 Hz.
    const float nextPeak = juce::jmax(rawPeak, peak * 0.90f);
    const float nextRms = juce::jmax(rawRms, rms * 0.86f);
    if (std::abs(nextPeak - peak) > 0.0005f || std::abs(nextRms - rms) > 0.0005f)
    {
        peak = nextPeak;
        rms = nextRms;
        changed = true;
    }

    const float pre = audioProcessor.engine().takeInputPrePeak();
    const float post = audioProcessor.engine().takeInputPostPeak();
    float nextInput = inputLevel;
    if (post >= nextInput)
    {
        nextInput = post;
        inputHoldUntil = now + 300;
    }
    else if (now >= inputHoldUntil)
    {
        nextInput *= 0.84f;
    }
    if (std::abs(nextInput - inputLevel) > 0.0005f)
    {
        inputLevel = nextInput;
        changed = true;
    }

    // -1 dBFS on the way in. Trim can pad this back into the window afterwards.
    const float clipGain = juce::Decibels::decibelsToGain(-1.0f);
    if (pre >= clipGain && !inputClipped)
    {
        inputClipped = true;
        inputClipUntil = now + 1800;
        changed = true;
    }
    else if (inputClipped && now > inputClipUntil && pre < clipGain)
    {
        inputClipped = false;
        changed = true;
    }

    if (changed)
        repaint();
}

void LevelMeter::drawLamp(juce::Graphics& g, juce::Rectangle<float> row, bool lit) const
{
    // The socket is drawn whether or not it is lit, so the unlit state reads as
    // a lamp that is off rather than as a gap in the row.
    auto lamp = juce::Rectangle<float>(row.getX(), row.getCentreY() - 5.5f, row.getWidth(), 11.0f)
                    .withSizeKeepingCentre(11.0f, 11.0f);
    g.setColour(dgui::darkMode() ? dgui::well().brighter(0.10f) : dgui::well().darker(0.12f));
    g.fillRoundedRectangle(lamp, 2.5f);
    g.setColour(dgui::darkMode() ? dgui::line().withAlpha(0.95f)
                                 : dgui::faint().withAlpha(0.85f));
    g.drawRoundedRectangle(lamp.reduced(0.5f), 2.5f, 1.0f);
    if (lit)
    {
        g.setColour(dgui::red().withAlpha(0.30f));
        g.fillRoundedRectangle(lamp.expanded(3.0f), 4.0f);
        g.setColour(dgui::red());
        g.fillRoundedRectangle(lamp, 2.5f);
    }
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    auto labels = area.removeFromLeft(44);
    auto lampBlock = area.removeFromRight(14);
    area.removeFromRight(6);

    constexpr int trackHeight = 11;
    constexpr int trackGap = 4;
    const auto takeRow = [](juce::Rectangle<int>& block) {
        auto row = block.removeFromTop(trackHeight);
        if (block.getHeight() >= trackGap)
            block.removeFromTop(trackGap);
        return row;
    };

    auto inputLabel = takeRow(labels);
    auto peakLabel = takeRow(labels);
    auto rmsLabel = labels.removeFromTop(trackHeight);

    auto inputArea = takeRow(area).toFloat();
    auto peakArea = takeRow(area).toFloat();
    auto rmsArea = area.removeFromTop(trackHeight).toFloat();
    auto inputLamp = takeRow(lampBlock).toFloat();
    auto outputLamp = lampBlock.toFloat();

    g.setColour(dgui::dim());
    g.setFont(dgui::trackedFont(7.5f, 1.2f, true));
    g.drawText("INPUT", inputLabel, juce::Justification::centredLeft);
    g.drawText("PEAK", peakLabel, juce::Justification::centredLeft);
    g.drawText("RMS", rmsLabel, juce::Justification::centredLeft);

    const auto dbToFraction = [](float level) {
        const float db = juce::Decibels::gainToDecibels(level, -60.0f);
        return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };

    const auto drawTrack = [&g, &dbToFraction](juce::Rectangle<float> rect, float level,
                                               juce::Colour colour) {
        dgui::fillWell(g, rect, 3.0f);
        juce::Path clipPath;
        clipPath.addRoundedRectangle(rect, 3.0f);
        g.saveState();
        g.reduceClipRegion(clipPath);
        const float width = rect.getWidth() * dbToFraction(level);
        if (width > 0.5f)
        {
            juce::ColourGradient fill(colour, rect.getX(), rect.getY(),
                                      colour.withAlpha(0.55f), rect.getX() + width,
                                      rect.getY(), false);
            g.setGradientFill(fill);
            g.fillRect(rect.withWidth(width));
        }
        g.restoreState();
        dgui::strokeWell(g, rect, 3.0f);
    };

    drawTrack(inputArea, inputLevel, inputClipped ? dgui::red() : dgui::amber());
    drawTrack(peakArea, peak, clipped ? dgui::red() : dgui::amber());
    drawTrack(rmsArea, rms, dgui::green().withAlpha(0.9f));

    for (const float fraction : { 0.25f, 0.5f, 0.75f })
    {
        const float x = peakArea.getX() + peakArea.getWidth() * fraction;
        g.setColour(dgui::faint().withAlpha(dgui::darkMode() ? 0.35f : 0.26f));
        g.drawVerticalLine(juce::roundToInt(x), peakArea.getY() + 1.0f, rmsArea.getBottom() - 1.0f);
    }

    // The factory window, on the input track only. Same -60 dB scale as the bars.
    g.setColour(dgui::green());
    for (const float db : { kInputWindowLowDb, kInputWindowHighDb })
    {
        const float fraction = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        const float x = inputArea.getX() + inputArea.getWidth() * fraction;
        g.drawLine(x, inputArea.getY() + 1.0f, x, inputArea.getBottom() - 1.0f, 1.5f);
    }

    drawLamp(g, inputLamp, inputClipped);
    drawLamp(g, outputLamp, clipped);
}

void Led::paint(juce::Graphics& g)
{
    auto dot = getLocalBounds().toFloat().withSizeKeepingCentre(7.0f, 7.0f);
    if (glow > 0.01f)
    {
        g.setColour(colour.withAlpha(0.22f * glow));
        g.fillEllipse(dot.expanded(3.0f + 3.0f * glow));
    }
    g.setColour(colour.withAlpha(0.30f + 0.70f * glow));
    g.fillEllipse(dot);
    g.setColour(dgui::line());
    g.drawEllipse(dot.reduced(0.5f), 1.0f);
}

void Led::setLedColour(juce::Colour newColour)
{
    if (colour == newColour)
        return;
    colour = newColour;
    repaint();
}

void Led::flash()
{
    glow = 1.0f;
    repaint();
}

void Led::tick()
{
    if (glow <= 0.01f)
        return;
    glow *= 0.86f;
    repaint();
}

void PresetBarButton::mouseWheelMove(const juce::MouseEvent& event,
                                     const juce::MouseWheelDetails& wheel)
{
    if (onStep && std::abs(wheel.deltaY) > 0.001f)
    {
        const int direction = wheel.deltaY > 0.0f ? 1 : -1;
        onStep(direction);
        return;
    }
    juce::TextButton::mouseWheelMove(event, wheel);
}

} // namespace dgui
