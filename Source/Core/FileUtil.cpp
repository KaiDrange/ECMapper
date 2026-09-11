#include "FileUtil.h"
#include "LayoutWrapper.h"

namespace ecm {

juce::File FileUtil::getLayoutsRootDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("ECMapperLayouts");
}

juce::File FileUtil::getLayoutDirectory(InstrumentType instrumentType)
{
    return getLayoutsRootDirectory().getChildFile(getDeviceFolder(instrumentType));
}

void FileUtil::loadLayout(InstrumentType instrumentType, juce::ValueTree& rootState, juce::Component* /*parentComponent*/, std::function<void()> onFinished) {
    juce::File pathFile = getLayoutDirectory(instrumentType);
    
    auto chooser = std::make_shared<juce::FileChooser>("Open layout", pathFile, getFileExtension(instrumentType));
    
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [chooser, instrumentType, rootState, onFinished](const juce::FileChooser& fc) mutable {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                auto xml = file.loadFileAsString();
                auto loadedTree = juce::ValueTree::fromXml(xml);
                if (loadedTree.isValid()) {
                    auto layoutTree = LayoutWrapper::getLayoutTree(instrumentType, rootState);
                    layoutTree.copyPropertiesAndChildrenFrom(loadedTree, nullptr);
                    if (onFinished) onFinished();
                }
            }
        });
}

void FileUtil::saveLayout(InstrumentType instrumentType, juce::ValueTree& rootState, juce::Component* /*parentComponent*/) {
    juce::File pathFile = getLayoutDirectory(instrumentType);
    
    auto chooser = std::make_shared<juce::FileChooser>("Save layout", pathFile, getFileExtension(instrumentType));
    
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [chooser, instrumentType, rootState](const juce::FileChooser& fc) mutable {
            auto file = fc.getResult();
            if (file != juce::File()) {
                auto layoutTree = LayoutWrapper::createPersistentLayoutTree(instrumentType, rootState);
                if (auto xml = layoutTree.createXml()) {
                    xml->writeTo(file);
                }
            }
        });
}

juce::String FileUtil::getFileExtension(InstrumentType instrumentType) {
    switch (instrumentType) {
        case InstrumentType::Alpha: return "*.alphamap";
        case InstrumentType::Tau:   return "*.taumap";
        case InstrumentType::Pico:  return "*.picomap";
        default:                    return "";
    }
}

juce::String FileUtil::getDeviceFolder(InstrumentType instrumentType) {
    switch (instrumentType) {
        case InstrumentType::Alpha: return "Alpha";
        case InstrumentType::Tau:   return "Tau";
        case InstrumentType::Pico:  return "Pico";
        default:                    return "";
    }
}

} // namespace ecm
