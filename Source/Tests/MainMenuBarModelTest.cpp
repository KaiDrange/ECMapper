#include "UI/MainMenuBarModel.h"
#include "UI/InputMidiReferenceText.h"

#include <iostream>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << std::endl;
        return false;
    }

    return true;
}

bool menuContainsItemWithText(const juce::PopupMenu& menu, const int itemId, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator iterator(menu); iterator.next();)
    {
        const auto& item = iterator.getItem();
        if (item.itemID == itemId)
            return item.text == text;
    }

    return false;
}

} // namespace

int main()
{
    bool inputMidiReferenceTriggered = false;
    MainMenuBarModel model(
        {},
        {},
        {},
        {},
        {},
        {},
        [&] { inputMidiReferenceTriggered = true; },
        {},
        {});

    const auto helpMenu = model.getMenuForIndex(2, "Help");
    const auto referenceText = ecm::getInputMidiReferenceText();

    bool ok = true;
    ok &= expect(menuContainsItemWithText(helpMenu, 33, "Input Midi reference"),
                 "help menu should include the Input Midi reference entry");
    ok &= expect(referenceText.contains("Program Change 0-15") && referenceText.contains("preset slot 1-16"),
                 "input MIDI reference should explain preset slot program changes");
    ok &= expect(referenceText.contains("CC 22") && referenceText.contains("CC 23") && referenceText.contains("CC 24")
                 && referenceText.contains("channel 1") && referenceText.contains("channel 2 targets Alpha")
                 && referenceText.contains("Value 64 = 0 semitones")
                 && referenceText.contains("1 semitone") && referenceText.contains("-64") && referenceText.contains("+63"),
                 "input MIDI reference should explain transpose controller handling");
    ok &= expect(referenceText.contains("CC 25") && referenceText.contains("CC 26") && referenceText.contains("CC 27")
                 && referenceText.contains("0-63 = off") && referenceText.contains("64-127 = on")
                 && referenceText.contains("channels 1-4 are all accepted"),
                 "input MIDI reference should explain zone enable controller handling");

    model.menuItemSelected(33, 2);
    ok &= expect(inputMidiReferenceTriggered,
                 "selecting the Input Midi reference entry should trigger its action");

    if (!ok)
        return 1;

    std::cout << "MainMenuBarModelTest passed" << std::endl;
    return 0;
}