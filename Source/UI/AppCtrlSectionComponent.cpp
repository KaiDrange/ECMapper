#include "AppCtrlSectionComponent.h"
#include "AppStyle.h"

namespace ecm {

AppCtrlSectionComponent::AppCtrlSectionComponent() :
    presetNumber("Preset #", 2, 1, 32, false),
    transposeSemitones("Semitones", 3, -96, 96, false)
{
    typeRadioGroup.setText("App Ctrl Type");
    addAndMakeVisible(typeRadioGroup);

    typePreset.setButtonText("Preset Switch");
    typePreset.setRadioGroupId(100);
    typePreset.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(typePreset);
    typePreset.onClick = [this] { 
        presetNumber.setVisible(true);
        transposeSemitones.setVisible(false);
        modeRadioGroup.setVisible(false);
        modeLatch.setVisible(false);
        modeMomentary.setVisible(false);
        modeTrigger.setVisible(false);
        resized();
        sendChangeMessage(); 
    };

    typeTranspose.setButtonText("Transpose");
    typeTranspose.setRadioGroupId(100);
    typeTranspose.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(typeTranspose);
    typeTranspose.onClick = [this] { 
        presetNumber.setVisible(false);
        transposeSemitones.setVisible(true);
        modeRadioGroup.setVisible(true);
        modeLatch.setVisible(true);
        modeMomentary.setVisible(true);
        modeTrigger.setVisible(true);
        resized();
        sendChangeMessage(); 
    };

    modeRadioGroup.setText("Mode");
    addAndMakeVisible(modeRadioGroup);

    modeLatch.setButtonText("Latch");
    modeLatch.setRadioGroupId(101);
    modeLatch.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(modeLatch);
    modeLatch.onClick = [this] { sendChangeMessage(); };

    modeMomentary.setButtonText("Momentary");
    modeMomentary.setRadioGroupId(101);
    modeMomentary.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(modeMomentary);
    modeMomentary.onClick = [this] { sendChangeMessage(); };

    modeTrigger.setButtonText("Trigger");
    modeTrigger.setRadioGroupId(101);
    modeTrigger.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(modeTrigger);
    modeTrigger.onClick = [this] { sendChangeMessage(); };

    addAndMakeVisible(presetNumber);
    presetNumber.addListener(this);
    addAndMakeVisible(transposeSemitones);
    transposeSemitones.addListener(this);

    presetNumber.setVisible(true);
    transposeSemitones.setVisible(false);
    
    modeRadioGroup.setVisible(false);
    modeLatch.setVisible(false);
    modeMomentary.setVisible(false);
    modeTrigger.setVisible(false);
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
    auto inputArea = area.removeFromTop(static_cast<int>(lineHeight));
    if (presetNumber.isVisible()) presetNumber.setBounds(inputArea);
    if (transposeSemitones.isVisible()) transposeSemitones.setBounds(inputArea);
    
    if (modeRadioGroup.isVisible())
    {
        area.removeFromTop(static_cast<int>(lineHeight));
        auto modeArea = area.removeFromTop(static_cast<int>(lineHeight * 6));
        modeRadioGroup.setBounds(modeArea);
        modeArea.reduce(static_cast<int>(modeArea.getWidth() * 0.1f), static_cast<int>(lineHeight));
        modeArea.removeFromTop(static_cast<int>(lineHeight));
        modeLatch.setBounds(modeArea.removeFromTop(static_cast<int>(lineHeight)));
        modeMomentary.setBounds(modeArea.removeFromTop(static_cast<int>(lineHeight)));
        modeTrigger.setBounds(modeArea.removeFromTop(static_cast<int>(lineHeight)));
    }
}

juce::String AppCtrlSectionComponent::getMessageString() {
    if (typePreset.getToggleState()) {
        return "Preset;" + juce::String(presetNumber.getValue());
    } else {
        juce::String mode = "Latch";
        if (modeMomentary.getToggleState()) mode = "Momentary";
        else if (modeTrigger.getToggleState()) mode = "Trigger";
        return "Transpose;" + mode + ";" + juce::String(transposeSemitones.getValue());
    }
}

void AppCtrlSectionComponent::updatePanelFromMessageString(const juce::String& msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    
    if (tokens.size() < 2) {
        typePreset.setToggleState(true, juce::dontSendNotification);
        typeTranspose.setToggleState(false, juce::dontSendNotification);
        presetNumber.setValue(1);
        transposeSemitones.setValue(0);
        presetNumber.setVisible(true);
        transposeSemitones.setVisible(false);
        modeRadioGroup.setVisible(false);
        return;
    }

    bool isPreset = tokens[0] == "Preset";
    typePreset.setToggleState(isPreset, juce::dontSendNotification);
    typeTranspose.setToggleState(!isPreset, juce::dontSendNotification);
    
    if (isPreset) {
        presetNumber.setValue(tokens[1].getIntValue());
        transposeSemitones.setValue(0);
        modeRadioGroup.setVisible(false);
        modeLatch.setVisible(false);
        modeMomentary.setVisible(false);
        modeTrigger.setVisible(false);
    } else {
        transposeSemitones.setValue(tokens.size() == 3 ? tokens[2].getIntValue() : tokens[1].getIntValue());
        
        juce::String mode = tokens.size() == 3 ? tokens[1] : "Latch";
        modeLatch.setToggleState(mode == "Latch", juce::dontSendNotification);
        modeMomentary.setToggleState(mode == "Momentary", juce::dontSendNotification);
        modeTrigger.setToggleState(mode == "Trigger", juce::dontSendNotification);
        
        presetNumber.setValue(1);
        modeRadioGroup.setVisible(true);
        modeLatch.setVisible(true);
        modeMomentary.setVisible(true);
        modeTrigger.setVisible(true);
    }

    presetNumber.setVisible(isPreset);
    transposeSemitones.setVisible(!isPreset);
    resized();
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
