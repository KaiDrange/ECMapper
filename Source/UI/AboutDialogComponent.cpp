#include "AboutDialogComponent.h"
#include "AppStyle.h"

AboutDialogComponent::AboutDialogComponent(const juce::String& appName,
                                           const juce::String& versionString)
    : sourceButton("Source code", juce::URL(sourceCodeUrl)),
      releasesButton("Releases", juce::URL(releasesUrl)),
      eigenLiteButton("EigenLite", juce::URL(eigenLiteUrl)),
      juceButton("JUCE", juce::URL(juceUrl))
{
    setSize(dialogWidth, dialogHeight);

    titleLabel.setText(appName, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);

    versionLabel.setText("Version " + versionString, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredLeft);

    descriptionBox.setMultiLine(true);
    descriptionBox.setReturnKeyStartsNewLine(true);
    descriptionBox.setReadOnly(true);
    descriptionBox.setPopupMenuEnabled(false);
    descriptionBox.setCaretVisible(false);
    descriptionBox.setText("ECMapper converts high resolution Eigenharp data to MIDI. "
                           "It is written by Kai Drange and is an open source project released under the "
                           "MIT license. Feel free to reach out to the author at community.polyexpression.com "
                           "if you have any questions or suggestions.");
    descriptionBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    descriptionBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    descriptionBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    descriptionBox.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

    linksLabel.setText("Links", juce::dontSendNotification);
    linksLabel.setJustificationType(juce::Justification::centredLeft);

    closeButton.onClick = [this]
    {
        closeDialog();
    };

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(versionLabel);
    addAndMakeVisible(descriptionBox);
    addAndMakeVisible(linksLabel);
    addAndMakeVisible(sourceButton);
    addAndMakeVisible(releasesButton);
    addAndMakeVisible(eigenLiteButton);
    addAndMakeVisible(juceButton);
    addAndMakeVisible(closeButton);
}

void AboutDialogComponent::resized()
{
    auto area = getLocalBounds().reduced(outerMargin);

    titleLabel.setBounds(area.removeFromTop(24));
    versionLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(rowGap);

    descriptionBox.setBounds(area.removeFromTop(96));
    area.removeFromTop(rowGap);

    linksLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);

    sourceButton.setBounds(area.removeFromTop(buttonHeight));
    area.removeFromTop(4);
    releasesButton.setBounds(area.removeFromTop(buttonHeight));
    area.removeFromTop(4);
    eigenLiteButton.setBounds(area.removeFromTop(buttonHeight));
    area.removeFromTop(rowGap);
    juceButton.setBounds(area.removeFromTop(buttonHeight));
    area.removeFromTop(rowGap);

    auto footerArea = area.removeFromBottom(buttonHeight);
    closeButton.setBounds(footerArea.removeFromRight(100));
}

void AboutDialogComponent::paint(juce::Graphics& g)
{
    g.fillAll(ecm::Style::background());
    Component::paint(g);
}

void AboutDialogComponent::closeDialog() const
{
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState(0);
}
