#pragma once

#include <JuceHeader.h>

namespace ecm::Style {

juce::Colour background();
juce::Colour surface();
juce::Colour surfaceRaised();
juce::Colour border();
juce::Colour text();
juce::Colour mutedText();
juce::Colour accent();
juce::Colour accentStrong();
juce::Colour warning();
juce::Colour danger();

juce::LookAndFeel_V4::ColourScheme colourScheme();

} // namespace ecm::Style

namespace ecm {

class AppLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    AppLookAndFeel();

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void fillTextEditorBackground(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;
};

} // namespace ecm
