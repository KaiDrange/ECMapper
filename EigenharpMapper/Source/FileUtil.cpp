#include "FileUtil.h"


juce::String FileUtil::openMapping() {
    juce::File pathFile("./");
    juce::FileChooser fileChooser("Open mapping", pathFile, "*.mapping");
    if (fileChooser.browseForFileToOpen()) {
        auto file = fileChooser.getResult();
        if (!file.existsAsFile())
            return "";
        
        return file.loadFileAsString();
    }
    return "";
}

bool FileUtil::saveMapping(juce::ValueTree valueTree) {
    auto success = false;
    juce::File pathFile("./Mappings/");
    juce::FileChooser fileChooser("Save mapping", pathFile, "*.mapping");
    if (fileChooser.browseForFileToSave(true)) {
        auto file = fileChooser.getResult();
        valueTree.createXml()->writeTo(file);
    }
    return success;
}
    
