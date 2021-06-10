#include <JuceHeader.h>
#include "MidiMessageSectionComponent.h"

MidiMessageSectionComponent::MidiMessageSectionComponent() :
        cmdKeyTypeRadioGroup("keyCmdType", "Key cmd type"),
        midiMsgTypeRadioGroup("midiMsgType", "Message type"),
        midiCmdValue("CC #", 3, 0, 127, false),
        offValue("Off val:", 3, 0, 127, false),
        onValue("On val:", 3, 0, 127, false),
        realtimeMsgGroup("realtimeMsgTyp", "Sys Realtime") {
    addAndMakeVisible(cmdKeyTypeRadioGroup);
    
    cmdKeyTypeLatch.setButtonText("Latch");
    cmdKeyTypeLatch.setRadioGroupId(1);
    cmdKeyTypeLatch.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeLatch);
    cmdKeyTypeLatch.onClick = [this] {
        sendChangeMessage();
    };

    cmdKeyTypeMomentary.setButtonText("Momentary");
    cmdKeyTypeMomentary.setRadioGroupId(1);
    cmdKeyTypeMomentary.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeMomentary);
    cmdKeyTypeMomentary.onClick = [this] {
        sendChangeMessage();
    };

    cmdKeyTypeTrigger.setButtonText("Trigger");
    cmdKeyTypeTrigger.setRadioGroupId(1);
    cmdKeyTypeTrigger.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(cmdKeyTypeTrigger);
    cmdKeyTypeTrigger.onClick = [this] {
        sendChangeMessage();
    };

    addAndMakeVisible(midiMsgTypeRadioGroup);
    
    midiMsgTypeCC.setButtonText("Ctrl change");
    midiMsgTypeCC.setRadioGroupId(2);
    midiMsgTypeCC.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeCC);
    midiMsgTypeCC.onClick = [this] {
        sendChangeMessage();
    };

    midiMsgTypeProgChange.setButtonText("Prog change");
    midiMsgTypeProgChange.setRadioGroupId(2);
    midiMsgTypeProgChange.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeProgChange);
    midiMsgTypeProgChange.onClick = [this] {
        sendChangeMessage();
    };

    midiMsgTypeRealtime.setButtonText("Realtime");
    midiMsgTypeRealtime.setRadioGroupId(2);
    midiMsgTypeRealtime.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeRealtime);
    midiMsgTypeRealtime.onClick = [this] {
        sendChangeMessage();
    };

    midiMsgTypeAllNotesOff.setButtonText("All notes off");
    midiMsgTypeAllNotesOff.setRadioGroupId(2);
    midiMsgTypeAllNotesOff.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeAllNotesOff);
    midiMsgTypeAllNotesOff.onClick = [this] {
        sendChangeMessage();
    };

    addAndMakeVisible(realtimeMsgGroup);
    realtimeOn.setLabelText("On value", true);
    realtimeOff.setLabelText("Off value", true);
    realtimeOn.addItem("None", -1);
    realtimeOn.addItem("Start", 1);
    realtimeOn.addItem("Stop", 2);
    realtimeOn.addItem("Continue", 3);
    realtimeOff.addItem("None", -1);
    realtimeOff.addItem("Start", 1);
    realtimeOff.addItem("Stop", 2);
    realtimeOff.addItem("Continue", 3);
    addAndMakeVisible(realtimeOn);
    addAndMakeVisible(realtimeOff);
    realtimeOn.box.onChange = [this] {
        sendChangeMessage();
    };
    realtimeOff.box.onChange = [this] {
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
    float lineHeight = area.getHeight()*0.04;

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
    midiMsgTypeRealtime.setBounds(groupArea.removeFromTop(lineHeight));
    midiMsgTypeAllNotesOff.setBounds(groupArea.removeFromTop(lineHeight));
    
    area.removeFromTop(lineHeight*1);

    midiCmdValue.setBounds(area.removeFromTop(lineHeight));
    onValue.setBounds(area.removeFromTop(lineHeight));
    offValue.setBounds(area.removeFromTop(lineHeight));

    area.removeFromTop(lineHeight*1);

    groupArea = area.removeFromTop(lineHeight*6);
    realtimeMsgGroup.setBounds(groupArea);
    groupArea.reduce(groupArea.getWidth()*0.1, lineHeight);
    realtimeOn.setBounds(groupArea.removeFromTop(lineHeight*2));
    realtimeOff.setBounds(groupArea.removeFromTop(lineHeight*2));
}

juce::String MidiMessageSectionComponent::getMessageString() {
    juce::String cmdType;
    juce::String msgType;
    juce::String onVal;
    juce::String offVal;

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
    else if (midiMsgTypeRealtime.getToggleState())
        msgType = "Realtime";
    else if (midiMsgTypeAllNotesOff.getToggleState())
        msgType = "AllNotesOff";
    
    if (msgType == "Realtime") {
        onVal = juce::String(realtimeOn.box.getSelectedId());
        offVal = juce::String(realtimeOff.box.getSelectedId());
    }
    else {
        onVal = juce::String(onValue.getValue());
        offVal = juce::String(offValue.getValue());
    }

    // cmdType;msgType;CC;off;on
    return cmdType + ";" +
        msgType + ";" +
        juce::String(midiCmdValue.getValue()) + ";" +
        offVal  + ";" +
        onVal;
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
    midiMsgTypeRealtime.setToggleState(tokens[1] == "Realtime", juce::dontSendNotification);
    midiMsgTypeAllNotesOff.setToggleState(tokens[1] == "AllNotesOff", juce::dontSendNotification);
    
    if (tokens[1] == "Realtime") {
        realtimeOn.setSelectedItemId(tokens[4].getIntValue());
        realtimeOff.setSelectedItemId(tokens[3].getIntValue());
    }
    
    midiCmdValue.setValue(tokens[2].getIntValue());
    offValue.setValue(tokens[1] == "Realtime" ? 0 : tokens[3].getIntValue());
    onValue.setValue(tokens[1] == "Realtime" ? 0 : tokens[4].getIntValue());
    
    offValue.setEnabled(tokens[0] != "Trigger" && tokens[1] != "Realtime" && tokens[1] != "AllNotesOff");
    onValue.setEnabled(tokens[1] != "Realtime" && tokens[1] != "AllNotesOff");
    midiCmdValue.setEnabled(tokens[1] == "CC");
    realtimeMsgGroup.setEnabled(tokens[1] == "Realtime");
    realtimeOn.setEnabled(tokens[1] == "Realtime");
    realtimeOff.setEnabled(tokens[1] == "Realtime" && tokens[0] != "Trigger");
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
