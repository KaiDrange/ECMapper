#include "AppStyle.h"

namespace ecm::Style {

juce::Colour background()   { return juce::Colour(0xff0f141a); }
juce::Colour surface()      { return juce::Colour(0xff171d26); }
juce::Colour surfaceRaised(){ return juce::Colour(0xff202836); }
juce::Colour border()       { return juce::Colour(0xff334154); }
juce::Colour text()         { return juce::Colour(0xffedf3f8); }
juce::Colour mutedText()    { return juce::Colour(0xff98a6b5); }
juce::Colour accent()       { return juce::Colour(0xff2bb6df); }
juce::Colour accentStrong() { return juce::Colour(0xff68d6ff); }
juce::Colour warning()      { return juce::Colour(0xffd2a24b); }
juce::Colour danger()       { return juce::Colour(0xffc96f6f); }

juce::Colour zoneColour(Zone zone)
{
    switch (zone)
    {
        case Zone::Zone1: return juce::Colour(0xff1b1bb0);
        case Zone::Zone2: return juce::Colours::maroon;
        case Zone::Zone3: return juce::Colours::darkorange;
        case Zone::AllZones: return juce::Colours::white;
        default: return juce::Colours::black;
    }
}

juce::Colour tabColour(int tabIndex)
{
    switch (tabIndex)
    {
        case 0: return juce::Colour(0xff4d79a6);
        case 1: return juce::Colour(0xff3f8aa8);
        case 2: return juce::Colour(0xff5f7aa8);
        case 3: return juce::Colour(0xff4e8f84);
        default: return accent();
    }
}

juce::LookAndFeel_V4::ColourScheme colourScheme()
{
    return {
        background().getARGB(),
        surface().getARGB(),
        surface().getARGB(),
        border().getARGB(),
        text().getARGB(),
        accent().getARGB(),
        text().getARGB(),
        surfaceRaised().getARGB(),
        text().getARGB()
    };
}

} // namespace ecm::Style

namespace ecm {

AppLookAndFeel::AppLookAndFeel()
{
    setColourScheme(Style::colourScheme());

    setColour(juce::ResizableWindow::backgroundColourId, Style::background());
    setColour(juce::TabbedComponent::backgroundColourId, Style::surface());
    setColour(juce::TabbedComponent::outlineColourId, Style::border());
    setColour(juce::TabbedButtonBar::tabOutlineColourId, Style::border());
    setColour(juce::TabbedButtonBar::frontOutlineColourId, Style::accentStrong());

    setColour(juce::Label::textColourId, Style::text());
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour(juce::TextEditor::backgroundColourId, Style::surface());
    setColour(juce::TextEditor::textColourId, Style::text());
    setColour(juce::TextEditor::outlineColourId, Style::border());
    setColour(juce::TextEditor::focusedOutlineColourId, Style::accent());
    setColour(juce::TextEditor::highlightColourId, Style::accent().withAlpha(0.35f));
    setColour(juce::TextEditor::highlightedTextColourId, Style::background());
    setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

    setColour(juce::ComboBox::backgroundColourId, Style::surface());
    setColour(juce::ComboBox::outlineColourId, Style::border());
    setColour(juce::ComboBox::textColourId, Style::text());
    setColour(juce::ComboBox::arrowColourId, Style::mutedText());
    setColour(juce::ComboBox::focusedOutlineColourId, Style::accent());
    setColour(juce::ComboBox::buttonColourId, Style::surfaceRaised());

    setColour(juce::TextButton::buttonColourId, Style::surfaceRaised());
    setColour(juce::TextButton::buttonOnColourId, Style::accent());
    setColour(juce::TextButton::textColourOffId, Style::text());
    setColour(juce::TextButton::textColourOnId, Style::background());

    setColour(juce::ToggleButton::textColourId, Style::text());
    setColour(juce::ToggleButton::tickColourId, Style::accentStrong());
    setColour(juce::ToggleButton::tickDisabledColourId, Style::border());

    setColour(juce::PopupMenu::backgroundColourId, Style::surface());
    setColour(juce::PopupMenu::textColourId, Style::text());
    setColour(juce::PopupMenu::headerTextColourId, Style::text());
    setColour(juce::PopupMenu::highlightedTextColourId, Style::background());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Style::accent());

    setColour(juce::TooltipWindow::backgroundColourId, Style::surfaceRaised());
    setColour(juce::TooltipWindow::textColourId, Style::text());
    setColour(juce::TooltipWindow::outlineColourId, Style::border());
}

juce::Font AppLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    float size = juce::jmin(14.0f, static_cast<float>(buttonHeight) * 0.58f);
    if (button.getWidth() > 0 && button.getWidth() < 40)
        size = juce::jmin(size, 11.0f);
    return juce::Font(juce::FontOptions(size, juce::Font::plain));
}

void AppLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = backgroundColour.isTransparent() ? Style::surfaceRaised() : backgroundColour;

    if (button.getToggleState())
        fill = Style::accent();
    else
        fill = fill.interpolatedWith(Style::surfaceRaised(), 0.18f);

    if (shouldDrawButtonAsDown)
        fill = fill.interpolatedWith(Style::accent(), 0.16f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.interpolatedWith(Style::accentStrong(), 0.08f);

    fill = fill.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.55f);

    auto outline = button.getToggleState() ? Style::accentStrong() : Style::border();
    if (shouldDrawButtonAsDown)
        outline = outline.brighter(0.12f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(outline.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));
    g.drawRoundedRectangle(bounds, 5.0f, button.getToggleState() ? 1.6f : 1.0f);
}

void AppLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    auto color = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                          : juce::TextButton::textColourOffId)
                    .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.55f);
    g.setColour(color);

    auto padding = (button.getWidth() < 40) ? 0 : juce::jmin(10, button.getWidth() / 6);
    auto textArea = button.getLocalBounds().reduced(padding, 0);

    if (button.getProperties().contains("isSaveButton"))
    {
        auto size = juce::jmin(textArea.getWidth(), textArea.getHeight()) * 0.72f;
        auto r = textArea.toFloat().withSizeKeepingCentre(size, size);

        juce::Path p;
        const float b = r.getWidth() * 0.22f;
        p.startNewSubPath(r.getX(), r.getBottom());
        p.lineTo(r.getX(), r.getY());
        p.lineTo(r.getRight() - b, r.getY());
        p.lineTo(r.getRight(), r.getY() + b);
        p.lineTo(r.getRight(), r.getBottom());
        p.closeSubPath();
        g.fillPath(p);

        g.setColour(button.findColour(juce::TextButton::buttonColourId));
        
        // Shutter area (top)
        auto s = juce::Rectangle<float>(r.getX() + r.getWidth() * 0.2f, r.getY() + r.getHeight() * 0.06f, 
                                        r.getWidth() * 0.52f, r.getHeight() * 0.35f);
        g.fillRect(s);
        
        // Label area (bottom)
        g.fillRect(r.getX() + r.getWidth() * 0.15f, r.getBottom() - r.getHeight() * 0.46f, 
                   r.getWidth() * 0.7f, r.getHeight() * 0.42f);
        
        // Shutter sliding piece
        g.setColour(color);
        g.fillRect(s.getX() + s.getWidth() * 0.65f, s.getY() + s.getHeight() * 0.15f, 
                   s.getWidth() * 0.2f, s.getHeight() * 0.65f);
        
        return;
    }

    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);
    
    // For small buttons or icons, we use a wider virtual area to prevent JUCE from 
    // adding ellipses, then rely on the graphics context's clipping.
    if (button.getButtonText().length() < 8 || button.getWidth() < 40)
    {
        auto largeArea = textArea.expanded(20, 0);
        g.drawText(button.getButtonText(), largeArea, juce::Justification::centred, false);
    }
    else
    {
        g.drawFittedText(button.getButtonText(), textArea, juce::Justification::centred, 1);
    }
}

void AppLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    g.setColour(Style::surface());
    g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 4.0f);
}

void AppLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    auto outline = editor.hasKeyboardFocus(true) ? Style::accent() : Style::border();
    g.setColour(outline.withMultipliedAlpha(editor.isEnabled() ? 1.0f : 0.5f));
    g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f, 4.0f, 1.0f);
}

void AppLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                  int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    auto fill = box.findColour(juce::ComboBox::backgroundColourId);
    if (isButtonDown)
        fill = fill.interpolatedWith(Style::accent(), 0.12f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    if (box.getProperties().contains("hideArrow"))
        return;

    g.setColour(Style::border().withAlpha(0.8f));
    g.drawLine(static_cast<float>(buttonX), 3.0f, static_cast<float>(buttonX), static_cast<float>(height - 3), 1.0f);

    juce::Path arrow;
    auto arrowArea = juce::Rectangle<float>(static_cast<float>(buttonX), static_cast<float>(buttonY),
                                            static_cast<float>(buttonW), static_cast<float>(buttonH)).reduced(6.0f, 5.0f);
    arrow.addTriangle(arrowArea.getX(), arrowArea.getY(),
                      arrowArea.getRight(), arrowArea.getY(),
                      arrowArea.getCentreX(), arrowArea.getBottom());

    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

juce::Font AppLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(13.0f, juce::Font::plain));
}

void AppLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    auto buttonWidth = box.getProperties().contains("hideArrow") ? 0 : juce::jmax(22, box.getHeight());
    auto textWidth = juce::jmax(0, box.getWidth() - buttonWidth);
    label.setBounds(juce::Rectangle<int>(4, 1, textWidth, box.getHeight() - 2));
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void AppLookAndFeel::drawTabButton(juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown)
{
    auto area = button.getActiveArea().toFloat().reduced(1.0f);
    auto fill = button.getTabBackgroundColour();

    if (fill == juce::Colours::transparentBlack)
        fill = button.isFrontTab() ? Style::accent() : Style::surfaceRaised();

    if (button.isFrontTab())
        fill = fill.withMultipliedBrightness(1.12f);
    else
        fill = fill.withMultipliedBrightness(1.28f);

    if (isMouseDown)
        fill = fill.interpolatedWith(Style::accent(), 0.10f);
    else if (isMouseOver)
        fill = fill.interpolatedWith(Style::accentStrong(), 0.10f);

    g.setColour(fill);
    g.fillRoundedRectangle(area, 6.0f);

    auto outline = button.isFrontTab() ? Style::accentStrong() : fill.contrasting(0.25f);
    g.setColour(outline);
    g.drawRoundedRectangle(area, 6.0f, button.isFrontTab() ? 1.8f : 1.4f);

    if (!button.isFrontTab())
    {
        g.setColour(button.getTabBackgroundColour().withAlpha(0.9f));
        g.fillRect(area.getX() + 2.0f, area.getY() + 2.0f, area.getWidth() - 4.0f, 3.0f);
        g.setColour(Style::border().withAlpha(0.85f));
        g.drawLine(area.getX() + 1.0f, area.getBottom() - 1.0f, area.getRight() - 1.0f, area.getBottom() - 1.0f, 1.0f);
    }

    g.setColour(Style::text());
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::plain)));
    g.drawFittedText(button.getButtonText(), button.getTextArea().reduced(8, 0),
                     juce::Justification::centred, 1);
}

void AppLookAndFeel::drawTabAreaBehindFrontButton(juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h)
{
    g.setColour(Style::surface());
    g.fillRect(0, 0, w, h);

    g.setColour(Style::border());
    if (bar.isVertical())
        g.fillRect(bar.getOrientation() == juce::TabbedButtonBar::TabsAtLeft ? w - 1 : 0, 0, 1, h);
    else
        g.fillRect(0, h - 1, w, 1);
}

} // namespace ecm
