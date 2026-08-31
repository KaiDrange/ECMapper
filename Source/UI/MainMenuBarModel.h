#pragma once

#include <JuceHeader.h>

class MainMenuBarModel : public juce::MenuBarModel
{
public:
    using Action = std::function<void()>;

    MainMenuBarModel(Action onQuitAction,
                     Action onAudioSettingsAction, Action onAboutAction = {},
                     Action onOnlineManualAction = {}, Action onOurMusicAction = {});
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    Action onQuit;
    Action onAudioSettings;
    Action onAbout;
    Action onOnlineManual;
    Action onOurMusic;
};
