#include "AppCtrlSectionComponent.h"
#include "AppStyle.h"

namespace ecm {

AppCtrlSectionComponent::AppCtrlSectionComponent() :
    presetNumber("Preset #", 2, 1, 32, false),
    transposeSemiTones("Semi-tones", 3, -96, 96, false)
{
    typeRadioGroup.setText("App Ctrl Type");
    addAndMakeVisible(typeRadioGroup);

    typePreset.setButtonText("Preset Switch");
    typePreset.setRadioGroupId(100);
    typePreset.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(typePreset);
    typePreset.onClick = [this] { 
        presetNumber.setEnabled(true);
        transposeSemiTones.setEnabled(false);
        sendChangeMessage(); 
    };

    typeTranspose.setButtonText("Transpose");
    typeTranspose.setRadioGroupId(100);
    typeTranspose.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(typeTranspose);
    typeTranspose.onClick = [this] { 
        presetNumber.setEnabled(false);
        transposeSemiTones.setEnabled(true);
        sendChangeMessage(); 
    };

    addAndMakeVisible(presetNumber);
    presetNumber.addListener(this);
    addAndMakeVisible(transposeSemiTones);
    transposeSemiTones.addListener(this);

    presetNumber.setEnabled(true);
    transposeSemiTones.setEnabled(false);
}

void AppCtrlSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = area.getHeight() * 0.05f;

    auto groupArea = area.removeFromTop(static_cast<int>(lineHeight * 5));
    typeRadioGroup.setBounds(groupArea);
    groupArea.reduce(static_cast<int>(groupArea.getWidth() * 0.1f), static_cast<int>(lineHeight));
    groupArea.removeFromTop(static_cast<int>(lineHeight));
    typePreset.setBounds(groupArea.removeFromTop(static_cast<int>(lineHeight)));
    typeTranspose.setBounds(groupArea.removeFromTop(static_cast<int>(lineHeight)));

    area.removeFromTop(static_cast<int>(lineHeight));
    presetNumber.setBounds(area.removeFromTop(static_cast<int>(lineHeight)));
    transposeSemiTones.setBounds(area.removeFromTop(static_cast<int>(lineHeight)));
}

juce::String AppCtrlSectionComponent::getMessageString() {
    juce::String type = typePreset.getToggleState() ? "Preset" : "Transpose";
    int val = typePreset.getToggleState() ? presetNumber.getValue() : transposeSemiTones.getValue();
    return type + ";" + juce::String(val);
}

void AppCtrlSectionComponent::updatePanelFromMessageString(const juce::String& msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    
    if (tokens.size() != 2) {
        typePreset.setToggleState(true, juce::dontSendNotification);
        typeTranspose.setToggleState(false, juce::dontSendNotification);
        presetNumber.setValue(1);
        transposeSemiTones.setValue(0);
        presetNumber.setEnabled(true);
        transposeSemiTones.setEnabled(false);
        return;
    }

    bool isPreset = tokens[0] == "Preset";
    typePreset.setToggleState(isPreset, juce::dontSendNotification);
    typeTranspose.setToggleState(!isPreset, juce::dontSendNotification);
    
    if (isPreset) {
        presetNumber.setValue(tokens[1].getIntValue());
        transposeSemiTones.setValue(0);
    } else {
        transposeSemiTones.setValue(tokens[1].getIntValue());
        presetNumber.setValue(1);
    }

    presetNumber.setEnabled(isPreset);
    transposeSemiTones.setEnabled(!isPreset);
}

void AppCtrlSectionComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.valuesChanged(this); });
}

void AppCtrlSectionComponent::numberInputChanged(NumberInputComponent*) {
    sendChangeMessage();
}

void AppCtrlSectionComponent::visibilityChanged() {
    if (isVisible()) sendChangeMessage();
}

} // namespace ecm
