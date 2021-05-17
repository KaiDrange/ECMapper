#include "FileUtil.h"


juce::String FileUtil::openMapping(DeviceType::DeviceType instrumentType) {
    juce::File pathFile("./Mappings/");
    
    juce::FileChooser fileChooser("Open mapping", pathFile, getFileExtension(instrumentType));
    if (fileChooser.browseForFileToOpen()) {
        auto file = fileChooser.getResult();
        if (!file.existsAsFile())
            return "";
        
        return file.loadFileAsString();
    }
    return "";
}

bool FileUtil::saveMapping(juce::ValueTree valueTree, DeviceType::DeviceType instrumentType) {
    auto success = false;
    juce::File pathFile("./Mappings/");
    juce::FileChooser fileChooser("Save mapping", pathFile, getFileExtension(instrumentType));
    if (fileChooser.browseForFileToSave(true)) {
        auto file = fileChooser.getResult();
        valueTree.createXml()->writeTo(file);
    }
    return success;
}

juce::String FileUtil::getFileExtension(DeviceType::DeviceType instrumentType) {
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
    
