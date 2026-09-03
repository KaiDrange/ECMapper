#include "MainMenuBarModel.h"

MainMenuBarModel::MainMenuBarModel(Action onQuitAction, Action onSavePresetAction, Action onBrowsePresetsAction, Action onAudioSettingsAction, Action onAboutAction,
                                   Action onOnlineManualAction, Action onOurMusicAction)
    : onQuit(std::move(onQuitAction)),
      onSavePreset(std::move(onSavePresetAction)),
      onBrowsePresets(std::move(onBrowsePresetsAction)),
      onAudioSettings(std::move(onAudioSettingsAction)),
      onAbout(std::move(onAboutAction)),
      onOnlineManual(std::move(onOnlineManualAction)),
      onOurMusic(std::move(onOurMusicAction))
{
}

juce::StringArray MainMenuBarModel::getMenuBarNames()
{
    return { "File", "Options", "Help" };
}

juce::PopupMenu MainMenuBarModel::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName)
{
    juce::ignoreUnused(topLevelMenuIndex);
    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem(3, "Browse presets...");
        menu.addSeparator();
        menu.addItem(1, "Quit");
    }
    else if(menuName == "Options")
    {
        menu.addItem(20, "Audio/midi settings");
    }
    else if (menuName == "Help")
    {
        menu.addItem(30, "About ECMapper");
        menu.addSeparator();
        menu.addItem(31, "Online manual");
        menu.addItem(32, "Our music as Tic Tic");
    }

    return menu;
}

void MainMenuBarModel::menuItemSelected(const int menuItemID, int topLevelMenuIndex)
{
    juce::ignoreUnused(topLevelMenuIndex);

    if (menuItemID == 1 && onQuit)
        onQuit();
    else if (menuItemID == 2 && onSavePreset)
        onSavePreset();
    else if (menuItemID == 3 && onBrowsePresets)
        onBrowsePresets();
    else if (menuItemID == 20 && onAudioSettings)
        onAudioSettings();
    else if (menuItemID == 30 && onAbout)
        onAbout();
    else if (menuItemID == 31 && onOnlineManual)
        onOnlineManual();
    else if (menuItemID == 32 && onOurMusic)
        onOurMusic();
}
