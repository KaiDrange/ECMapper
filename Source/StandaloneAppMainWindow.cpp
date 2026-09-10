#include "StandaloneAppMainWindow.h"

#include "UI/AboutDialogComponent.h"
#include "StandaloneApp.h"
#include "UI/PresetBrowserComponent.h"
#include "UI/CalibrationDialogComponent.h"

#include <functional>

namespace {

constexpr const char* audioSettingsRootTag = "audioDeviceManager";
constexpr const char* standaloneSettingsTag = "standaloneSettings";
constexpr const char* bufferSizeAttribute = "bufferSize";
constexpr const char* midiInputAttribute = "midiInputId";
constexpr const char* zoneOutputAttributePrefix = "zoneOutput";
constexpr int defaultBufferSize = 256;

bool isInternalVirtualMidiOutput(const juce::String& name)
{
    return name.contains("ECMapper Virtual Out")
        || name.contains("ECMapper Direct");
}

juce::Array<juce::MidiDeviceInfo> getSelectableMidiOutputs()
{
    juce::Array<juce::MidiDeviceInfo> outputs;

    for (const auto& device : juce::MidiOutput::getAvailableDevices()) {
        if (!isInternalVirtualMidiOutput(device.name))
            outputs.add(device);
    }

    return outputs;
}

juce::String getZoneOutputAttributeName(const int zoneIndex)
{
    return juce::String(zoneOutputAttributePrefix) + juce::String(zoneIndex + 1);
}

int getComboIdForIndex(const int index)
{
    return index + 1;
}

int getIndexForComboId(const int comboId)
{
    return comboId - 1;
}

class StandaloneSettingsComponent final : public juce::Component
{
public:
    using SaveCallback = std::function<void(const juce::String&, const std::array<juce::String, 3>&, int)>;

    StandaloneSettingsComponent(const juce::Array<juce::MidiDeviceInfo>& availableInputs,
                                const juce::Array<juce::MidiDeviceInfo>& availableOutputs,
                                const juce::String& currentMidiInputId,
                                const std::array<juce::String, 3>& currentZoneOutputIds,
                                int currentBufferSize,
                                bool midi2ModeEnabled,
                                SaveCallback onSave)
        : availableInputs_(availableInputs),
          availableOutputs_(availableOutputs),
          onSave_(std::move(onSave))
    {
        addAndMakeVisible(descriptionLabel_);
        descriptionLabel_.setText("Select the MIDI input used to control ECMapper, the three MPE zone outputs, and the audio buffer size used by the standalone engine.", juce::dontSendNotification);
        descriptionLabel_.setJustificationType(juce::Justification::topLeft);
        descriptionLabel_.setColour(juce::Label::textColourId, ecm::Style::text());

        configureCombo(midiInputLabel_, midiInputBox_, "MIDI Input");
        addAndMakeVisible(midiInputBox_);
        midiInputBox_.addItem("None", getComboIdForIndex(0));
        for (int i = 0; i < availableInputs_.size(); ++i)
            midiInputBox_.addItem(availableInputs_.getReference(i).name, getComboIdForIndex(i + 1));
        midiInputBox_.setSelectedId(getSelectedDeviceId(availableInputs_, currentMidiInputId, true));

        for (int zoneIndex = 0; zoneIndex < 3; ++zoneIndex) {
            configureCombo(zoneLabels_[zoneIndex], zoneBoxes_[zoneIndex], "Zone " + juce::String(zoneIndex + 1) + " MIDI Output");
            addAndMakeVisible(zoneBoxes_[zoneIndex]);
            zoneBoxes_[zoneIndex].addItem("None", getComboIdForIndex(0));
            for (int outputIndex = 0; outputIndex < availableOutputs_.size(); ++outputIndex)
                zoneBoxes_[zoneIndex].addItem(availableOutputs_.getReference(outputIndex).name, getComboIdForIndex(outputIndex + 1));
            zoneBoxes_[zoneIndex].setSelectedId(getSelectedDeviceId(availableOutputs_, currentZoneOutputIds[zoneIndex], true));
            zoneBoxes_[zoneIndex].setEnabled(!midi2ModeEnabled);
        }

        configureCombo(bufferSizeLabel_, bufferSizeBox_, "Buffer Size");
        addAndMakeVisible(bufferSizeBox_);
        for (int size : { 64, 128, 256, 512, 1024 })
            bufferSizeBox_.addItem(juce::String(size), size);
        bufferSizeBox_.setSelectedId(bufferSizeBox_.indexOfItemId(currentBufferSize) >= 0 ? currentBufferSize : defaultBufferSize);

        addAndMakeVisible(cancelButton_);
        cancelButton_.setButtonText("Cancel");
        cancelButton_.onClick = [this] { closeWithResult(0); };

        addAndMakeVisible(saveButton_);
        saveButton_.setButtonText("Save");
        saveButton_.onClick = [this] {
            if (onSave_) {
                std::array<juce::String, 3> zoneOutputIds;
                for (int zoneIndex = 0; zoneIndex < 3; ++zoneIndex)
                    zoneOutputIds[zoneIndex] = selectedDeviceId(zoneBoxes_[zoneIndex], availableOutputs_);

                onSave_(selectedDeviceId(midiInputBox_, availableInputs_), zoneOutputIds, bufferSizeBox_.getSelectedId());
            }

            closeWithResult(1);
        };

        setSize(520, 310);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        descriptionLabel_.setBounds(area.removeFromTop(56));
        area.removeFromTop(8);

        layoutRow(area, midiInputLabel_, midiInputBox_);
        for (int zoneIndex = 0; zoneIndex < 3; ++zoneIndex)
            layoutRow(area, zoneLabels_[zoneIndex], zoneBoxes_[zoneIndex]);
        layoutRow(area, bufferSizeLabel_, bufferSizeBox_);

        auto buttonArea = area.removeFromBottom(36);
        saveButton_.setBounds(buttonArea.removeFromRight(100));
        buttonArea.removeFromRight(8);
        cancelButton_.setBounds(buttonArea.removeFromRight(100));
    }

private:
    static int getSelectedDeviceId(const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& identifier, bool includeDefaultChoice)
    {
        int selectedIndex = 0;
        for (int i = 0; i < devices.size(); ++i) {
            if (devices.getReference(i).identifier == identifier) {
                selectedIndex = i + (includeDefaultChoice ? 1 : 0);
                break;
            }
        }

        return getComboIdForIndex(selectedIndex);
    }

    static juce::String selectedDeviceId(const juce::ComboBox& comboBox, const juce::Array<juce::MidiDeviceInfo>& devices)
    {
        auto selectedIndex = getIndexForComboId(comboBox.getSelectedId());
        if (selectedIndex <= 0 || selectedIndex - 1 >= devices.size())
            return {};

        return devices.getReference(selectedIndex - 1).identifier;
    }

    static void configureCombo(juce::Label& label, juce::ComboBox& comboBox, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, ecm::Style::text());
        label.attachToComponent(&comboBox, true);
    }

    void layoutRow(juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& comboBox)
    {
        juce::ignoreUnused(label);
        area.removeFromTop(4);
        auto row = area.removeFromTop(28);
        comboBox.setBounds(row.removeFromRight(320));
        area.removeFromTop(4);
    }

    void closeWithResult(int result)
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState(result);
        else
            setVisible(false);
    }

    juce::Array<juce::MidiDeviceInfo> availableInputs_;
    juce::Array<juce::MidiDeviceInfo> availableOutputs_;
    SaveCallback onSave_;
    juce::Label descriptionLabel_;
    juce::Label midiInputLabel_;
    juce::ComboBox midiInputBox_;
    std::array<juce::Label, 3> zoneLabels_;
    std::array<juce::ComboBox, 3> zoneBoxes_;
    juce::Label bufferSizeLabel_;
    juce::ComboBox bufferSizeBox_;
    juce::TextButton cancelButton_;
    juce::TextButton saveButton_;
};

} // namespace

StandaloneAppMainWindow::StandaloneAppMainWindow(const juce::String& name)
    : DocumentWindow(name,
                     ecm::Style::background(),
                     DocumentWindow::allButtons),
      menuBarModel(
          [] { requestQuit(); },
          nullptr,
          [this] { showPresetBrowser(); },
          [this] { showAudioSettings(); },
          [this] { showCalibrationDialog(); },
          [this] { showAboutDialog(); },
          [] { showOnlineManual(); },
          [] { showOurMusic(); }
      )
{
    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    setUsingNativeTitleBar(true);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(&menuBarModel);
#endif

#if ! JUCE_MAC
    setMenuBar(&menuBarModel);
#endif
    setResizable(true, true);

    processor = std::make_unique<ECMapperAudioProcessor>();
    processor->setDeviceManager(&deviceManager);
    
    loadAppState();
    if (!processor->hasPresetSlot(1))
        processor->loadStandalonePresetBank();

    processorPlayer.setProcessor(processor.get());
    
    ecm::SettingsWrapper::addListener(this, processor->state.state);
    juce::universal_midi_packets::Endpoints::getInstance()->addListener(*this);

    loadAudioSettings();

    deviceManager.addAudioCallback(&processorPlayer);
    deviceManager.addMidiInputDeviceCallback({}, &processorPlayer.getMidiMessageCollector());

    updateMidiInputs();
    updateMidiOutput();

    deviceManager.addChangeListener(this);

    setContentOwned(processor->createUI(), true);

    centreWithSize(1000, 700);
    Component::setVisible(true);

    saveAudioSettings();
}

StandaloneAppMainWindow::~StandaloneAppMainWindow()
{
    juce::universal_midi_packets::Endpoints::getInstance()->removeListener(*this);

    if (processor != nullptr)
        processor->state.state.removeListener(this);
        
    saveAppState();
    saveAudioSettings();
    setContentOwned(nullptr, true);
    setLookAndFeel(nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    deviceManager.removeChangeListener(this);
    processorPlayer.setProcessor(nullptr);
    deviceManager.removeMidiInputDeviceCallback({}, &processorPlayer.getMidiMessageCollector());
    deviceManager.removeAudioCallback(&processorPlayer);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif

    setMenuBar(nullptr);
}

void StandaloneAppMainWindow::closeButtonPressed()
{
    requestQuit();
}

void StandaloneAppMainWindow::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager && !isUpdatingSettings)
    {
        juce::Logger::writeToLog("StandaloneAppMainWindow: changeListenerCallback triggered by deviceManager.");
        isUpdatingSettings = true;
        updateMidiInputs();
        updateMidiOutput();
        saveAudioSettings();
        isUpdatingSettings = false;
    }
}

void StandaloneAppMainWindow::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged);

    if (property == ecm::SettingsWrapper::id_midi2Mode)
        updateMidiOutput();
}

void StandaloneAppMainWindow::endpointsChanged()
{
    juce::Logger::writeToLog("StandaloneAppMainWindow: endpointsChanged() triggered by UMP Endpoints broadcast.");
    updateMidiOutput();
}

void StandaloneAppMainWindow::updateMidiOutput()
{
    juce::Logger::writeToLog("StandaloneAppMainWindow::updateMidiOutput() called.");
    auto availableOutputs = juce::MidiOutput::getAvailableDevices();
    juce::Logger::writeToLog("ECMapper: Available MIDI Outputs: " + juce::String(availableOutputs.size()));
    for (auto& device : availableOutputs)
        juce::Logger::writeToLog("  - " + device.name + " [" + device.identifier + "]");

    juce::MidiOutput* currentOutput = deviceManager.getDefaultMidiOutput();

    if (currentOutput != nullptr && isInternalVirtualMidiOutput(currentOutput->getName()))
    {
        juce::Logger::writeToLog("ECMapper: Ignoring disabled internal virtual MIDI output '" + currentOutput->getName() + "'.");
        currentOutput = nullptr;
    }

    if (currentOutput != nullptr)
    {
        juce::Logger::writeToLog("ECMapper: MIDI Output set to: " + currentOutput->getName());
    }
    else
    {
        juce::Logger::writeToLog("ECMapper: No MIDI Output selected.");
    }

    if (processor != nullptr)
    {
        auto* primaryOutput = isMidi2ModeEnabled() ? currentOutput : nullptr;
        processorPlayer.setMidiOutput(primaryOutput);
        processor->setMidiOutput(primaryOutput);

        openStandaloneZoneMidiOutputs();
        std::array<juce::MidiOutput*, 3> zoneOutputs { nullptr, nullptr, nullptr };
        if (!isMidi2ModeEnabled()) {
            for (size_t i = 0; i < standaloneZoneOutputs_.size(); ++i)
                zoneOutputs[i] = standaloneZoneOutputs_[i].get();
        }

        processor->setStandaloneLegacyMidiOutputs(zoneOutputs, nullptr);
    }
}

void StandaloneAppMainWindow::updateMidiInputs()
{
    restoreMidiInputSelection();
}

bool StandaloneAppMainWindow::isMidi2ModeEnabled() const
{
    return processor != nullptr && ecm::SettingsWrapper::getMidi2Mode(const_cast<juce::ValueTree&>(processor->state.state));
}

void StandaloneAppMainWindow::applyBufferSize(int bufferSize)
{
    if (bufferSize <= 0)
        return;

    requestedBufferSize_ = bufferSize;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (setup.bufferSize == requestedBufferSize_)
        return;

    setup.bufferSize = requestedBufferSize_;
    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
        juce::Logger::writeToLog("ECMapper: Failed to apply requested buffer size: " + error);
}

int StandaloneAppMainWindow::getRequestedBufferSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice()) {
        auto currentSize = device->getCurrentBufferSizeSamples();
        if (currentSize > 0)
            return currentSize;
    }

    return requestedBufferSize_ > 0 ? requestedBufferSize_ : defaultBufferSize;
}

void StandaloneAppMainWindow::openStandaloneZoneMidiOutputs()
{
    for (size_t i = 0; i < standaloneZoneOutputs_.size(); ++i) {
        standaloneZoneOutputs_[i].reset();
        auto configuredId = standaloneZoneOutputIds_[i].trim();
        if (configuredId.isEmpty())
            continue;

        standaloneZoneOutputs_[i] = juce::MidiOutput::openDevice(configuredId);
    }
}

void StandaloneAppMainWindow::restoreMidiInputSelection()
{
    auto configuredId = standaloneMidiInputId_.trim();
    auto devices = juce::MidiInput::getAvailableDevices();

    for (const auto& device : devices)
        deviceManager.setMidiInputDeviceEnabled(device.identifier, device.identifier == configuredId);
}

juce::String StandaloneAppMainWindow::getConfiguredZoneOutputId(int zoneIndex) const
{
    zoneIndex = juce::jlimit(0, 2, zoneIndex);
    return standaloneZoneOutputIds_[zoneIndex];
}

juce::String StandaloneAppMainWindow::getConfiguredMidiInputId() const
{
    return standaloneMidiInputId_;
}

void StandaloneAppMainWindow::saveAudioSettings()
{
    auto xml = deviceManager.createStateXml();
    if (xml != nullptr)
    {
        xml->setTagName(audioSettingsRootTag);
        auto* settingsNode = xml->createNewChildElement(standaloneSettingsTag);
        settingsNode->setAttribute(bufferSizeAttribute, getRequestedBufferSize());
        settingsNode->setAttribute(midiInputAttribute, standaloneMidiInputId_);
        for (int zoneIndex = 0; zoneIndex < 3; ++zoneIndex)
            settingsNode->setAttribute(getZoneOutputAttributeName(zoneIndex), standaloneZoneOutputIds_[zoneIndex]);

        auto file = getAudioSettingsFile();
        if (!file.getParentDirectory().exists())
            // ReSharper disable once CppExpressionWithoutSideEffects
            file.getParentDirectory().createDirectory();

        // ReSharper disable once CppExpressionWithoutSideEffects
        xml->writeTo(file);
    }
}

void StandaloneAppMainWindow::loadAudioSettings()
{
    auto file = getAudioSettingsFile();
    requestedBufferSize_ = defaultBufferSize;
    standaloneMidiInputId_.clear();
    standaloneZoneOutputIds_.fill(juce::String{});

    if (file.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr)
        {
            juce::Logger::writeToLog("ECMapper: Loading audio settings from " + file.getFullPathName());
            if (auto* settingsNode = xml->getChildByName(standaloneSettingsTag)) {
                requestedBufferSize_ = settingsNode->getIntAttribute(bufferSizeAttribute, defaultBufferSize);
                standaloneMidiInputId_ = settingsNode->getStringAttribute(midiInputAttribute);
                for (int zoneIndex = 0; zoneIndex < 3; ++zoneIndex)
                    standaloneZoneOutputIds_[zoneIndex] = settingsNode->getStringAttribute(getZoneOutputAttributeName(zoneIndex));
                xml->removeChildElement(settingsNode, true);
            }

            deviceManager.initialise(0, 2, xml.get(), true);
            applyBufferSize(requestedBufferSize_);
            return;
        }
    }

    juce::Logger::writeToLog("ECMapper: Initializing with default audio devices.");
    deviceManager.initialiseWithDefaultDevices(0, 2);
    applyBufferSize(requestedBufferSize_);
}

juce::File StandaloneAppMainWindow::getAudioSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("ECMapper")
           .getChildFile("audio_settings.xml");
}

void StandaloneAppMainWindow::saveAppState()
{
    juce::MemoryBlock data;
    processor->getStateInformation(data);
    
    auto file = getAppStateFile();
    if (!file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();
        
    file.replaceWithData(data.getData(), data.getSize());
}

void StandaloneAppMainWindow::loadAppState()
{
    auto file = getAppStateFile();
    if (file.existsAsFile())
    {
        juce::MemoryBlock data;
        if (file.loadFileAsData(data))
        {
            processor->setStateInformation(data.getData(), (int)data.getSize());
        }
    }
}

juce::File StandaloneAppMainWindow::getAppStateFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("ECMapper")
           .getChildFile("app_state.bin");
}

void StandaloneAppMainWindow::showAudioSettings()
{
    auto availableInputs = juce::MidiInput::getAvailableDevices();
    auto availableOutputs = getSelectableMidiOutputs();

    auto content = std::make_unique<StandaloneSettingsComponent>(
        availableInputs,
        availableOutputs,
        getConfiguredMidiInputId(),
        standaloneZoneOutputIds_,
        getRequestedBufferSize(),
        isMidi2ModeEnabled(),
        [this](const juce::String& midiInputId, const std::array<juce::String, 3>& zoneOutputIds, const int bufferSize)
        {
            standaloneMidiInputId_ = midiInputId;
            standaloneZoneOutputIds_ = zoneOutputIds;
            applyBufferSize(bufferSize);
            updateMidiInputs();
            updateMidiOutput();
            saveAudioSettings();
        });

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(content.release());
    options.dialogTitle = "Audio/MIDI Settings";
    options.dialogBackgroundColour = ecm::Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;
    options.launchAsync();
}

void StandaloneAppMainWindow::showPresetBrowser()
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ecm::PresetBrowserComponent(*processor));
    options.dialogTitle = "Presets";
    options.dialogBackgroundColour = ecm::Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = this;
    options.launchAsync();
}

void StandaloneAppMainWindow::showCalibrationDialog()
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new ecm::CalibrationDialogComponent(processor->state.state));
    options.dialogTitle = "Calibration";
    options.dialogBackgroundColour = ecm::Style::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;
    options.launchAsync();
}

void StandaloneAppMainWindow::requestQuit()
{
    if (auto* app = dynamic_cast<ECMapperStandaloneApplication*>(juce::JUCEApplication::getInstance()))
        app->allowQuitWithoutPromptOnce();

    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void StandaloneAppMainWindow::showAboutDialog()
{
    auto about = std::make_unique<AboutDialogComponent>(
        "ECMapper",
        ProjectInfo::versionString
    );

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(about.release());
    options.dialogTitle = "About ECMapper";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = this;

    options.launchAsync();
}

void StandaloneAppMainWindow::showOnlineManual()
{
    juce::ignoreUnused(juce::URL(AboutDialogComponent::onlineManualUrl).launchInDefaultBrowser());
}

void StandaloneAppMainWindow::showOurMusic()
{
    juce::ignoreUnused(juce::URL(AboutDialogComponent::ourMusicUrl).launchInDefaultBrowser());
}
