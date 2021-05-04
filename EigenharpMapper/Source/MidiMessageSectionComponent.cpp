#include <JuceHeader.h>
#include "MidiMessageSectionComponent.h"

MidiMessageSectionComponent::MidiMessageSectionComponent() : cmdKeyTypeRadioGroup("keyCmdType", "Key cmd type"), midiMsgTypeRadioGroup("midiMsgType", "Message type") {
    addAndMakeVisible(cmdKeyTypeRadioGroup);
    
    cmdKeyTypeLatch.setButtonText("Latch");
    cmdKeyTypeLatch.setRadioGroupId(1);
    cmdKeyTypeLatch.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeLatch);

    cmdKeyTypeMomentary.setButtonText("Momentary");
    cmdKeyTypeMomentary.setRadioGroupId(1);
    cmdKeyTypeMomentary.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeMomentary);

    cmdKeyTypeTrigger.setButtonText("Trigger");
    cmdKeyTypeTrigger.setRadioGroupId(1);
    cmdKeyTypeTrigger.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeTrigger);

    addAndMakeVisible(midiMsgTypeRadioGroup);
    
    midiMsgTypeCC.setButtonText("Control change");
    midiMsgTypeCC.setRadioGroupId(2);
    midiMsgTypeCC.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeCC);

    midiMsgTypeProgChange.setButtonText("Program change");
    midiMsgTypeProgChange.setRadioGroupId(2);
    midiMsgTypeProgChange.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeProgChange);

    midiMsgTypeTransport.setButtonText("Transport");
    midiMsgTypeTransport.setRadioGroupId(2);
    midiMsgTypeTransport.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeTransport);

    midiMsgTypeAllNotesOff.setButtonText("All notes off");
    midiMsgTypeAllNotesOff.setRadioGroupId(2);
    midiMsgTypeAllNotesOff.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeAllNotesOff);
}

MidiMessageSectionComponent::~MidiMessageSectionComponent() {
}

void MidiMessageSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = area.getHeight()*0.05;

    auto groupArea = area.removeFromTop(lineHeight*6);
    cmdKeyTypeRadioGroup.setBounds(groupArea);
    groupArea.reduce(groupArea.getWidth()*0.1, lineHeight);
    groupArea.removeFromTop(lineHeight);
    cmdKeyTypeLatch.setBounds(groupArea.removeFromTop(lineHeight));
    cmdKeyTypeMomentary.setBounds(groupArea.removeFromTop(lineHeight));
    cmdKeyTypeTrigger.setBounds(groupArea.removeFromTop(lineHeight));
    
    groupArea = area.removeFromTop(lineHeight*7);
    midiMsgTypeRadioGroup.setBounds(groupArea);
    groupArea.reduce(groupArea.getWidth()*0.1, lineHeight);
    groupArea.removeFromTop(lineHeight);
    midiMsgTypeCC.setBounds(groupArea.removeFromTop(lineHeight));
    midiMsgTypeProgChange.setBounds(groupArea.removeFromTop(lineHeight));
    midiMsgTypeTransport.setBounds(groupArea.removeFromTop(lineHeight));
    midiMsgTypeAllNotesOff.setBounds(groupArea.removeFromTop(lineHeight));
}

