#include "PresetBrowserComponent.h"
#include "../PluginProcessor.h"

namespace ecm {

namespace {

void configurePresetActionButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Style::surfaceRaised());
    button.setColour(juce::TextButton::buttonOnColourId, Style::accent());
    button.setColour(juce::TextButton::textColourOffId, Style::text());
    button.setColour(juce::TextButton::textColourOnId, Style::background());
}

void configureDeleteButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Style::danger());
    button.setColour(juce::TextButton::buttonOnColourId, Style::danger().brighter(0.2f));
    button.setColour(juce::TextButton::textColourOffId, Style::text());
    button.setColour(juce::TextButton::textColourOnId, Style::text());
}

void configureSaveButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, Style::accent().darker(0.5f));
    button.setColour(juce::TextButton::buttonOnColourId, Style::accent().darker(0.3f));
    button.setColour(juce::TextButton::textColourOffId, Style::text());
    button.setColour(juce::TextButton::textColourOnId, Style::text());
}

}

PresetBrowserComponent::PresetBrowserComponent(ECMapperAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setOpaque(true);
    setSize(860, 620);
    selectedSlot = processor.getCurrentPresetSlot();

    headerLabel.setText("Presets", juce::dontSendNotification);
    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setColour(juce::Label::textColourId, Style::text());
    addAndMakeVisible(headerLabel);

    for (int slot = 1; slot <= slotCount; ++slot)
    {
        auto index = slot - 1;

        slotButtons[(size_t) index].setClickingTogglesState(false);
        configurePresetActionButton(slotButtons[(size_t) index]);
        slotButtons[(size_t) index].onClick = [this, slot] { handleSlotSelected(slot); };
        addAndMakeVisible(slotButtons[(size_t) index]);

        configureSaveButton(saveButtons[(size_t) index]);
        saveButtons[(size_t) index].setButtonText(""); // We will draw the icon in LookAndFeel
        saveButtons[(size_t) index].getProperties().set("isSaveButton", true);
        saveButtons[(size_t) index].onClick = [this, slot] { requestSaveToSlot(slot); };
        addAndMakeVisible(saveButtons[(size_t) index]);

        configureDeleteButton(deleteButtons[(size_t) index]);
        deleteButtons[(size_t) index].setButtonText("X");
        deleteButtons[(size_t) index].onClick = [this, slot] { requestDeleteSlot(slot); };
        addAndMakeVisible(deleteButtons[(size_t) index]);
    }

    refreshSlots();
}

void PresetBrowserComponent::paint(juce::Graphics& g)
{
    g.fillAll(Style::background());
}

void PresetBrowserComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    headerLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);

    auto columnWidth = (area.getWidth() - 12) / 2;
    auto rowHeight = 28;
    auto columnGap = 12;

    for (int column = 0; column < 2; ++column)
    {
        auto columnArea = area.withTrimmedLeft(column == 0 ? 0 : columnWidth + columnGap);
        columnArea.setWidth(columnWidth);

        for (int row = 0; row < rowsPerColumn; ++row)
        {
            auto slot = column * rowsPerColumn + row + 1;
            auto rowArea = columnArea.removeFromTop(rowHeight);

            auto deleteWidth = 36;
            auto saveWidth = 36;
            deleteButtons[(size_t) (slot - 1)].setBounds(rowArea.removeFromRight(deleteWidth));
            rowArea.removeFromRight(4);
            saveButtons[(size_t) (slot - 1)].setBounds(rowArea.removeFromRight(saveWidth));
            rowArea.removeFromRight(6);
            slotButtons[(size_t) (slot - 1)].setBounds(rowArea);
        }
    }
}

void PresetBrowserComponent::refreshSlots()
{
    auto currentSlot = processor.getCurrentPresetSlot();
    selectedSlot = juce::jlimit(1, slotCount, currentSlot);

    for (int slot = 1; slot <= slotCount; ++slot)
    {
        auto index = slot - 1;
        auto displayName = processor.getPresetSlotDisplayName(slot);
        auto isCurrent = (slot == selectedSlot);

        slotButtons[(size_t) index].setButtonText(displayName);
        slotButtons[(size_t) index].setToggleState(isCurrent, juce::dontSendNotification);
        slotButtons[(size_t) index].setEnabled(processor.hasPresetSlot(slot));

        saveButtons[(size_t) index].setEnabled(true);

        auto deleteEnabled = processor.hasPresetSlot(slot);
        deleteButtons[(size_t) index].setEnabled(deleteEnabled);
    }

    repaint();
}

void PresetBrowserComponent::handleSlotSelected(int slot)
{
    if (slot < 1 || slot > slotCount)
        return;

    if (!processor.hasPresetSlot(slot))
        return;

    closeDialog();
    juce::MessageManager::callAsync([processor = &processor, slot]
    {
        processor->loadPresetSlot(slot);
    });
}

void PresetBrowserComponent::requestDeleteSlot(int slot)
{
    if (slot < 1 || slot > slotCount)
        return;

    if (!processor.hasPresetSlot(slot))
        return;

    auto safeThis = juce::Component::SafePointer<PresetBrowserComponent>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Delete Preset?")
            .withMessage("Delete preset " + juce::String(slot) + "?")
            .withButton("Delete")
            .withButton("Cancel"),
        [safeThis, slot](int result)
        {
            if (result == 0 || safeThis == nullptr)
                return;

            if (safeThis->processor.deletePresetSlot(slot))
                safeThis->refreshSlots();
        });
}

void PresetBrowserComponent::requestSaveToSlot(int slot)
{
    if (slot < 1 || slot > slotCount)
        return;

    auto currentName = processor.hasPresetSlot(slot) ? processor.getPresetNode(slot).getProperty("name", "").toString() 
                                                     : processor.getCurrentPresetName();
    if (currentName.isEmpty())
        currentName = "Preset " + juce::String(slot);

    auto safeThis = juce::Component::SafePointer<PresetBrowserComponent>(this);
    
    auto* alert = new juce::AlertWindow("Save Preset", "Enter name for preset " + juce::String(slot), juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("name", currentName, "Preset name");
    alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    alert->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, slot, alert](int result) {
        if (result == 1 && safeThis != nullptr)
        {
            auto name = alert->getTextEditorContents("name").trim();
            if (name.isEmpty())
                name = "Preset " + juce::String(slot);
            
            safeThis->savePresetToSlot(slot, name);
        }
        delete alert;
    }), true);
}

void PresetBrowserComponent::savePresetToSlot(int slot, const juce::String& name)
{
    if (slot < 1 || slot > slotCount)
        return;

    auto saveNow = [this, slot, name]
    {
        if (processor.savePresetSlot(slot, name))
        {
            refreshSlots();
        }
    };

    if (processor.hasPresetSlot(slot))
    {
        auto safeThis = juce::Component::SafePointer<PresetBrowserComponent>(this);
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Overwrite Preset?")
                .withMessage("Preset " + juce::String(slot) + " already exists. Overwrite it?")
                .withButton("Overwrite")
                .withButton("Cancel"),
            [safeThis, saveNow](int result)
            {
                if (result == 0 || safeThis == nullptr)
                    return;

                saveNow();
            });
        return;
    }

    saveNow();
}

void PresetBrowserComponent::closeDialog()
{
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState(0);
    else if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->exitModalState(0);
}

} // namespace ecm
