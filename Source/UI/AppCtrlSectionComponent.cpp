#include "AppCtrlSectionComponent.h"
#include "AppStyle.h"

namespace ecm {

void AppCtrlSectionComponent::ZoneToggleButton::paintButton(juce::Graphics& g, bool highlighted, bool down) {
    const auto fontSize = juce::jmin(15.0f, static_cast<float>(getHeight()) * 0.75f);
    const auto tickWidth = fontSize * 1.1f;
    getLookAndFeel().drawTickBox(g, *this, 4.0f, (static_cast<float>(getHeight()) - tickWidth) * 0.5f,
                               tickWidth, tickWidth, getToggleState(), isEnabled(), highlighted, down);
    g.setColour(findColour(juce::ToggleButton::textColourId).withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f));
    g.setFont(fontSize);
    // JUCE's default toggle renderer permits wrapping; zone names must stay together.
    g.drawFittedText(getButtonText(), getLocalBounds().withTrimmedLeft(juce::roundToInt(tickWidth) + 10)
                                                   .withTrimmedRight(2),
                     juce::Justification::centredLeft, 1);
}

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
    typePreset.onClick = [this] { updateVisibility(); sendChangeMessage(); };

    typeTranspose.setButtonText("Transpose");
    typeTranspose.setRadioGroupId(100);
    typeTranspose.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(typeTranspose);
    typeTranspose.onClick = [this] { updateVisibility(); sendChangeMessage(); };

    typeTransport.setButtonText("Transport Start/Stop");
    typeTransport.setRadioGroupId(100);
    addAndMakeVisible(typeTransport);
    typeTransport.onClick = [this] {
        if (modeMomentary.getToggleState())
            modeLatch.setToggleState(true, juce::dontSendNotification);
        updateVisibility();
        sendChangeMessage();
    };
    transportAction.addItem("Start", 1);
    transportAction.addItem("Stop", 2);
    transportAction.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(transportAction);
    transportAction.onChange = [this] { updateVisibility(); sendChangeMessage(); };

    modeRadioGroup.setText("Mode");
    addAndMakeVisible(modeRadioGroup);

    modeLatch.setButtonText("Latch");
    modeLatch.setRadioGroupId(101);
    modeLatch.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(modeLatch);
    modeLatch.onClick = [this] { updateVisibility(); sendChangeMessage(); };

    modeMomentary.setButtonText("Momentary");
    modeMomentary.setRadioGroupId(101);
    modeMomentary.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(modeMomentary);
    modeMomentary.onClick = [this] { updateVisibility(); sendChangeMessage(); };

    modeTrigger.setButtonText("Trigger");
    modeTrigger.setRadioGroupId(101);
    modeTrigger.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(modeTrigger);
    modeTrigger.onClick = [this] { updateVisibility(); sendChangeMessage(); };

    addAndMakeVisible(presetNumber);
    presetNumber.addListener(this);
    addAndMakeVisible(transposeSemitones);
    transposeSemitones.addListener(this);

    presetProgramGroup.setText("MIDI prg. Chng.");
    addAndMakeVisible(presetProgramGroup);
    for (size_t zone = 0; zone < presetPrograms.size(); ++zone) {
        auto& enabled = presetProgramEnabled[zone];
        enabled.setButtonText("Zone " + juce::String(static_cast<int>(zone) + 1));
        enabled.setTooltip("Send on this device's current zone output when this preset key is pressed");
        addAndMakeVisible(enabled);
        enabled.onClick = [this] { updateVisibility(); sendChangeMessage(); };
        presetPrograms[zone] = std::make_unique<NumberInputComponent>("", 3, 0, 127, false);
        addAndMakeVisible(*presetPrograms[zone]);
        presetPrograms[zone]->addListener(this);
        presetPrograms[zone]->setEnabled(false);
    }

    presetNumber.setVisible(true);
    transposeSemitones.setVisible(false);
    
    modeRadioGroup.setVisible(false);
    modeLatch.setVisible(false);
    modeMomentary.setVisible(false);
    modeTrigger.setVisible(false);
}

void AppCtrlSectionComponent::resized() {
    auto area = getLocalBounds();
    float lineHeight = static_cast<float>(area.getHeight()) * 0.05f;

    auto groupArea = area.removeFromTop(static_cast<int>(lineHeight * 6));
    typeRadioGroup.setBounds(groupArea);
    groupArea.reduce(static_cast<int>(static_cast<float>(groupArea.getWidth()) * 0.1f), static_cast<int>(lineHeight));
    groupArea.removeFromTop(static_cast<int>(lineHeight));
    typePreset.setBounds(groupArea.removeFromTop(static_cast<int>(lineHeight)));
    typeTranspose.setBounds(groupArea.removeFromTop(static_cast<int>(lineHeight)));

    typeTransport.setBounds(groupArea.removeFromTop(static_cast<int>(lineHeight)));

    area.removeFromTop(static_cast<int>(lineHeight));
    auto inputArea = area.removeFromTop(static_cast<int>(lineHeight));
    if (transportAction.isVisible()) transportAction.setBounds(inputArea);
    if (presetNumber.isVisible()) presetNumber.setBounds(inputArea);
    if (transposeSemitones.isVisible()) transposeSemitones.setBounds(inputArea);
    
    if (presetProgramGroup.isVisible()) {
        area.removeFromTop(static_cast<int>(lineHeight));
        auto programArea = area.removeFromTop(static_cast<int>(lineHeight * 8));
        presetProgramGroup.setBounds(programArea);
        programArea.reduce(static_cast<int>(static_cast<float>(programArea.getWidth()) * 0.1f), static_cast<int>(lineHeight));
        programArea.removeFromTop(static_cast<int>(lineHeight));
        for (size_t zone = 0; zone < presetPrograms.size(); ++zone) {
            auto row = programArea.removeFromTop(static_cast<int>(lineHeight));
            presetPrograms[zone]->setBounds(row.removeFromRight(36));
            row.removeFromRight(6);
            presetProgramEnabled[zone].setBounds(row);
            programArea.removeFromTop(static_cast<int>(lineHeight / 2));
        }
    }

    if (modeRadioGroup.isVisible())
    {
        area.removeFromTop(static_cast<int>(lineHeight));
        auto modeArea = area.removeFromTop(static_cast<int>(lineHeight * 6));
        modeRadioGroup.setBounds(modeArea);
        modeArea.reduce(static_cast<int>(static_cast<float>(modeArea.getWidth()) * 0.1f), static_cast<int>(lineHeight));
        modeArea.removeFromTop(static_cast<int>(lineHeight));
        modeLatch.setBounds(modeArea.removeFromTop(static_cast<int>(lineHeight)));
        if (modeMomentary.isVisible()) modeMomentary.setBounds(modeArea.removeFromTop(static_cast<int>(lineHeight)));
        modeTrigger.setBounds(modeArea.removeFromTop(static_cast<int>(lineHeight)));
    }
}

juce::String AppCtrlSectionComponent::getMessageString() {
    if (typePreset.getToggleState()) {
        juce::String result = "Preset;" + juce::String(presetNumber.getValue());
        bool hasPrograms = false;
        for (auto& enabled : presetProgramEnabled)
            hasPrograms |= enabled.getToggleState();
        if (hasPrograms)
            for (size_t zone = 0; zone < presetPrograms.size(); ++zone)
                result += ";" + juce::String(presetProgramEnabled[zone].getToggleState() ? presetPrograms[zone]->getValue() : -1);
        return result;
    } else if (typeTransport.getToggleState()) {
        return modeTrigger.getToggleState()
            ? "Transport;Trigger;" + juce::String(transportAction.getSelectedId() == 2 ? "Stop" : "Start")
            : "Transport;Latch";
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
    
    const bool transport = tokens[0] == "Transport";
    const bool transpose = tokens[0] == "Transpose";
    typePreset.setToggleState(!transport && !transpose, juce::dontSendNotification);
    typeTranspose.setToggleState(transpose, juce::dontSendNotification);
    typeTransport.setToggleState(transport, juce::dontSendNotification);
    presetNumber.setValue(!transport && !transpose && tokens.size() >= 2 ? tokens[1].getIntValue() : 1);
    transposeSemitones.setValue(transpose ? (tokens.size() == 3 ? tokens[2] : tokens[1]).getIntValue() : 0);
    for (size_t zone = 0; zone < presetPrograms.size(); ++zone) {
        const int tokenIndex = static_cast<int>(zone) + 2;
        const int program = !transport && !transpose && tokens.size() > tokenIndex
            ? tokens[tokenIndex].getIntValue() : -1;
        presetProgramEnabled[zone].setToggleState(program >= 0 && program <= 127, juce::dontSendNotification);
        presetPrograms[zone]->setValue(juce::jlimit(0, 127, program));
    }
    const auto mode = transport || tokens.size() == 3 ? tokens[1] : juce::String("Latch");
    modeTrigger.setToggleState(mode == "Trigger", juce::dontSendNotification);
    modeMomentary.setToggleState(transpose && mode == "Momentary", juce::dontSendNotification);
    modeLatch.setToggleState(!modeTrigger.getToggleState() && !modeMomentary.getToggleState(), juce::dontSendNotification);
    transportAction.setSelectedId(tokens[2] == "Stop" ? 2 : 1, juce::dontSendNotification);
    updateVisibility();
}

void AppCtrlSectionComponent::updateVisibility() {
    const bool transport = typeTransport.getToggleState();
    const bool transpose = typeTranspose.getToggleState();
    presetNumber.setVisible(typePreset.getToggleState());
    presetProgramGroup.setVisible(typePreset.getToggleState());
    for (size_t zone = 0; zone < presetPrograms.size(); ++zone) {
        presetProgramEnabled[zone].setVisible(typePreset.getToggleState());
        presetPrograms[zone]->setVisible(typePreset.getToggleState());
        presetPrograms[zone]->setEnabled(presetProgramEnabled[zone].getToggleState());
    }
    transposeSemitones.setVisible(transpose);
    modeRadioGroup.setVisible(transport || transpose);
    modeLatch.setVisible(transport || transpose);
    modeMomentary.setVisible(transpose);
    modeTrigger.setVisible(transport || transpose);
    modeLatch.setButtonText(transport ? "Latch (On: Start, Off: Stop)" : "Latch");
    transportAction.setVisible(transport && modeTrigger.getToggleState());
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
