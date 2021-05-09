#include <JuceHeader.h>
#include "MidiMessageSectionComponent.h"

MidiMessageSectionComponent::MidiMessageSectionComponent() :
        cmdKeyTypeRadioGroup("keyCmdType", "Key cmd type"),
        midiMsgTypeRadioGroup("midiMsgType", "Message type"),
        midiCmdValue("CC #", 3, 0, 127, 0),
        offValue("Off val:", 3, 0, 127, 0),
        onValue("On val:", 3, 0, 127, 127) {
    addAndMakeVisible(cmdKeyTypeRadioGroup);
    
    cmdKeyTypeLatch.setButtonText("Latch");
    cmdKeyTypeLatch.setRadioGroupId(1);
    cmdKeyTypeLatch.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeLatch);
    cmdKeyTypeLatch.onStateChange = [this] {
        sendChangeMessage();
    };

    cmdKeyTypeMomentary.setButtonText("Momentary");
    cmdKeyTypeMomentary.setRadioGroupId(1);
    cmdKeyTypeMomentary.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeMomentary);
    cmdKeyTypeMomentary.onStateChange = [this] {
        sendChangeMessage();
    };

    cmdKeyTypeTrigger.setButtonText("Trigger");
    cmdKeyTypeTrigger.setRadioGroupId(1);
    cmdKeyTypeTrigger.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeTrigger);
    cmdKeyTypeTrigger.onStateChange = [this] {
        sendChangeMessage();
    };

    addAndMakeVisible(midiMsgTypeRadioGroup);
    
    midiMsgTypeCC.setButtonText("Control change");
    midiMsgTypeCC.setRadioGroupId(2);
    midiMsgTypeCC.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeCC);
    midiMsgTypeCC.onStateChange = [this] {
        sendChangeMessage();
    };

    midiMsgTypeProgChange.setButtonText("Program change");
    midiMsgTypeProgChange.setRadioGroupId(2);
    midiMsgTypeProgChange.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeProgChange);
    midiMsgTypeProgChange.onStateChange = [this] {
        sendChangeMessage();
    };

    midiMsgTypeTransport.setButtonText("Transport");
    midiMsgTypeTransport.setRadioGroupId(2);
    midiMsgTypeTransport.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeTransport);
    midiMsgTypeTransport.onStateChange = [this] {
        sendChangeMessage();
    };

    midiMsgTypeAllNotesOff.setButtonText("All notes off");
    midiMsgTypeAllNotesOff.setRadioGroupId(2);
    midiMsgTypeAllNotesOff.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeAllNotesOff);
    midiMsgTypeAllNotesOff.onStateChange = [this] {
        sendChangeMessage();
    };

    addAndMakeVisible(midiCmdValue);
    midiCmdValue.addListener(this);
    addAndMakeVisible(offValue);
    offValue.addListener(this);
    addAndMakeVisible(onValue);
    onValue.addListener(this);
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
    
    midiCmdValue.setBounds(area.removeFromTop(lineHeight));
    offValue.setBounds(area.removeFromTop(lineHeight));
    onValue.setBounds(area.removeFromTop(lineHeight));
}

juce::String MidiMessageSectionComponent::getMessageString() {
    juce::String cmdType;
    juce::String msgType;

    if (cmdKeyTypeLatch.getToggleState())
        cmdType = "Latch";
    else if (cmdKeyTypeMomentary.getToggleState())
        cmdType = "Momentary";
    else if (cmdKeyTypeTrigger.getToggleState())
        cmdType = "Trigger";
    
    if (midiMsgTypeCC.getToggleState())
        msgType = "CC";
    else if (midiMsgTypeProgChange.getToggleState())
        msgType = "PC";
    else if (midiMsgTypeTransport.getToggleState())
        msgType = "Transport";
    else if (midiMsgTypeAllNotesOff.getToggleState())
        msgType = "AllNotesOff";
    
    
    return cmdType + ";" +
        msgType + ";" +
        juce::String(midiCmdValue.getValue()) + ";" +
        juce::String(offValue.getValue())  + ";" +
        juce::String(onValue.getValue());
}

void MidiMessageSectionComponent::updatePanelFromMessageString(juce::String msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    if (tokens.size() != 5)
        return;
    
    cmdKeyTypeLatch.setToggleState(tokens[0] == "Latch", juce::dontSendNotification);
    cmdKeyTypeMomentary.setToggleState(tokens[0] == "Momentary", juce::dontSendNotification);
    cmdKeyTypeTrigger.setToggleState(tokens[0] == "Trigger", juce::dontSendNotification);
    
    midiMsgTypeCC.setToggleState(tokens[1] == "CC", juce::dontSendNotification);
    midiMsgTypeProgChange.setToggleState(tokens[1] == "PC", juce::dontSendNotification);
    midiMsgTypeTransport.setToggleState(tokens[1] == "Transport", juce::dontSendNotification);
    midiMsgTypeAllNotesOff.setToggleState(tokens[1] == "midiMsgTypeAllNotesOff", juce::dontSendNotification);
    
    midiCmdValue.setValue(tokens[2].getIntValue());
    offValue.setValue(tokens[3].getIntValue());
    onValue.setValue(tokens[4].getIntValue());
}

void MidiMessageSectionComponent::addListener(Listener* listenerToAdd) {
    listeners.add(listenerToAdd);
}

void MidiMessageSectionComponent::removeListener(Listener* listenerToRemove) {
    jassert(listeners.contains(listenerToRemove));
    listeners.remove(listenerToRemove);
}

void MidiMessageSectionComponent::sendChangeMessage() {
    listeners.call([this](Listener& l) { l.valuesChanged(this); });
}

void MidiMessageSectionComponent::numberInputChanged(NumberInputComponent*) {
    sendChangeMessage();
}

