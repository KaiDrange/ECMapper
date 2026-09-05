#pragma once

#include <JuceHeader.h>
#include <functional>

class MainMenuBarModel : public juce::MenuBarModel
{
public:
    using Action = std::function<void()>;

    MainMenuBarModel(Action onQuitAction,
                     Action onSavePresetAction,
                     Action onBrowsePresetsAction,
                     Action onAudioSettingsAction, Action onMidiMonitorAction = {}, Action onAboutAction = {},
                     Action onOnlineManualAction = {}, Action onOurMusicAction = {});
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    Action onQuit;
    Action onSavePreset;
    Action onBrowsePresets;
    Action onAudioSettings;
    Action onMidiMonitor;
    Action onAbout;
    Action onOnlineManual;
    Action onOurMusic;
};
