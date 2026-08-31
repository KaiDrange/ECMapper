#pragma once

#include <JuceHeader.h>

class AboutDialogComponent final : public juce::Component
{
public:
    static constexpr int dialogWidth = 520;
    static constexpr int dialogHeight = 356;
    static constexpr int outerMargin = 16;
    static constexpr int rowGap = 10;
    static constexpr int buttonHeight = 28;
    static inline const juce::String sourceCodeUrl{"https://github.com/KaiDrange/ECMapper"};
    static inline const juce::String releasesUrl{"https://github.com/KaiDrange/ECMapper/releases"};
    static inline const juce::String onlineManualUrl{"https://ticticelectro.com/ECMapper"};
    static inline const juce::String ourMusicUrl{"https://ticticelectro.com/our-music/"};
    static inline const juce::String eigenLiteUrl{"https://github.com/thetechnobear/EigenLite"};
    static inline const juce::String juceUrl{"https://juce.com/"};


    AboutDialogComponent(const juce::String& appName,
                         const juce::String& versionString);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void closeDialog() const;

    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::TextEditor descriptionBox;
    juce::Label linksLabel;
    juce::HyperlinkButton sourceButton;
    juce::HyperlinkButton releasesButton;
    juce::HyperlinkButton juceButton;
    juce::HyperlinkButton eigenLiteButton;
    juce::TextButton closeButton{"Close"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutDialogComponent)
};
