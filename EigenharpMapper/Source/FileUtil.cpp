#include "FileUtil.h"


juce::String FileUtil::openMapping(InstrumentType instrumentType) {
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

bool FileUtil::saveMapping(juce::ValueTree valueTree, InstrumentType instrumentType) {
    auto success = false;
    juce::File pathFile("./Mappings/");
    juce::FileChooser fileChooser("Save mapping", pathFile, getFileExtension(instrumentType));
    if (fileChooser.browseForFileToSave(true)) {
        auto file = fileChooser.getResult();
        valueTree.createXml()->writeTo(file);
    }
    return success;
}

juce::String FileUtil::getFileExtension(InstrumentType instrumentType) {
    juce::String fileExtension;
    switch (instrumentType) {
        case InstrumentType::Alpha:
            fileExtension = "*.alphamap";
            break;
        case InstrumentType::Tau:
            fileExtension = "*.taumap";
            break;
        case InstrumentType::Pico:
            fileExtension = "*.picomap";
            break;
        default:
            break;
    }
    return fileExtension;
}
    
