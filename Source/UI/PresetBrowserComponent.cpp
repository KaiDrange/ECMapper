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
    button.setColour(juce::TextButton::textColourOffId, Style::background());
    button.setColour(juce::TextButton::textColourOnId, Style::background());
}

}

PresetBrowserComponent::PresetBrowserComponent(ECMapperAudioProcessor& processorToUse, Mode modeToUse)
    : processor(processorToUse),
      mode(modeToUse)
{
    setOpaque(true);
    setSize(860, 620);
    if (mode == Mode::Save)
        selectedSlot = processor.getCurrentPresetSlot();

    headerLabel.setText(mode == Mode::Save ? "Save preset" : "Presets", juce::dontSendNotification);
    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setColour(juce::Label::textColourId, Style::text());
    addAndMakeVisible(headerLabel);

    if (mode == Mode::Save)
    {
        presetNameEditor.setText(processor.getCurrentPresetName());
        presetNameEditor.setTextToShowWhenEmpty("Preset name", Style::mutedText());
        addAndMakeVisible(presetNameEditor);

        saveButton.onClick = [this] { requestSave(); };
        configurePresetActionButton(saveButton);
        addAndMakeVisible(saveButton);
    }

    for (int slot = 1; slot <= slotCount; ++slot)
    {
        auto index = slot - 1;

        slotButtons[(size_t) index].setClickingTogglesState(false);
        configurePresetActionButton(slotButtons[(size_t) index]);
        slotButtons[(size_t) index].onClick = [this, slot] { handleSlotSelected(slot); };
        addAndMakeVisible(slotButtons[(size_t) index]);

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

    if (mode == Mode::Save)
    {
        auto header = area.removeFromTop(34);
        headerLabel.setBounds(header.removeFromLeft(120));
        header.removeFromLeft(8);
        saveButton.setBounds(header.removeFromRight(92));
        header.removeFromRight(8);
        presetNameEditor.setBounds(header);
        area.removeFromTop(10);
    }
    else
    {
        headerLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
    }

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

            auto deleteWidth = 28;
            deleteButtons[(size_t) (slot - 1)].setBounds(rowArea.removeFromRight(deleteWidth));
            rowArea.removeFromRight(6);
            slotButtons[(size_t) (slot - 1)].setBounds(rowArea);
        }
    }
}

void PresetBrowserComponent::refreshSlots()
{
    auto currentSlot = processor.getCurrentPresetSlot();
    if (mode == Mode::Save)
        selectedSlot = juce::jlimit(1, slotCount, selectedSlot);
    else
        selectedSlot = juce::jlimit(1, slotCount, currentSlot);

    for (int slot = 1; slot <= slotCount; ++slot)
    {
        auto index = slot - 1;
        auto displayName = processor.getPresetSlotDisplayName(slot);
        auto isCurrent = (slot == selectedSlot);

        slotButtons[(size_t) index].setButtonText(displayName);
        slotButtons[(size_t) index].setToggleState(isCurrent, juce::dontSendNotification);

        auto deleteEnabled = processor.hasPresetSlot(slot);
        deleteButtons[(size_t) index].setEnabled(deleteEnabled);
    }

    repaint();
}

void PresetBrowserComponent::handleSlotSelected(int slot)
{
    if (slot < 1 || slot > slotCount)
        return;

    if (mode == Mode::Save)
    {
        selectedSlot = slot;
        refreshSlots();
        return;
    }

    if (processor.loadPresetSlot(slot))
    {
        refreshSlots();
    }
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

void PresetBrowserComponent::requestSave()
{
    auto name = presetNameEditor.getText().trim();
    if (name.isEmpty())
        name = "Preset " + juce::String(selectedSlot);

    savePresetToSlot(selectedSlot, name);
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

} // namespace ecm
