#include "PresetBankFileUtil.h"

#include "SettingsWrapper.h"

namespace ecm {

namespace {

juce::ValueTree extractPresetBankTree(const juce::ValueTree& tree, bool& wasStateBundle)
{
    wasStateBundle = false;

    if (!tree.isValid())
        return {};

    if (tree.hasType("ECMapperPresetBank"))
        return tree;

    if (tree.hasType("ECMapperStateBundle")) {
        wasStateBundle = true;
        return tree.getChildWithName("ECMapperPresetBank");
    }

    return {};
}

}

juce::ValueTree PresetBankFileUtil::createExportTree(const juce::ValueTree& presetBankState)
{
    auto bankCopy = presetBankState.createCopy();
    normalizePresetBankState(bankCopy);
    return bankCopy;
}

bool PresetBankFileUtil::writePresetBankFile(const juce::File& file, const juce::ValueTree& presetBankState)
{
    if (file == juce::File())
        return false;

    const auto parent = file.getParentDirectory();
    if (!parent.exists() && !parent.createDirectory())
        return false;

    const auto tree = createExportTree(presetBankState);
    const std::unique_ptr xml(tree.createXml());
    if (xml == nullptr)
        return false;

    return xml->writeTo(file);
}

PresetBankImportResult PresetBankFileUtil::readPresetBankFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};

    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return {};

    return readPresetBankTree(juce::ValueTree::fromXml(*xml));
}

PresetBankImportResult PresetBankFileUtil::readPresetBankTree(const juce::ValueTree& tree)
{
    PresetBankImportResult result;
    auto extractedTree = extractPresetBankTree(tree, result.wasStateBundle);
    if (!extractedTree.isValid())
        return result;

    result.presetBank = extractedTree.createCopy();
    normalizePresetBankState(result.presetBank);
    return result;
}

void PresetBankFileUtil::normalizePresetBankState(juce::ValueTree& presetBankState)
{
    for (int i = 0; i < presetBankState.getNumChildren(); ++i) {
        auto preset = presetBankState.getChild(i);
        auto snapshot = preset.getNumChildren() > 0 ? preset.getChild(0) : juce::ValueTree();
        if (snapshot.isValid())
            SettingsWrapper::normalizeStateTree(snapshot);
    }
}

} // namespace ecm