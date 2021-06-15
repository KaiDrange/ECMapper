#include "FileUtil.h"


juce::String FileUtil::openMapping(DeviceType instrumentType) {
    juce::File pathFile("~/Documents/ECMapperLayouts/" + getDeviceFolder(instrumentType) + "/");
    
    juce::FileChooser fileChooser("Open layout", pathFile, getFileExtension(instrumentType));
    if (fileChooser.browseForFileToOpen()) {
        auto file = fileChooser.getResult();
        if (!file.existsAsFile())
            return "";
        
        return file.loadFileAsString();
    }
    return "";
}

bool FileUtil::saveMapping(juce::ValueTree valueTree, DeviceType instrumentType) {
    auto success = false;
    juce::File pathFile("~/Documents/ECMapperLayouts/" + getDeviceFolder(instrumentType) + "/");
    juce::FileChooser fileChooser("Save layout", pathFile, getFileExtension(instrumentType));
    if (fileChooser.browseForFileToSave(true)) {
        auto file = fileChooser.getResult();
        valueTree.createXml()->writeTo(file);
    }
    return success;
}

juce::String FileUtil::getFileExtension(DeviceType instrumentType) {
    juce::String fileExtension;
    switch (instrumentType) {
        case DeviceType::Alpha:
            fileExtension = "*.alphamap";
            break;
        case DeviceType::Tau:
            fileExtension = "*.taumap";
            break;
        case DeviceType::Pico:
            fileExtension = "*.picomap";
            break;
        default:
            break;
    }
    return fileExtension;
}

juce::String FileUtil::getDeviceFolder(DeviceType instrumentType) {
    juce::String folderName;
    switch (instrumentType) {
        case DeviceType::Alpha:
            folderName = "Alpha";
            break;
        case DeviceType::Tau:
            folderName = "Tau";
            break;
        case DeviceType::Pico:
            folderName = "Pico";
            break;
        default:
            break;
    }
    return folderName;
}
    
