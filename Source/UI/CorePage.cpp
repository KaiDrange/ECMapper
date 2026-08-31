#include "CorePage.h"
#include "../Core/SettingsWrapper.h"
#include "AppStyle.h"

namespace ecm {

namespace {

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

void CorePage::timerCallback() {
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
        row->nameLabel->setColour(juce::Label::textColourId, d.isRemote ? Style::accentStrong() : Style::text());
        addAndMakeVisible(row->nameLabel.get());
        
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
        addAndMakeVisible(row->emptyAddButton.get());
        
        for (int i = 0; i < (int)d.oscTargets.size(); ++i) {
            auto& target = d.oscTargets[i];
            auto tRow = std::make_unique<TargetRow>();
            
            tRow->ipLabel = std::make_unique<juce::Label>("", "Host:");
            tRow->ipLabel->setColour(juce::Label::textColourId, Style::mutedText());
            addAndMakeVisible(tRow->ipLabel.get());
            
            tRow->ipInput = std::make_unique<juce::TextEditor>();
            tRow->ipInput->setText(target.ip, juce::dontSendNotification);
            tRow->ipInput->onReturnKey = [this, dev = d.dev, targetIndex = i, r = row.get(), ti = i] {
                if (ti < (int)r->targets.size()) {
                    auto& t = r->targets[ti];
                    hardwareService_.updateDeviceOSCTarget(dev, ti, t->ipInput->getText(), t->portInput->getText().getIntValue(), t->ledToggle->getToggleState());
                }
            };
            tRow->ipInput->onFocusLost = tRow->ipInput->onReturnKey;
            addAndMakeVisible(tRow->ipInput.get());
            
            tRow->portLabel = std::make_unique<juce::Label>("", "Port:");
            tRow->portLabel->setColour(juce::Label::textColourId, Style::mutedText());
            addAndMakeVisible(tRow->portLabel.get());
            
            tRow->portInput = std::make_unique<juce::TextEditor>();
            tRow->portInput->setText(juce::String(target.port), juce::dontSendNotification);
            tRow->portInput->setInputRestrictions(5, "0123456789");
            tRow->portInput->onReturnKey = tRow->ipInput->onReturnKey;
            tRow->portInput->onFocusLost = tRow->ipInput->onReturnKey;
            addAndMakeVisible(tRow->portInput.get());
            
            tRow->ledToggle = std::make_unique<juce::TextButton>(isHost ? "L" : "Control LEDs");
            tRow->ledToggle->setClickingTogglesState(true);
            tRow->ledToggle->setToggleState(target.receiveLEDs, juce::dontSendNotification);
            tRow->ledToggle->setColour(juce::TextButton::buttonOnColourId, Style::warning());
            tRow->ledToggle->setColour(juce::TextButton::textColourOnId, Style::background());
            tRow->ledToggle->setTooltip(isHost ? "Toggle Send LEDs" : "Toggle Control LEDs");
            tRow->ledToggle->onClick = [this, dev = d.dev, ti = i, r = row.get()] {
                if (ti < (int)r->targets.size()) {
                    auto& t = r->targets[ti];
                    hardwareService_.updateDeviceOSCTarget(dev, ti, t->ipInput->getText(), t->portInput->getText().getIntValue(), t->ledToggle->getToggleState());
                }
            };
            addAndMakeVisible(tRow->ledToggle.get());
            if (isHost) tRow->ledToggle->setVisible(false);
            
            tRow->addButton = std::make_unique<juce::TextButton>("+");
            tRow->addButton->onClick = [this, dev = d.dev] {
                hardwareService_.addDeviceOSCTarget(dev, "127.0.0.1", 12130);
            };
            addAndMakeVisible(tRow->addButton.get());
            
            tRow->removeButton = std::make_unique<juce::TextButton>("X");
            tRow->removeButton->onClick = [this, dev = d.dev, ti = i] {
                hardwareService_.removeDeviceOSCTarget(dev, ti);
            };
            addAndMakeVisible(tRow->removeButton.get());
            
            row->targets.push_back(std::move(tRow));
        }
        
        auto updateVisibility = [row = row.get(), isHost]() {
            bool oscVisible = row->modeCombo->getSelectedId() > 1;
            bool canAdd = row->targets.size() < 3;
            bool canRemove = row->targets.size() > 1;
            bool empty = row->targets.empty();
            
            for (auto& t : row->targets) {
                t->ipLabel->setVisible(oscVisible);
                t->ipInput->setVisible(oscVisible);
                t->portLabel->setVisible(oscVisible);
                t->portInput->setVisible(oscVisible);
                t->ledToggle->setVisible(oscVisible && !isHost);
                t->addButton->setVisible(oscVisible && canAdd);
                t->removeButton->setVisible(oscVisible && canRemove);
            }
            
            row->emptyAddButton->setVisible(oscVisible && empty);
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
        
        addAndMakeVisible(row->modeCombo.get());
        deviceRows_.push_back(std::move(row));
    }
    
    resized();
    repaint();
}

void CorePage::paint(juce::Graphics& g) {
    g.fillAll(Style::background());
    
    auto area = getLocalBounds().reduced(20);
    auto headerArea = area.removeFromTop(40);
    
    g.setColour(Style::text());
    g.setFont(20.0f);
    g.drawText("Communication", headerArea.removeFromLeft(200), juce::Justification::centredLeft);
    
    if (deviceRows_.empty()) {
        g.setColour(Style::mutedText());
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
    
    bool isHost = hardwareService_.getAppRole() == AppRole::Host;
    
    for (auto& row : deviceRows_) {
        bool oscVisible = row->modeCombo->getSelectedId() > 1 || !isHost;
        int rowHeight = 35;
        if (oscVisible) {
            int additionalRows = std::max(0, (int)row->targets.size() - 1);
            rowHeight += additionalRows * 35;
        }
        auto rowArea = area.removeFromTop(rowHeight);
        
        auto mainRow = rowArea.removeFromTop(35);
        row->statusLed->setBounds(mainRow.removeFromLeft(20).reduced(4));
        row->nameLabel->setBounds(mainRow.removeFromLeft(140));
        
        if (isHost) {
            row->modeCombo->setBounds(mainRow.removeFromLeft(100).reduced(2));
            mainRow.removeFromLeft(5);
        }
        
        if (oscVisible && !row->targets.empty()) {
            auto& t = row->targets[0];
            t->ipLabel->setBounds(mainRow.removeFromLeft(40));
            t->ipInput->setBounds(mainRow.removeFromLeft(100).reduced(2));
            mainRow.removeFromLeft(5);
            t->portLabel->setBounds(mainRow.removeFromLeft(35));
            t->portInput->setBounds(mainRow.removeFromLeft(50).reduced(2));
            mainRow.removeFromLeft(5);
            
            if (!isHost) {
                t->ledToggle->setBounds(mainRow.removeFromLeft(100).reduced(2));
            } else {
                t->ledToggle->setBounds(mainRow.removeFromLeft(25).reduced(2));
            }
            
            if (t->addButton->isVisible()) t->addButton->setBounds(mainRow.removeFromLeft(25).reduced(2));
            if (t->removeButton->isVisible()) t->removeButton->setBounds(mainRow.removeFromLeft(25).reduced(2));
        } else if (oscVisible && row->targets.empty()) {
            row->emptyAddButton->setBounds(mainRow.removeFromLeft(100).reduced(2));
        }
        
        if (oscVisible) {
            for (int i = 1; i < (int)row->targets.size(); ++i) {
                auto& t = row->targets[i];
                auto oscRow = rowArea.removeFromTop(35);
                oscRow.removeFromLeft(isHost ? 265 : 160); // align with first target
                t->ipLabel->setBounds(oscRow.removeFromLeft(40));
                t->ipInput->setBounds(oscRow.removeFromLeft(100).reduced(2));
                oscRow.removeFromLeft(5);
                t->portLabel->setBounds(oscRow.removeFromLeft(35));
                t->portInput->setBounds(oscRow.removeFromLeft(50).reduced(2));
                oscRow.removeFromLeft(5);
                
                if (!isHost) {
                    t->ledToggle->setBounds(oscRow.removeFromLeft(100).reduced(2));
                } else {
                    t->ledToggle->setBounds(oscRow.removeFromLeft(25).reduced(2));
                }
                
                if (t->addButton->isVisible()) t->addButton->setBounds(oscRow.removeFromLeft(25).reduced(2));
                if (t->removeButton->isVisible()) t->removeButton->setBounds(oscRow.removeFromLeft(25).reduced(2));
            }
        }
    }
}

} // namespace ecm
