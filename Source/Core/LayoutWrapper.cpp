#include "LayoutWrapper.h"
#include "SettingsWrapper.h"

namespace ecm {

namespace {

LayoutWrapper::KeyId canonicalizeTauKeyId(LayoutWrapper::KeyId keyId)
{
    if (keyId.deviceType != InstrumentType::Tau)
        return keyId;

    if (keyId.course == 0 && keyId.keyNo >= 72 && keyId.keyNo < 84)
        return { 1, keyId.keyNo - 72, keyId.deviceType };

    if (keyId.course == 1 && keyId.keyNo >= 88 && keyId.keyNo < 96)
        return { 2, keyId.keyNo - 88, keyId.deviceType };

    return keyId;
}

LayoutWrapper::KeyId canonicalizeTauKeyTreeId(juce::ValueTree keyTree)
{
    if (!keyTree.isValid())
        return { 0, 0, InstrumentType::None };

    const auto deviceType = LayoutWrapper::getInstrumentTypeFromKeyTree(keyTree);
    auto typeStr = keyTree.getType().toString();
    auto parts = juce::StringArray::fromTokens(typeStr, "_", "");
    const int course = (parts.size() > 1) ? parts[1].getIntValue() : 0;
    const int keyNo = (parts.size() > 2) ? parts[2].getIntValue() : 0;

    const LayoutWrapper::KeyId keyId {
        .course = course,
        .keyNo = keyNo,
        .deviceType = deviceType
    };

    auto canonicalKeyId = canonicalizeTauKeyId(keyId);
    if (canonicalKeyId != keyId)
        return canonicalKeyId;

    if (deviceType != InstrumentType::Tau)
        return keyId;

    const auto defaultKeyType = course == 2 ? EigenharpKeyType::Button : EigenharpKeyType::Perc;
    const auto storedKeyType = static_cast<EigenharpKeyType>(int(keyTree.getProperty(LayoutWrapper::id_keyType,
                                                                                     (int)defaultKeyType)));
    if (course == 1 && keyNo >= 5 && keyNo < 13 && storedKeyType == EigenharpKeyType::Button)
        return { 2, keyNo - 5, deviceType };

    return keyId;
}

juce::String getKeyNodeName(LayoutWrapper::KeyId keyId)
{
    return LayoutWrapper::id_key.toString() + "_" + juce::String(keyId.course) + "_" + juce::String(keyId.keyNo);
}

juce::ValueTree renameKeyTree(juce::ValueTree keyTree, const juce::String& newType)
{
    juce::ValueTree renamedKeyTree(newType);
    renamedKeyTree.copyPropertiesFrom(keyTree, nullptr);

    for (int i = 0; i < keyTree.getNumChildren(); ++i)
        renamedKeyTree.addChild(keyTree.getChild(i).createCopy(), -1, nullptr);

    return renamedKeyTree;
}

juce::ValueTree getLegacyTauKeyTree(LayoutWrapper::KeyId canonicalKeyId, juce::ValueTree layoutTree)
{
    if (canonicalKeyId.deviceType != InstrumentType::Tau)
        return {};

    if (canonicalKeyId.course == 1 && canonicalKeyId.keyNo >= 0 && canonicalKeyId.keyNo < 12) {
        auto legacyPerc = layoutTree.getChildWithName(getKeyNodeName({ 0, canonicalKeyId.keyNo + 72, canonicalKeyId.deviceType }));
        if (legacyPerc.isValid())
            return legacyPerc;
    }

    if (canonicalKeyId.course == 2 && canonicalKeyId.keyNo >= 0 && canonicalKeyId.keyNo < 8) {
        auto legacyButtons = layoutTree.getChildWithName(getKeyNodeName({ 1, canonicalKeyId.keyNo + 5, canonicalKeyId.deviceType }));
        if (legacyButtons.isValid()) {
            const auto storedKeyType = static_cast<EigenharpKeyType>(int(legacyButtons.getProperty(LayoutWrapper::id_keyType,
                                                                                                   (int)EigenharpKeyType::Perc)));
            if (storedKeyType == EigenharpKeyType::Button)
                return legacyButtons;
        }

        legacyButtons = layoutTree.getChildWithName(getKeyNodeName({ 1, canonicalKeyId.keyNo + 88, canonicalKeyId.deviceType }));
        if (legacyButtons.isValid())
            return legacyButtons;
    }

    return {};
}

juce::ValueTree getExistingKeyTree(LayoutWrapper::KeyId keyId, juce::ValueTree& rootState)
{
    auto canonicalKeyId = canonicalizeTauKeyId(keyId);
    auto layoutTree = LayoutWrapper::getLayoutTree(canonicalKeyId.deviceType, rootState);
    auto keyTree = layoutTree.getChildWithName(getKeyNodeName(canonicalKeyId));

    if (keyTree.isValid())
        return keyTree;

    auto legacyKeyTree = getLegacyTauKeyTree(canonicalKeyId, layoutTree);
    if (!legacyKeyTree.isValid())
        return {};

    auto migratedKeyTree = renameKeyTree(legacyKeyTree, getKeyNodeName(canonicalKeyId));
    layoutTree.addChild(migratedKeyTree, -1, nullptr);
    layoutTree.removeChild(legacyKeyTree, nullptr);
    return layoutTree.getChildWithName(getKeyNodeName(canonicalKeyId));
}

void normalizeTauLayoutTree(juce::ValueTree& layoutTree)
{
    if (!layoutTree.isValid())
        return;

    for (int i = 0; i < layoutTree.getNumChildren(); ++i) {
        auto keyTree = layoutTree.getChild(i);
        const auto deviceType = LayoutWrapper::getInstrumentTypeFromKeyTree(keyTree);
        auto typeStr = keyTree.getType().toString();
        auto parts = juce::StringArray::fromTokens(typeStr, "_", "");
        LayoutWrapper::KeyId keyId {
            .course = (parts.size() > 1) ? parts[1].getIntValue() : 0,
            .keyNo = (parts.size() > 2) ? parts[2].getIntValue() : 0,
            .deviceType = deviceType
        };
        auto canonicalKeyId = canonicalizeTauKeyTreeId(keyTree);
        if (canonicalKeyId == keyId)
            continue;

        auto canonicalNodeName = getKeyNodeName(canonicalKeyId);
        if (!layoutTree.getChildWithName(canonicalNodeName).isValid())
            layoutTree.addChild(renameKeyTree(keyTree, canonicalNodeName), -1, nullptr);

        layoutTree.removeChild(keyTree, nullptr);
        --i;
    }
}

} // namespace

juce::ValueTree LayoutWrapper::getDeviceTree(InstrumentType deviceType, juce::ValueTree& rootState, bool create) {
    auto presetTree = SettingsWrapper::getPresetTree(rootState);
    auto deviceName = id_device + juce::String((int)deviceType);
    return create ? presetTree.getOrCreateChildWithName(deviceName, nullptr)
                  : presetTree.getChildWithName(deviceName);
}

juce::ValueTree LayoutWrapper::getLayoutTree(InstrumentType deviceType, juce::ValueTree& rootState) {
    auto deviceChild = getDeviceTree(deviceType, rootState);
    return deviceChild.getOrCreateChildWithName(id_layout, nullptr);
}

juce::ValueTree LayoutWrapper::createPersistentLayoutTree(InstrumentType deviceType, juce::ValueTree& rootState) {
    auto layoutTree = getLayoutTree(deviceType, rootState).createCopy();
    if (deviceType == InstrumentType::Tau)
        normalizeTauLayoutTree(layoutTree);
    layoutTree.setProperty(id_ecMapperVersion, ProjectInfo::versionString, nullptr);
    return layoutTree;
}

juce::ValueTree LayoutWrapper::getKeyTree(KeyId keyId, juce::ValueTree& rootState) {
    auto canonicalKeyId = canonicalizeTauKeyId(keyId);
    auto existingKeyTree = getExistingKeyTree(canonicalKeyId, rootState);
    if (existingKeyTree.isValid())
        return existingKeyTree;

    auto layoutTree = getLayoutTree(canonicalKeyId.deviceType, rootState);
    return layoutTree.getOrCreateChildWithName(getKeyNodeName(canonicalKeyId), nullptr);
}

void LayoutWrapper::addListener(InstrumentType deviceType, juce::ValueTree::Listener* listener, juce::ValueTree& rootState) {
    auto vTree = getLayoutTree(deviceType, rootState);
    vTree.addListener(listener);
}

void LayoutWrapper::clearLayout(InstrumentType deviceType, juce::ValueTree& rootState) {
    auto layoutTree = getLayoutTree(deviceType, rootState);
    layoutTree.removeAllChildren(nullptr);
    layoutTree.removeAllProperties(nullptr);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKey(KeyId keyId, juce::ValueTree& rootState) {
    auto canonicalKeyId = canonicalizeTauKeyId(keyId);
    auto defaultKeyType = getCorrectDefaultKeyType(canonicalKeyId.deviceType, canonicalKeyId.course, canonicalKeyId.keyNo);
    auto keyTree = getExistingKeyTree(canonicalKeyId, rootState);
    
    return LayoutKey {
        .keyId = canonicalKeyId,
        .keyType = (EigenharpKeyType)int(keyTree.getProperty(id_keyType, (int)defaultKeyType)),
        .keyColour = (KeyColour)int(keyTree.getProperty(id_keyColour, (int)KeyColour::Off)),
        .zone = (Zone)int(keyTree.getProperty(id_zone, (int)Zone::Zone1)),
        .keyMappingType = (KeyMappingType)int(keyTree.getProperty(id_keyMappingType, (int)getDefaultMappingTypeFromKeyType(defaultKeyType))),
        .mappingValue = keyTree.getProperty(id_mappingValue, defaultKeyType == EigenharpKeyType::Normal ? "0" : "")
    };
}

void LayoutWrapper::setLayoutKey(LayoutKey& key, juce::ValueTree& rootState) {
    setKeyColour(key.keyId, key.keyColour, rootState);
    setKeyType(key.keyId, key.keyType, rootState);
    setKeyZone(key.keyId, key.zone, rootState);
    setKeyMappingType(key.keyId, key.keyMappingType, rootState);
    setKeyMappingValue(key.keyId, key.mappingValue, rootState);
}

void LayoutWrapper::setKeyColour(KeyId keyId, KeyColour keyColour, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyColour, (int)keyColour, nullptr);
}

void LayoutWrapper::setKeyType(KeyId keyId, EigenharpKeyType keyType, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyType, (int)keyType, nullptr);
}

void LayoutWrapper::setKeyZone(KeyId keyId, Zone zone, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_zone, (int)zone, nullptr);
}

void LayoutWrapper::setKeyMappingType(KeyId keyId, KeyMappingType keyMappingType, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_keyMappingType, (int)keyMappingType, nullptr);
}

void LayoutWrapper::setKeyMappingValue(KeyId keyId, juce::String keyMappingValue, juce::ValueTree& rootState) {
    auto keyTree = getKeyTree(keyId, rootState);
    keyTree.setProperty(id_mappingValue, keyMappingValue, nullptr);
}

LayoutWrapper::LayoutKey LayoutWrapper::getLayoutKeyFromKeyTree(juce::ValueTree keyTree) {
    if (!keyTree.isValid())
        return LayoutKey { .keyId = {0,0,InstrumentType::None}, .keyType = EigenharpKeyType::Normal, .keyColour = KeyColour::Off, .zone = Zone::Zone1, .keyMappingType = KeyMappingType::Note, .mappingValue = "0" };
    
    InstrumentType deviceType = getInstrumentTypeFromKeyTree(keyTree);
    
    auto typeStr = keyTree.getType().toString();
    auto parts = juce::StringArray::fromTokens(typeStr, "_", "");
    int course = (parts.size() > 1) ? parts[1].getIntValue() : 0;
    int keyNo = (parts.size() > 2) ? parts[2].getIntValue() : 0;
    
    KeyId keyId = {
        .course = course,
        .keyNo = keyNo,
        .deviceType = deviceType
    };
    
    auto rootState = keyTree.getRoot();
    return getLayoutKey(keyId, rootState);
}

InstrumentType LayoutWrapper::getInstrumentTypeFromKeyTree(juce::ValueTree keyTree) {
    if (!keyTree.isValid() || !keyTree.getParent().isValid() || !keyTree.getParent().getParent().isValid())
        return InstrumentType::None;

    return (InstrumentType)keyTree.getParent().getParent().getType().toString().substring(6).getIntValue();
}

InstrumentType LayoutWrapper::getInstrumentTypeFromLayoutTree(juce::ValueTree layoutTree) {
    if (!layoutTree.isValid() || !layoutTree.getParent().isValid())
        return InstrumentType::None;

    return (InstrumentType)layoutTree.getParent().getType().toString().substring(6).getIntValue();
}

EigenharpKeyType LayoutWrapper::getCorrectDefaultKeyType(InstrumentType deviceType, int course, int keyNo) {
    switch (deviceType) {
        case InstrumentType::Alpha:
            return course == 0 ? EigenharpKeyType::Normal : EigenharpKeyType::Perc;
        case InstrumentType::Tau:
            if (course == 0 && keyNo < 72) return EigenharpKeyType::Normal;
            if (course == 1 && keyNo < 12) return EigenharpKeyType::Perc;
            if (course == 2 && keyNo < 8) return EigenharpKeyType::Button;
            return EigenharpKeyType::Normal;
        case InstrumentType::Pico:
            return course == 0 ? EigenharpKeyType::Normal : EigenharpKeyType::Button;
        default:
            return EigenharpKeyType::Normal;
    }
}

KeyMappingType LayoutWrapper::getDefaultMappingTypeFromKeyType(EigenharpKeyType keyType) {
    return keyType == EigenharpKeyType::Normal ? KeyMappingType::Note : KeyMappingType::None;
}

} // namespace ecm
