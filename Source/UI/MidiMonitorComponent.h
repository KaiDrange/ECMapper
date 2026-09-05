#pragma once
#include <JuceHeader.h>
#include "../Core/MidiMonitor.h"

namespace ecm {

class MidiMonitorComponent : public juce::Component, private juce::Timer {
public:
    MidiMonitorComponent() {
        addAndMakeVisible(textEditor);
        textEditor.setReadOnly(true);
        textEditor.setMultiLine(true);
        textEditor.setReturnKeyStartsNewLine(true);
        textEditor.setScrollbarsShown(true);
        textEditor.setCaretVisible(false);
        textEditor.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));

        addAndMakeVisible(clearButton);
        clearButton.onClick = [this] { 
            MidiMonitor::getInstance().clear(); 
            textEditor.clear();
        };

        addAndMakeVisible(autoScrollToggle);
        autoScrollToggle.setButtonText("Auto-scroll");
        autoScrollToggle.setToggleState(true, juce::dontSendNotification);

        startTimer(100);
        updateLog();
    }

    void resized() override {
        auto r = getLocalBounds().reduced(10);
        auto footer = r.removeFromBottom(30);
        clearButton.setBounds(footer.removeFromRight(80));
        footer.removeFromRight(10);
        autoScrollToggle.setBounds(footer.removeFromRight(100));
        textEditor.setBounds(r);
    }

private:
    void timerCallback() override {
        if (MidiMonitor::getInstance().checkAndResetNewMessages())
            updateLog();
    }

    void updateLog() {
        auto messages = MidiMonitor::getInstance().getMessages();
        textEditor.setText(messages.joinIntoString("\n"), false);
        if (autoScrollToggle.getToggleState())
            textEditor.moveCaretToEnd();
    }

    juce::TextEditor textEditor;
    juce::TextButton clearButton{ "Clear" };
    juce::ToggleButton autoScrollToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMonitorComponent)
};

}
