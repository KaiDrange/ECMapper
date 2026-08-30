#include "CorePage.h"
#include "../Core/SettingsWrapper.h"

namespace ecm {

CorePage::CorePage(HardwareService& hardwareService, juce::ValueTree& state) 
    : hardwareService_(hardwareService), state_(state) {
    ledGreen = juce::ImageFileFormat::loadFrom(BinaryData::GreenLight_png, BinaryData::GreenLight_pngSize);
    ledOff = juce::ImageFileFormat::loadFrom(BinaryData::DarkLight_png, BinaryData::DarkLight_pngSize);
    
    roleLabel.setText("Role:", juce::dontSendNotification);
    addAndMakeVisible(roleLabel);
    
    roleCombo.addItem("Host (with Hardware)", 1);
    roleCombo.addItem("Client (Remote)", 2);
    roleCombo.setSelectedId(hardwareService_.getAppRole() == AppRole::Host ? 1 : 2, juce::dontSendNotification);
    roleCombo.onChange = [this] {
        auto role = roleCombo.getSelectedId() == 1 ? AppRole::Host : AppRole::Client;
        hardwareService_.setAppRole(role);
        SettingsWrapper::setAppRole(role, state_);
        updateDeviceList();
    };
    addAndMakeVisible(roleCombo);
    
    clientIpLabel.setText("Listen IP:", juce::dontSendNotification);
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
    
    bool isHost = hardwareService_.getAppRole() == AppRole::Host;
    
    clientIpLabel.setVisible(!isHost);
    clientIpInput.setVisible(!isHost);
    clientPortLabel.setVisible(!isHost);
    clientPortInput.setVisible(!isHost);

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
        
        if (isHost) {
            row->modeCombo->setItemEnabled(3, false); // No Receive OSC in Host mode for local devices
        } else {
            row->modeCombo->setItemEnabled(1, false); // No Local in Client mode
            row->modeCombo->setItemEnabled(2, false); // No Transmit in Client mode
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
        
        for (int i = 0; i < (int)d.oscTargets.size(); ++i) {
            auto& target = d.oscTargets[i];
            auto tRow = std::make_unique<TargetRow>();
            
            tRow->ipLabel = std::make_unique<juce::Label>("", "IP:");
            tRow->ipLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible(tRow->ipLabel.get());
            
            tRow->ipInput = std::make_unique<juce::TextEditor>();
            tRow->ipInput->setText(target.ip, juce::dontSendNotification);
            tRow->ipInput->onReturnKey = [this, dev = d.dev, targetIndex = i, r = row.get(), ti = i] {
                if (ti < (int)r->targets.size()) {
                    hardwareService_.updateDeviceOSCTarget(dev, ti, r->targets[ti]->ipInput->getText(), r->targets[ti]->portInput->getText().getIntValue());
                }
            };
            tRow->ipInput->onFocusLost = tRow->ipInput->onReturnKey;
            addAndMakeVisible(tRow->ipInput.get());
            
            tRow->portLabel = std::make_unique<juce::Label>("", "Port:");
            tRow->portLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
            addAndMakeVisible(tRow->portLabel.get());
            
            tRow->portInput = std::make_unique<juce::TextEditor>();
            tRow->portInput->setText(juce::String(target.port), juce::dontSendNotification);
            tRow->portInput->setInputRestrictions(5, "0123456789");
            tRow->portInput->onReturnKey = tRow->ipInput->onReturnKey;
            tRow->portInput->onFocusLost = tRow->ipInput->onReturnKey;
            addAndMakeVisible(tRow->portInput.get());
            
            tRow->removeButton = std::make_unique<juce::TextButton>("X");
            tRow->removeButton->onClick = [this, dev = d.dev, ti = i] {
                hardwareService_.removeDeviceOSCTarget(dev, ti);
            };
            addAndMakeVisible(tRow->removeButton.get());
            
            row->targets.push_back(std::move(tRow));
        }
        
        row->addTargetButton = std::make_unique<juce::TextButton>("+ Target");
        row->addTargetButton->onClick = [this, dev = d.dev] {
            hardwareService_.addDeviceOSCTarget(dev, "127.0.0.1", 12130);
        };
        addAndMakeVisible(row->addTargetButton.get());
        
        row->disconnectButton = std::make_unique<juce::TextButton>("Disconnect");
        row->disconnectButton->onClick = [this, dev = d.dev] {
            juce::Logger::writeToLog("Disconnect requested for device: " + dev);
        };
        addAndMakeVisible(row->disconnectButton.get());
        
        auto updateVisibility = [row = row.get()]() {
            bool oscVisible = row->modeCombo->getSelectedId() > 1;
            for (auto& t : row->targets) {
                t->ipLabel->setVisible(oscVisible);
                t->ipInput->setVisible(oscVisible);
                t->portLabel->setVisible(oscVisible);
                t->portInput->setVisible(oscVisible);
                t->removeButton->setVisible(oscVisible);
            }
            row->addTargetButton->setVisible(oscVisible && row->targets.size() < 3);
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
    
    if (deviceRows_.empty()) {
        g.setColour(juce::Colours::grey);
        auto emptyArea = getLocalBounds().reduced(20);
        emptyArea.removeFromTop(100);
        g.drawFittedText(hardwareService_.getAppRole() == AppRole::Host ? "No local devices connected" : "No remote devices discovered", emptyArea, juce::Justification::centred, 1);
    }
}

void CorePage::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40); // Title area
    
    auto roleArea = area.removeFromTop(40);
    roleLabel.setBounds(roleArea.removeFromLeft(50));
    roleCombo.setBounds(roleArea.removeFromLeft(200).reduced(5));
    
    if (hardwareService_.getAppRole() == AppRole::Client) {
        auto clientArea = area.removeFromTop(40);
        clientIpLabel.setBounds(clientArea.removeFromLeft(70));
        clientIpInput.setBounds(clientArea.removeFromLeft(120).reduced(5));
        clientArea.removeFromLeft(10);
        clientPortLabel.setBounds(clientArea.removeFromLeft(80));
        clientPortInput.setBounds(clientArea.removeFromLeft(80).reduced(5));
    }
    
    area.removeFromTop(10);
    
    for (auto& row : deviceRows_) {
        bool oscVisible = row->modeCombo->getSelectedId() > 1;
        int rowHeight = 40;
        if (oscVisible) {
            rowHeight = 35 + (int)row->targets.size() * 35;
            if (row->targets.size() < 3) rowHeight += 35;
        }
        auto rowArea = area.removeFromTop(rowHeight);
        
        auto mainRow = rowArea.removeFromTop(35);
        row->statusLed->setBounds(mainRow.removeFromLeft(20).reduced(4));
        row->nameLabel->setBounds(mainRow.removeFromLeft(180));
        row->disconnectButton->setBounds(mainRow.removeFromRight(80).reduced(2));
        row->modeCombo->setBounds(mainRow.reduced(2));
        
        if (oscVisible) {
            for (auto& t : row->targets) {
                auto oscRow = rowArea.removeFromTop(35);
                oscRow.removeFromLeft(40); // indent
                t->ipLabel->setBounds(oscRow.removeFromLeft(30));
                t->ipInput->setBounds(oscRow.removeFromLeft(150).reduced(2));
                oscRow.removeFromLeft(10);
                t->portLabel->setBounds(oscRow.removeFromLeft(40));
                t->portInput->setBounds(oscRow.removeFromLeft(80).reduced(2));
                oscRow.removeFromLeft(10);
                t->removeButton->setBounds(oscRow.removeFromLeft(30).reduced(2));
            }
            if (row->targets.size() < 3) {
                auto addRow = rowArea.removeFromTop(35);
                addRow.removeFromLeft(40);
                row->addTargetButton->setBounds(addRow.removeFromLeft(100).reduced(2));
            }
        }
    }
}

} // namespace ecm
