#include "CorePage.h"
#include "../Core/SettingsWrapper.h"
#include "AppStyle.h"

namespace ecm {

namespace {

void configureAudioKnob(juce::Slider& slider, const juce::String& name, double maximum)
{
    slider.setName(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    slider.setRange(0.0, maximum, 1.0);
    slider.setScrollWheelEnabled(false);
    slider.setColour(juce::Slider::rotarySliderFillColourId, Style::accent());
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, Style::border());
}

juce::Image createStatusLed(juce::Colour bodyColour, juce::Colour glowColour, bool glowEnabled)
{
    constexpr int size = 32;
    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    const auto full = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(size), static_cast<float>(size));
    const auto body = full.reduced(4.0f);
    const auto centre = body.getCentre();

    if (glowEnabled)
    {
        juce::ColourGradient outerGlow(glowColour.withAlpha(0.0f), centre,
                                       glowColour.withAlpha(0.55f), centre.translated(16.0f, 0.0f),
                                       true);
        g.setGradientFill(outerGlow);
        g.fillEllipse(0.0f, 0.0f, static_cast<float>(size), static_cast<float>(size));

        juce::ColourGradient innerGlow(glowColour.withAlpha(0.0f), centre,
                                       glowColour.withAlpha(0.8f), centre.translated(9.0f, 0.0f),
                                       true);
        g.setGradientFill(innerGlow);
        g.fillEllipse(4.0f, 4.0f, 24.0f, 24.0f);
    }

    juce::ColourGradient bodyFill(bodyColour.brighter(0.25f), centre.x, body.getY(),
                                  bodyColour.darker(0.25f), centre.x, body.getBottom(), false);
    g.setGradientFill(bodyFill);
    g.fillEllipse(body);

    g.setColour(bodyColour.brighter(0.45f).withAlpha(glowEnabled ? 0.9f : 0.55f));
    g.drawEllipse(body.reduced(0.5f), 1.0f);

    g.setColour(juce::Colours::white.withAlpha(glowEnabled ? 0.45f : 0.2f));
    g.fillEllipse(body.getX() + 5.5f, body.getY() + 4.0f, body.getWidth() * 0.36f, body.getHeight() * 0.24f);

    if (glowEnabled)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawEllipse(full.reduced(1.5f), 1.0f);
    }

    return image;
}

} // namespace

CorePage::CorePage(HardwareService& hardwareService, juce::ValueTree& state) 
    : hardwareService_(hardwareService), state_(state) {
    const bool hardwareEnabled = HardwareService::supportsLocalHardware();

    ledGreen = createStatusLed(juce::Colour(0xff4fd17a), juce::Colour(0xff4fd17a), true);
    ledOff = createStatusLed(juce::Colour(0xff51625a), juce::Colour(0xff51625a), false);
    
    ledRed = juce::Image(juce::Image::ARGB, 32, 32, true);
    {
        juce::Graphics g(ledRed);
        g.setColour(Style::danger());
        g.fillEllipse(4, 4, 24, 24);
        g.setColour(Style::text().withAlpha(0.4f));
        g.fillEllipse(8, 8, 10, 10);
    }
    
    roleLabel.setText("Role:", juce::dontSendNotification);
    addAndMakeVisible(roleLabel);
    
    roleCombo.addItem("Host (with Hardware)", 1);
    roleCombo.addItem("Client (Remote)", 2);
    if (!hardwareEnabled) {
        roleCombo.setItemEnabled(1, false);
    }
    roleCombo.setSelectedId(hardwareService_.getAppRole() == AppRole::Host ? 1 : 2, juce::dontSendNotification);
    roleCombo.onChange = [this] {
        auto role = roleCombo.getSelectedId() == 1 ? AppRole::Host : AppRole::Client;
        if (!HardwareService::supportsLocalHardware()) {
            role = AppRole::Client;
        }
        hardwareService_.setAppRole(role);
        SettingsWrapper::setAppRole(role, state_);
        updateDeviceList();
    };
    addAndMakeVisible(roleCombo);
    
    clientIpLabel.setText("Listen Host:", juce::dontSendNotification);
    clientIpInput.setText(hardwareService_.getClientListenIP(), juce::dontSendNotification);
    clientIpInput.onReturnKey = [this] {
        hardwareService_.setClientListenSettings(clientIpInput.getText(), clientPortInput.getText().getIntValue());
        SettingsWrapper::setClientListenIP(clientIpInput.getText(), state_);
        SettingsWrapper::setClientListenPort(clientPortInput.getText().getIntValue(), state_);
    };
    clientIpInput.onFocusLost = clientIpInput.onReturnKey;
    
    clientPortLabel.setText("Listen Port:", juce::dontSendNotification);
    clientPortInput.setText(juce::String(hardwareService_.getClientListenPort()), juce::dontSendNotification);
    clientPortInput.setInputRestrictions(5, "0123456789");
    clientPortInput.onReturnKey = clientIpInput.onReturnKey;
    clientPortInput.onFocusLost = clientIpInput.onReturnKey;
    
    addAndMakeVisible(clientIpLabel);
    addAndMakeVisible(clientIpInput);
    addAndMakeVisible(clientPortLabel);
    addAndMakeVisible(clientPortInput);
    
    addAndMakeVisible(audioGroup);
    configureAudioKnob(metronomeVolume, "Metronome volume", 100.0);
    configureAudioKnob(audioInputVolume, "Audio input volume", 100.0);
    auto audioSettings = SettingsWrapper::getAudioOutputSettings(state_);
    metronomeVolume.getValueObject().referTo(audioSettings.getPropertyAsValue(SettingsWrapper::id_metronomeVolume, nullptr));
    audioInputVolume.getValueObject().referTo(audioSettings.getPropertyAsValue(SettingsWrapper::id_audioInputVolume, nullptr));
    for (auto* slider : { &metronomeVolume, &audioInputVolume }) {
        slider->setTextValueSuffix(" %");
        slider->setDoubleClickReturnValue(true, 100.0);
        addAndMakeVisible(slider);
    }
    for (auto* label : { &metronomeLabel, &audioInputLabel }) {
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    }
    metronomeVolume.setTooltip("Metronome level for all Alpha and Tau outputs (currently the test WAV)");
    metronomeVolume.onValueChange = [this] {
        hardwareService_.setTestAudioVolume(static_cast<float>(metronomeVolume.getValue()) / 100.0f);
    };
    metronomeVolume.onValueChange();
    audioInputVolume.setTooltip("Audio input level for all Alpha and Tau outputs");
    clockSettings = SettingsWrapper::getClockSettings(state_);
    addAndMakeVisible(clockGroup);
    const auto configureClockSource = [this](juce::ToggleButton& button, const char* source) {
        button.setRadioGroupId(1001);
        button.onClick = [this, source] {
            clockSettings.setProperty(SettingsWrapper::id_clockSource, source, nullptr);
            updateClockControls();
        };
        addAndMakeVisible(button);
    };
    configureClockSource(midiClockIn, "midiIn");
    configureClockSource(midiClockMaster, "midiMaster");
    configureClockSource(abletonLink, "abletonLink");
    midiClockIn.setTooltip("Follow incoming MIDI clock (slave mode)");
    midiClockMaster.setTooltip("Set the tempo locally as MIDI clock master");
    abletonLink.setTooltip("Use Ableton Link for tempo synchronization");

    bpmInput.setName("BPM");
    bpmInput.setSliderStyle(juce::Slider::IncDecButtons);
    bpmInput.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 62, 26);
    bpmInput.setRange(20.0, 300.0, 0.1);
    bpmInput.setScrollWheelEnabled(false);
    bpmInput.getValueObject().referTo(clockSettings.getPropertyAsValue(SettingsWrapper::id_clockBpm, nullptr));
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(bpmInput);
    addAndMakeVisible(timeSignatureLabel);
    addAndMakeVisible(timeSignature);
    timeSignature.addItemList({ "None", "2/4", "3/4", "4/4", "5/4", "6/4", "3/8", "6/8", "7/8", "9/8", "12/8" }, 1);
    timeSignature.onChange = [this] {
        clockSettings.setProperty(SettingsWrapper::id_timeSignature, timeSignature.getText(), nullptr);
    };
    for (auto* button : { &startButton, &stopButton }) {
        button->setColour(juce::TextButton::buttonOnColourId, Style::accent());
        button->setColour(juce::TextButton::textColourOnId, Style::background());
        addAndMakeVisible(button);
    }
    startButton.setTooltip("Play the temporary 48 kHz test WAV from the beginning");
    startButton.onClick = [this] {
        if (const auto error = hardwareService_.startTestAudio(); error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Test audio", error);
        updateClockControls();
    };
    stopButton.onClick = [this] { hardwareService_.stopTestAudio(); updateClockControls(); };
    updateClockControls();

    addAndMakeVisible(devicesLabel);
    emptyDevicesLabel.setJustificationType(juce::Justification::centred);
    emptyDevicesLabel.setColour(juce::Label::textColourId, Style::mutedText());
    addAndMakeVisible(emptyDevicesLabel);
    deviceViewport.setViewedComponent(&deviceContent, false);
    deviceViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(deviceViewport);

    hardwareService_.addListener(this);
    updateDeviceList();
    
    startTimer(200);
}

CorePage::~CorePage() {
    hardwareService_.removeListener(this);
}

void CorePage::deviceListChanged() {
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<CorePage>(this)] {
        if (safeThis != nullptr) safeThis->updateDeviceList();
    });
}

void CorePage::updateClockControls() {
    const auto source = clockSettings.getProperty(SettingsWrapper::id_clockSource).toString();
    const bool slave = source == "midiIn";
    const bool link = source == "abletonLink";
    midiClockIn.setToggleState(slave, juce::dontSendNotification);
    midiClockMaster.setToggleState(!slave && !link, juce::dontSendNotification);
    abletonLink.setToggleState(link, juce::dontSendNotification);
    bpmInput.setEnabled(!slave);
    bpmLabel.setEnabled(!slave);
    bpmInput.setTooltip(slave ? "Tempo follows incoming MIDI clock" : "Tempo in beats per minute (20-300)");
    timeSignature.setText(clockSettings.getProperty(SettingsWrapper::id_timeSignature).toString(), juce::dontSendNotification);
    const bool transportRunning = hardwareService_.isTestAudioPlaying();
    startButton.setToggleState(transportRunning, juce::dontSendNotification);
    stopButton.setToggleState(!transportRunning, juce::dontSendNotification);
}

void CorePage::timerCallback() {
    updateClockControls();
    auto devices = hardwareService_.getConnectedDevices();
    auto currentTime = juce::Time::getMillisecondCounter();
    
    for (auto& row : deviceRows_) {
        for (const auto& d : devices) {
            if (d.dev == row->dev) {
                if (d.isRemote) {
                    // Green if message received in the last 2 seconds
                    if (currentTime - d.lastMessageTime < 2000) {
                        row->statusLed->setImage(ledGreen);
                    } else {
                        row->statusLed->setImage(ledRed);
                    }
                } else {
                    // Local devices are green if they are in the list
                    row->statusLed->setImage(ledGreen);
                }
                break;
            }
        }
    }
    repaint();
}

void CorePage::updateDeviceList() {
    deviceRows_.clear();
    
    bool isHost = hardwareService_.getAppRole() == AppRole::Host;
    roleCombo.setSelectedId(isHost ? 1 : 2, juce::dontSendNotification);
    
    clientIpLabel.setVisible(!isHost);
    clientIpInput.setVisible(!isHost);
    clientPortLabel.setVisible(!isHost);
    clientPortInput.setVisible(!isHost);

    auto devices = hardwareService_.getConnectedDevices();
    emptyDevicesLabel.setText(isHost ? "No local devices connected" : "No remote devices discovered", juce::dontSendNotification);
    emptyDevicesLabel.setVisible(devices.empty());
    
    for (const auto& d : devices) {
        auto row = std::make_unique<DeviceRow>();
        row->dev = d.dev;
        row->card = std::make_unique<juce::GroupComponent>();
        row->card->setColour(juce::GroupComponent::outlineColourId, Style::border());
        deviceContent.addAndMakeVisible(row->card.get());

        if (d.type == InstrumentType::Alpha || d.type == InstrumentType::Tau) {
            auto settings = SettingsWrapper::getHeadphoneSettings(
                d.isRemote && !d.remoteOriginalDevId.empty() ? juce::String(d.remoteOriginalDevId) : juce::String(d.dev), state_);
            const bool canControl = isHost && !d.isRemote;
            row->headphoneGain = std::make_unique<juce::Slider>();
            // EigenLite stores 127 - attenuation in dB, not a linear percentage.
            const bool limited = settings.getProperty(SettingsWrapper::id_headphoneLimited, true);
            configureAudioKnob(*row->headphoneGain, "Headphone gain", limited ? 97.0 : 127.0);
            row->headphoneGain->textFromValueFunction = [](double value) {
                return juce::String(juce::roundToInt(value) - 127) + " dB";
            };
            row->headphoneGain->valueFromTextFunction = [](const juce::String& text) {
                return text.getDoubleValue() + 127.0;
            };
            row->headphoneGain->getValueObject().referTo(settings.getPropertyAsValue(SettingsWrapper::id_headphoneGain, nullptr));
            row->headphoneGain->setDoubleClickReturnValue(true, 70.0);
            row->headphoneGain->setEnabled(canControl);
            row->headphoneGain->setTooltip("Hardware headphone gain in dB. Limit on: maximum -30 dB. Limit off: maximum 0 dB. Controlled by the Host.");
            deviceContent.addAndMakeVisible(row->headphoneGain.get());
            row->headphoneGainLabel = std::make_unique<juce::Label>("", "Headphone gain");
            row->headphoneGainLabel->setJustificationType(juce::Justification::centred);
            deviceContent.addAndMakeVisible(row->headphoneGainLabel.get());
            row->headphoneEnabled = std::make_unique<juce::TextButton>("Enable");
            row->headphoneEnabled->setClickingTogglesState(true);
            row->headphoneEnabled->getToggleStateValue().referTo(settings.getPropertyAsValue(SettingsWrapper::id_headphoneEnabled, nullptr));
            row->headphoneEnabled->setEnabled(canControl);
            row->headphoneEnabled->setColour(juce::TextButton::buttonOnColourId, Style::accent());
            row->headphoneEnabled->setColour(juce::TextButton::textColourOnId, Style::background());
            row->headphoneEnabled->setTooltip("Enable this device's headphone output. Controlled by the Host.");
            deviceContent.addAndMakeVisible(row->headphoneEnabled.get());
            row->headphoneLimited = std::make_unique<juce::TextButton>("Limit to -30 dB");
            row->headphoneLimited->setClickingTogglesState(true);
            row->headphoneLimited->getToggleStateValue().referTo(settings.getPropertyAsValue(SettingsWrapper::id_headphoneLimited, nullptr));
            row->headphoneLimited->setEnabled(canControl);
            row->headphoneLimited->setColour(juce::TextButton::buttonOnColourId, Style::accent());
            row->headphoneLimited->setColour(juce::TextButton::textColourOnId, Style::background());
            row->headphoneLimited->setTooltip("Limit hardware headphone gain to -30 dB. On by default. Controlled by the Host.");
            deviceContent.addAndMakeVisible(row->headphoneLimited.get());
            const auto applyHeadphones = [this, deviceRow = row.get()] {
                hardwareService_.setHeadphoneSettings(deviceRow->dev,
                    deviceRow->headphoneEnabled->getToggleState(),
                    static_cast<unsigned>(deviceRow->headphoneGain->getValue()),
                    deviceRow->headphoneLimited->getToggleState());
            };
            row->headphoneGain->onValueChange = applyHeadphones;
            row->headphoneEnabled->onClick = applyHeadphones;
            row->headphoneLimited->onClick = [deviceRow = row.get(), applyHeadphones] {
                deviceRow->headphoneGain->setRange(0.0, deviceRow->headphoneLimited->getToggleState() ? 97.0 : 127.0, 1.0);
                applyHeadphones();
            };
            if (canControl) applyHeadphones();
        }
        
        row->statusLed = std::make_unique<juce::ImageComponent>();
        row->statusLed->setImage(ledGreen);
        deviceContent.addAndMakeVisible(row->statusLed.get());
        
        juce::String typeStr;
        switch (d.type) {
            case InstrumentType::Alpha: typeStr = "Alpha"; break;
            case InstrumentType::Tau: typeStr = "Tau"; break;
            case InstrumentType::Pico: typeStr = "Pico"; break;
            case InstrumentType::None:
            default: typeStr = "Unknown"; break;
        }
        
        juce::String labelText = typeStr + " (" + d.dev + ")";
        if (d.isRemote) labelText += " [Remote]";
        row->nameLabel = std::make_unique<juce::Label>("", labelText);
        row->nameLabel->setColour(juce::Label::textColourId, d.isRemote ? Style::accentStrong() : Style::text());
        deviceContent.addAndMakeVisible(row->nameLabel.get());
        
        row->modeCombo = std::make_unique<juce::ComboBox>();
        row->modeCombo->addItem("Local", 1);
        row->modeCombo->addItem("Transmit", 2);
        row->modeCombo->addItem("Receive", 3);
        
        if (isHost) {
            row->modeCombo->setItemEnabled(3, false); // No Receive OSC in Host mode for local devices
            row->modeCombo->setVisible(true);
        } else {
            row->modeCombo->setVisible(false);
        }
        
        if (d.isRemote) {
            row->modeCombo->setItemEnabled(1, false);
            row->modeCombo->setItemEnabled(2, false);
            row->modeCombo->setItemEnabled(3, true);
        }
        
        int selectedId = 1;
        switch (d.mode) {
            case DeviceMode::Local: selectedId = 1; break;
            case DeviceMode::TransmitOSC: selectedId = 2; break;
            case DeviceMode::ReceiveOSC: selectedId = 3; break;
        }
        row->modeCombo->setSelectedId(selectedId, juce::dontSendNotification);
        
        row->emptyAddButton = std::make_unique<juce::TextButton>("+ Target");
        row->emptyAddButton->onClick = [this, dev = d.dev] {
            hardwareService_.addDeviceOSCTarget(dev, "127.0.0.1", 12130);
        };
        deviceContent.addAndMakeVisible(row->emptyAddButton.get());
        
        for (int i = 0; i < (int)d.oscTargets.size(); ++i) {
            auto& target = d.oscTargets[static_cast<std::size_t>(i)];
            auto tRow = std::make_unique<TargetRow>();
            
            tRow->ipLabel = std::make_unique<juce::Label>("", "Host:");
            tRow->ipLabel->setColour(juce::Label::textColourId, Style::mutedText());
            deviceContent.addAndMakeVisible(tRow->ipLabel.get());
            
            tRow->ipInput = std::make_unique<juce::TextEditor>();
            tRow->ipInput->setText(target.ip, juce::dontSendNotification);
            tRow->ipInput->onReturnKey = [this, dev = d.dev, r = row.get(), ti = i] {
                if (ti < (int)r->targets.size()) {
                    auto& t = r->targets[static_cast<std::size_t>(ti)];
                    hardwareService_.updateDeviceOSCTarget(dev, ti, t->ipInput->getText(), t->portInput->getText().getIntValue(), t->ledToggle->getToggleState());
                }
            };
            tRow->ipInput->onFocusLost = tRow->ipInput->onReturnKey;
            deviceContent.addAndMakeVisible(tRow->ipInput.get());
            
            tRow->portLabel = std::make_unique<juce::Label>("", "Port:");
            tRow->portLabel->setColour(juce::Label::textColourId, Style::mutedText());
            deviceContent.addAndMakeVisible(tRow->portLabel.get());
            
            tRow->portInput = std::make_unique<juce::TextEditor>();
            tRow->portInput->setText(juce::String(target.port), juce::dontSendNotification);
            tRow->portInput->setInputRestrictions(5, "0123456789");
            tRow->portInput->onReturnKey = tRow->ipInput->onReturnKey;
            tRow->portInput->onFocusLost = tRow->ipInput->onReturnKey;
            deviceContent.addAndMakeVisible(tRow->portInput.get());
            
            tRow->ledToggle = std::make_unique<juce::TextButton>(isHost ? "L" : "Control LEDs");
            tRow->ledToggle->setClickingTogglesState(true);
            tRow->ledToggle->setToggleState(target.receiveLEDs, juce::dontSendNotification);
            tRow->ledToggle->setColour(juce::TextButton::buttonOnColourId, Style::warning());
            tRow->ledToggle->setColour(juce::TextButton::textColourOnId, Style::background());
            tRow->ledToggle->setTooltip(isHost ? "Toggle Send LEDs" : "Toggle Control LEDs");
            tRow->ledToggle->onClick = [this, dev = d.dev, ti = i, r = row.get()] {
                if (ti < (int)r->targets.size()) {
                    auto& t = r->targets[static_cast<std::size_t>(ti)];
                    hardwareService_.updateDeviceOSCTarget(dev, ti, t->ipInput->getText(), t->portInput->getText().getIntValue(), t->ledToggle->getToggleState());
                }
            };
            deviceContent.addAndMakeVisible(tRow->ledToggle.get());
            if (isHost) tRow->ledToggle->setVisible(false);
            
            tRow->addButton = std::make_unique<juce::TextButton>("+");
            tRow->addButton->onClick = [this, dev = d.dev] {
                hardwareService_.addDeviceOSCTarget(dev, "127.0.0.1", 12130);
            };
            deviceContent.addAndMakeVisible(tRow->addButton.get());
            
            tRow->removeButton = std::make_unique<juce::TextButton>("X");
            tRow->removeButton->onClick = [this, dev = d.dev, ti = i] {
                hardwareService_.removeDeviceOSCTarget(dev, ti);
            };
            deviceContent.addAndMakeVisible(tRow->removeButton.get());
            
            row->targets.push_back(std::move(tRow));
        }
        
        auto updateVisibility = [deviceRow = row.get(), isHost]() {
            bool oscVisible = deviceRow->modeCombo->getSelectedId() > 1;
            bool canAdd = deviceRow->targets.size() < 3;
            bool canRemove = deviceRow->targets.size() > 1;
            bool empty = deviceRow->targets.empty();
            
            for (auto& t : deviceRow->targets) {
                t->ipLabel->setVisible(oscVisible);
                t->ipInput->setVisible(oscVisible);
                t->portLabel->setVisible(oscVisible);
                t->portInput->setVisible(oscVisible);
                t->ledToggle->setVisible(oscVisible && !isHost);
                t->addButton->setVisible(oscVisible && canAdd);
                t->removeButton->setVisible(oscVisible && canRemove);
            }
            
            deviceRow->emptyAddButton->setVisible(oscVisible && empty);
        };
        
        row->modeCombo->onChange = [this, dev = d.dev, combo = row->modeCombo.get(), updateVisibility] {
            DeviceMode mode = DeviceMode::Local;
            switch (combo->getSelectedId()) {
                case 1: mode = DeviceMode::Local; break;
                case 2: mode = DeviceMode::TransmitOSC; break;
                case 3: mode = DeviceMode::ReceiveOSC; break;
                default: ;
            }
            hardwareService_.setDeviceMode(dev, mode);
            updateVisibility();
            resized();
        };
        
        updateVisibility();
        
        deviceContent.addAndMakeVisible(row->modeCombo.get());
        row->modeCombo->setVisible(isHost);
        deviceRows_.push_back(std::move(row));
    }
    
    resized();
    repaint();
}

void CorePage::paint(juce::Graphics& g) {
    g.fillAll(Style::background());
    g.setColour(Style::tabColour(0).withAlpha(0.80f));
    g.fillRect(0, 0, getWidth(), 3);
    g.setColour(Style::text());
    g.setFont(20.0f);
    g.drawText("Connections", getLocalBounds().reduced(20).removeFromTop(32), juce::Justification::centredLeft);
}

void CorePage::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(36);
    const bool isHost = hardwareService_.getAppRole() == AppRole::Host;
    auto roleArea = area.removeFromTop(36);
    roleLabel.setBounds(roleArea.removeFromLeft(50));
    roleCombo.setBounds(roleArea.removeFromLeft(200).reduced(2));

    if (!isHost) {
        auto clientArea = area.removeFromTop(36);
        clientIpLabel.setBounds(clientArea.removeFromLeft(80));
        clientIpInput.setBounds(clientArea.removeFromLeft(120).reduced(2));
        clientArea.removeFromLeft(12);
        clientPortLabel.setBounds(clientArea.removeFromLeft(80));
        clientPortInput.setBounds(clientArea.removeFromLeft(80).reduced(2));
    }
    area.removeFromTop(8);
    auto controlsArea = area.removeFromTop(140);
    auto audioArea = controlsArea.removeFromLeft(340);
    controlsArea.removeFromLeft(12);
    clockGroup.setBounds(controlsArea);
    auto clockArea = controlsArea.reduced(12, 10);
    clockArea.removeFromTop(10);
    auto sources = clockArea.removeFromTop(28);
    midiClockIn.setBounds(sources.removeFromLeft(112));
    midiClockMaster.setBounds(sources.removeFromLeft(140));
    abletonLink.setBounds(sources);
    clockArea.removeFromTop(6);
    auto tempo = clockArea.removeFromTop(28);
    bpmLabel.setBounds(tempo.removeFromLeft(36));
    bpmInput.setBounds(tempo.removeFromLeft(100));
    tempo.removeFromLeft(12);
    timeSignatureLabel.setBounds(tempo.removeFromLeft(98));
    timeSignature.setBounds(tempo.removeFromLeft(80));
    clockArea.removeFromTop(8);
    auto transport = clockArea.removeFromTop(28);
    startButton.setBounds(transport.removeFromLeft(90));
    transport.removeFromLeft(8);
    stopButton.setBounds(transport.removeFromLeft(90));
    audioGroup.setBounds(audioArea);
    audioArea = audioArea.reduced(16, 10);
    audioArea.removeFromTop(10);
    auto layoutKnob = [](juce::Rectangle<int> bounds, juce::Label& label, juce::Slider& slider) {
        label.setBounds(bounds.removeFromTop(22));
        slider.setBounds(bounds.withSizeKeepingCentre(90, bounds.getHeight()));
    };
    layoutKnob(audioArea.removeFromLeft(154), metronomeLabel, metronomeVolume);
    layoutKnob(audioArea.removeFromLeft(154), audioInputLabel, audioInputVolume);
    area.removeFromTop(10);
    devicesLabel.setBounds(area.removeFromTop(26));
    deviceViewport.setBounds(area);
    emptyDevicesLabel.setBounds(area.removeFromTop(80));

    const int width = juce::jmax(0, deviceViewport.getWidth() - deviceViewport.getScrollBarThickness());
    int y = 0;
    for (auto& row : deviceRows_) {
        const bool oscVisible = row->modeCombo->getSelectedId() > 1 || !isHost;
        const int extraTargets = oscVisible ? juce::jmax(0, static_cast<int>(row->targets.size()) - 1) : 0;
        const int connectionHeight = 36 + extraTargets * 36;
        const int height = 16 + juce::jmax(connectionHeight, row->headphoneGain ? 116 : 36);
        row->card->setBounds(0, y, width, height);
        auto cardArea = juce::Rectangle<int>(0, y, width, height).reduced(12, 8);
        if (row->headphoneGain) {
            auto headphones = cardArea.removeFromRight(164).withSizeKeepingCentre(164, 116);
            row->headphoneLimited->setBounds(headphones.removeFromBottom(24).reduced(2, 0));
            headphones.removeFromBottom(2);
            row->headphoneEnabled->setBounds(headphones.removeFromLeft(70).withSizeKeepingCentre(66, 28));
            headphones.removeFromLeft(4);
            row->headphoneGainLabel->setBounds(headphones.removeFromTop(20));
            row->headphoneGain->setBounds(headphones.withSizeKeepingCentre(70, 70));
            cardArea.removeFromRight(8);
        }
        cardArea = cardArea.withSizeKeepingCentre(cardArea.getWidth(), connectionHeight);
        auto header = cardArea.removeFromTop(36);
        row->statusLed->setBounds(header.removeFromLeft(24).withSizeKeepingCentre(20, 20));

        const int ipWidth = juce::jlimit(100, 160, cardArea.getWidth() / 6);
        const int targetWidth = 40 + ipWidth + 4 + 38 + 60 + 4 + (isHost ? 0 : 90) + 48;
        const int reservedWidth = (isHost ? 94 : 0)
            + (oscVisible ? (row->targets.empty() ? 110 : targetWidth) : 0);
        row->nameLabel->setBounds(header.removeFromLeft(juce::jlimit(100, 240, header.getWidth() - reservedWidth)));
        row->nameLabel->setTooltip(row->nameLabel->getText());
        if (isHost) row->modeCombo->setBounds(header.removeFromLeft(94).reduced(2));

        const int targetX = header.getX();
        const auto layoutTarget = [isHost, ipWidth](TargetRow& target, juce::Rectangle<int> bounds) {
            target.ipLabel->setBounds(bounds.removeFromLeft(40));
            target.ipInput->setBounds(bounds.removeFromLeft(ipWidth).reduced(2));
            bounds.removeFromLeft(4);
            target.portLabel->setBounds(bounds.removeFromLeft(38));
            target.portInput->setBounds(bounds.removeFromLeft(60).reduced(2));
            bounds.removeFromLeft(4);
            if (!isHost) target.ledToggle->setBounds(bounds.removeFromLeft(90).reduced(2));
            if (target.addButton->isVisible()) target.addButton->setBounds(bounds.removeFromLeft(24).reduced(2));
            if (target.removeButton->isVisible()) target.removeButton->setBounds(bounds.removeFromLeft(24).reduced(2));
        };
        if (oscVisible) {
            if (row->targets.empty()) {
                row->emptyAddButton->setBounds(header.removeFromLeft(110).reduced(2));
            } else {
                layoutTarget(*row->targets.front(), header);
                for (std::size_t i = 1; i < row->targets.size(); ++i) {
                    auto targetArea = cardArea.removeFromTop(36);
                    targetArea.removeFromLeft(targetX - targetArea.getX());
                    layoutTarget(*row->targets[i], targetArea);
                }
            }
        }
        y += height + 10;
    }
    deviceContent.setSize(width, juce::jmax(deviceViewport.getHeight(), y));
}

} // namespace ecm
