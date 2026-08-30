#include "CorePage.h"
#include "../Core/SettingsWrapper.h"

namespace ecm {

CorePage::CorePage(HardwareService& hardwareService, juce::ValueTree& state) 
    : hardwareService_(hardwareService), state_(state) {
    ledGreen = juce::ImageFileFormat::loadFrom(BinaryData::GreenLight_png, BinaryData::GreenLight_pngSize);
    ledOff = juce::ImageFileFormat::loadFrom(BinaryData::DarkLight_png, BinaryData::DarkLight_pngSize);
    
    roleLabel.setText("Role:", juce::dontSendNotification);
    addAndMakeVisible(roleLabel);
    
    roleCombo.addItem("Master (with Hardware)", 1);
    roleCombo.addItem("Slave (Remote)", 2);
    roleCombo.setSelectedId(hardwareService_.getAppRole() == AppRole::Master ? 1 : 2, juce::dontSendNotification);
    roleCombo.onChange = [this] {
        auto role = roleCombo.getSelectedId() == 1 ? AppRole::Master : AppRole::Slave;
        hardwareService_.setAppRole(role);
        SettingsWrapper::setAppRole(role, state_);
        updateDeviceList();
    };
    addAndMakeVisible(roleCombo);
    
    slaveIpLabel.setText("Listen IP:", juce::dontSendNotification);
    slaveIpInput.setText(hardwareService_.getSlaveListenIP(), juce::dontSendNotification);
    slaveIpInput.onReturnKey = [this] {
        hardwareService_.setSlaveListenSettings(slaveIpInput.getText(), slavePortInput.getText().getIntValue());
        SettingsWrapper::setSlaveListenIP(slaveIpInput.getText(), state_);
        SettingsWrapper::setSlaveListenPort(slavePortInput.getText().getIntValue(), state_);
    };
    slaveIpInput.onFocusLost = slaveIpInput.onReturnKey;
    
    slavePortLabel.setText("Listen Port:", juce::dontSendNotification);
    slavePortInput.setText(juce::String(hardwareService_.getSlaveListenPort()), juce::dontSendNotification);
    slavePortInput.setInputRestrictions(5, "0123456789");
    slavePortInput.onReturnKey = slaveIpInput.onReturnKey;
    slavePortInput.onFocusLost = slaveIpInput.onReturnKey;
    
    addAndMakeVisible(slaveIpLabel);
    addAndMakeVisible(slaveIpInput);
    addAndMakeVisible(slavePortLabel);
    addAndMakeVisible(slavePortInput);
    
    hardwareService_.addListener(this);
    updateDeviceList();
    
    startTimer(200);
}

CorePage::~CorePage() {
    hardwareService_.removeListener(this);
}

void CorePage::deviceListChanged() {
    juce::MessageManager::callAsync([this] {
        updateDeviceList();
    });
}

void CorePage::updateDeviceList() {
    deviceRows_.clear();
    
    bool isMaster = hardwareService_.getAppRole() == AppRole::Master;
    
    slaveIpLabel.setVisible(!isMaster);
    slaveIpInput.setVisible(!isMaster);
    slavePortLabel.setVisible(!isMaster);
    slavePortInput.setVisible(!isMaster);

    auto devices = hardwareService_.getConnectedDevices();
    
    for (const auto& d : devices) {
        auto row = std::make_unique<DeviceRow>();
        row->dev = d.dev;
        
        row->statusLed = std::make_unique<juce::ImageComponent>();
        row->statusLed->setImage(ledGreen);
        addAndMakeVisible(row->statusLed.get());
        
        juce::String typeStr;
        switch (d.type) {
            case InstrumentType::Alpha: typeStr = "Alpha"; break;
            case InstrumentType::Tau: typeStr = "Tau"; break;
            case InstrumentType::Pico: typeStr = "Pico"; break;
            default: typeStr = "Unknown"; break;
        }
        
        juce::String labelText = typeStr + " (" + d.dev + ")";
        if (d.isRemote) labelText += " [Remote]";
        row->nameLabel = std::make_unique<juce::Label>("", labelText);
        row->nameLabel->setColour(juce::Label::textColourId, d.isRemote ? juce::Colours::lightblue : juce::Colours::white);
        addAndMakeVisible(row->nameLabel.get());
        
        row->modeCombo = std::make_unique<juce::ComboBox>();
        row->modeCombo->addItem("Local", 1);
        row->modeCombo->addItem("Transmit OSC (EigenCore)", 2);
        row->modeCombo->addItem("Receive OSC (ECMapper)", 3);
        
        if (isMaster) {
            row->modeCombo->setItemEnabled(3, false); // No Receive OSC in Master mode for local devices
        } else {
            row->modeCombo->setItemEnabled(1, false); // No Local in Slave mode
            row->modeCombo->setItemEnabled(2, false); // No Transmit in Slave mode
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
        
        row->ipLabel = std::make_unique<juce::Label>("", "IP:");
        row->ipLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(row->ipLabel.get());
        
        row->ipInput = std::make_unique<juce::TextEditor>();
        row->portInput = std::make_unique<juce::TextEditor>();
        
        row->ipInput->setText(d.oscIP, juce::dontSendNotification);
        row->ipInput->onReturnKey = [this, dev = d.dev, row = row.get()] {
            hardwareService_.setDeviceOSCSettings(dev, row->ipInput->getText(), row->portInput->getText().getIntValue());
        };
        row->ipInput->onFocusLost = row->ipInput->onReturnKey;
        addAndMakeVisible(row->ipInput.get());
        
        row->portInput->setText(juce::String(d.oscPort), juce::dontSendNotification);
        row->portInput->setInputRestrictions(5, "0123456789");
        row->portInput->onReturnKey = row->ipInput->onReturnKey;
        row->portInput->onFocusLost = row->ipInput->onReturnKey;
        addAndMakeVisible(row->portInput.get());
        
        row->portLabel = std::make_unique<juce::Label>("", "Port:");
        row->portLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(row->portLabel.get());
        
        row->disconnectButton = std::make_unique<juce::TextButton>("Disconnect");
        row->disconnectButton->onClick = [this, dev = d.dev] {
            // For now, since EigenApi doesn't support disconnect, we just notify
            juce::Logger::writeToLog("Disconnect requested for device: " + dev);
        };
        addAndMakeVisible(row->disconnectButton.get());
        
        auto updateVisibility = [row = row.get()]() {
            bool oscVisible = row->modeCombo->getSelectedId() > 1;
            row->ipLabel->setVisible(oscVisible);
            row->ipInput->setVisible(oscVisible);
            row->portLabel->setVisible(oscVisible);
            row->portInput->setVisible(oscVisible);
        };
        
        row->modeCombo->onChange = [this, dev = d.dev, combo = row->modeCombo.get(), updateVisibility] {
            DeviceMode mode = DeviceMode::Local;
            switch (combo->getSelectedId()) {
                case 1: mode = DeviceMode::Local; break;
                case 2: mode = DeviceMode::TransmitOSC; break;
                case 3: mode = DeviceMode::ReceiveOSC; break;
            }
            hardwareService_.setDeviceMode(dev, mode);
            updateVisibility();
            resized();
        };
        
        updateVisibility();
        
        addAndMakeVisible(row->modeCombo.get());
        deviceRows_.push_back(std::move(row));
    }
    
    resized();
    repaint();
}

void CorePage::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    auto area = getLocalBounds().reduced(20);
    auto headerArea = area.removeFromTop(40);
    
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("Communication", headerArea.removeFromLeft(200), juce::Justification::centredLeft);
    
    bool serviceRunning = hardwareService_.isServiceRunning();
    float ledSize = 16.0f;
    float ledX = headerArea.getRight() - ledSize - 5;
    float ledY = headerArea.getY() + (headerArea.getHeight() - ledSize) / 2.0f;
    
    g.drawImage(serviceRunning ? ledGreen : ledOff, ledX, ledY, ledSize, ledSize, 0, 0, ledGreen.getWidth(), ledGreen.getHeight());
    
    g.setFont(14.0f);
    g.drawText("Service Status:", headerArea.removeFromRight(130), juce::Justification::centredRight);

    if (deviceRows_.empty()) {
        g.setColour(juce::Colours::grey);
        auto emptyArea = getLocalBounds().reduced(20);
        emptyArea.removeFromTop(100);
        g.drawFittedText(hardwareService_.getAppRole() == AppRole::Master ? "No local devices connected" : "No remote devices discovered", emptyArea, juce::Justification::centred, 1);
    }
}

void CorePage::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40); // Title area
    
    auto roleArea = area.removeFromTop(40);
    roleLabel.setBounds(roleArea.removeFromLeft(50));
    roleCombo.setBounds(roleArea.removeFromLeft(200).reduced(5));
    
    if (hardwareService_.getAppRole() == AppRole::Slave) {
        auto slaveArea = area.removeFromTop(40);
        slaveIpLabel.setBounds(slaveArea.removeFromLeft(70));
        slaveIpInput.setBounds(slaveArea.removeFromLeft(120).reduced(5));
        slaveArea.removeFromLeft(10);
        slavePortLabel.setBounds(slaveArea.removeFromLeft(80));
        slavePortInput.setBounds(slaveArea.removeFromLeft(80).reduced(5));
    }
    
    area.removeFromTop(10);
    
    for (auto& row : deviceRows_) {
        bool oscVisible = row->ipInput->isVisible();
        int rowHeight = oscVisible ? 70 : 40;
        auto rowArea = area.removeFromTop(rowHeight);
        
        auto mainRow = rowArea.removeFromTop(35);
        row->statusLed->setBounds(mainRow.removeFromLeft(20).reduced(4));
        row->nameLabel->setBounds(mainRow.removeFromLeft(180));
        row->disconnectButton->setBounds(mainRow.removeFromRight(80).reduced(2));
        row->modeCombo->setBounds(mainRow.reduced(2));
        
        if (oscVisible) {
            auto oscRow = rowArea.removeFromTop(35);
            oscRow.removeFromLeft(40); // indent
            row->ipLabel->setBounds(oscRow.removeFromLeft(30));
            row->ipInput->setBounds(oscRow.removeFromLeft(150).reduced(2));
            oscRow.removeFromLeft(10);
            row->portLabel->setBounds(oscRow.removeFromLeft(40));
            row->portInput->setBounds(oscRow.removeFromLeft(80).reduced(2));
        }
    }
}

} // namespace ecm
