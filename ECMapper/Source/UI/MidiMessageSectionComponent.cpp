#include <JuceHeader.h>
#include "MidiMessageSectionComponent.h"

MidiMessageSectionComponent::MidiMessageSectionComponent() :
        cmdKeyTypeRadioGroup("keyCmdType", "Key cmd type"),
        midiMsgTypeRadioGroup("midiMsgType", "Message type"),
        midiCmdValue("CC #", 3, 0, 127, false),
        offValue("Off val:", 3, 0, 127, false),
        onValue("On val:", 3, 0, 127, false),
        realtimeMsgTypeRadioGroup("realtimeMsgTyp", "Sys Realtime") {
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
    
    midiMsgTypeCC.setButtonText("Ctrl change");
    midiMsgTypeCC.setRadioGroupId(2);
    midiMsgTypeCC.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeCC);
    midiMsgTypeCC.onStateChange = [this] {
        sendChangeMessage();
    };

    midiMsgTypeProgChange.setButtonText("Prog change");
    midiMsgTypeProgChange.setRadioGroupId(2);
    midiMsgTypeProgChange.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeProgChange);
    midiMsgTypeProgChange.onStateChange = [this] {
        sendChangeMessage();
    };

    midiMsgTypeRealtime.setButtonText("Realtime");
    midiMsgTypeRealtime.setRadioGroupId(2);
    midiMsgTypeRealtime.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeRealtime);
    midiMsgTypeRealtime.onStateChange = [this] {
        sendChangeMessage();
    };

    midiMsgTypeAllNotesOff.setButtonText("All notes off");
    midiMsgTypeAllNotesOff.setRadioGroupId(2);
    midiMsgTypeAllNotesOff.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(midiMsgTypeAllNotesOff);
    midiMsgTypeAllNotesOff.onStateChange = [this] {
        sendChangeMessage();
    };

    addAndMakeVisible(realtimeMsgTypeRadioGroup);
    realtimeStart.setButtonText("Start");
    realtimeStart.setRadioGroupId(3);
    realtimeStart.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(realtimeStart);
    realtimeStart.onStateChange = [this] {
        sendChangeMessage();
    };

    realtimeStop.setButtonText("Stop");
    realtimeStop.setRadioGroupId(3);
    realtimeStop.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(realtimeStop);
    realtimeStop.onStateChange = [this] {
        sendChangeMessage();
    };

    realtimeContinue.setButtonText("Continue");
    realtimeContinue.setRadioGroupId(3);
    realtimeContinue.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(realtimeContinue);
    realtimeContinue.onStateChange = [this] {
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
    offValue.setBounds(area.removeFromTop(lineHeight));
    onValue.setBounds(area.removeFromTop(lineHeight));

    area.removeFromTop(lineHeight*1);

    groupArea = area.removeFromTop(lineHeight*6);
    realtimeMsgTypeRadioGroup.setBounds(groupArea);
    groupArea.reduce(groupArea.getWidth()*0.1, lineHeight);
    groupArea.removeFromTop(lineHeight);
    realtimeStart.setBounds(groupArea.removeFromTop(lineHeight));
    realtimeStop.setBounds(groupArea.removeFromTop(lineHeight));
    realtimeContinue.setBounds(groupArea.removeFromTop(lineHeight));
}

juce::String MidiMessageSectionComponent::getMessageString() {
    juce::String cmdType;
    juce::String msgType;
    juce::String realtimeType;

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
    
    if (realtimeStart.getToggleState())
        realtimeType = "Start";
    else if (realtimeStop.getToggleState())
        realtimeType = "Stop";
    else if (realtimeContinue.getToggleState())
        realtimeType = "Continue";

    // cmdType;msgType;CC;off;on;realtime
    return cmdType + ";" +
        msgType + ";" +
        juce::String(midiCmdValue.getValue()) + ";" +
        juce::String(offValue.getValue())  + ";" +
        juce::String(onValue.getValue()) + ";" +
        realtimeType;
}

void MidiMessageSectionComponent::updatePanelFromMessageString(juce::String msgString) {
    juce::StringArray tokens;
    tokens.addTokens(msgString, ";", "\"");
    if (tokens.size() != 6)
        return;
    
    cmdKeyTypeLatch.setToggleState(tokens[0] == "Latch", juce::dontSendNotification);
    cmdKeyTypeMomentary.setToggleState(tokens[0] == "Momentary", juce::dontSendNotification);
    cmdKeyTypeTrigger.setToggleState(tokens[0] == "Trigger", juce::dontSendNotification);
    
    midiMsgTypeCC.setToggleState(tokens[1] == "CC", juce::dontSendNotification);
    midiMsgTypeProgChange.setToggleState(tokens[1] == "PC", juce::dontSendNotification);
    midiMsgTypeRealtime.setToggleState(tokens[1] == "Realtime", juce::dontSendNotification);
    midiMsgTypeAllNotesOff.setToggleState(tokens[1] == "AllNotesOff", juce::dontSendNotification);
    
    if (tokens[1] == "Realtime") {
        realtimeStart.setToggleState(tokens[5] == "Start", juce::dontSendNotification);
        realtimeStop.setToggleState(tokens[5] == "Stop", juce::dontSendNotification);
        realtimeContinue.setToggleState(tokens[5] == "Continue", juce::dontSendNotification);
    }
    
    midiCmdValue.setValue(tokens[2].getIntValue());
    offValue.setValue(tokens[3].getIntValue());
    onValue.setValue(tokens[4].getIntValue());
    
    offValue.setEnabled(tokens[0] != "Trigger");
    onValue.setEnabled(tokens[1] != "Realtime" && tokens[1] != "AllNotesOff");
    midiCmdValue.setEnabled(tokens[1] == "CC");
    realtimeMsgTypeRadioGroup.setEnabled(tokens[1] == "Realtime");
    realtimeStart.setEnabled(tokens[1] == "Realtime");
    realtimeStop.setEnabled(tokens[1] == "Realtime");
    realtimeContinue.setEnabled(tokens[1] == "Realtime");
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

