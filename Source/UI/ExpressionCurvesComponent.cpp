#include "ExpressionCurvesComponent.h"
#include "../Core/SettingsWrapper.h"

#include <array>

namespace ecm {

namespace {

juce::String labelForTarget(ExpressionCurveTarget target)
{
    switch (target) {
        case ExpressionCurveTarget::Breath: return "Breath";
        case ExpressionCurveTarget::Velocity: return "Velocity";
        case ExpressionCurveTarget::ReleaseVelocity: return "Release velocity";
        case ExpressionCurveTarget::Pressure: return "Pressure";
        case ExpressionCurveTarget::Yaw: return "Yaw";
        case ExpressionCurveTarget::Roll: return "Roll";
        default: return "";
    }
}

} // namespace

ExpressionCurvesComponent::ExpressionCurvesComponent(InstrumentType deviceType, juce::AudioProcessorValueTreeState& pluginState, MidiService& midiService)
    : deviceType_(deviceType), pluginState_(pluginState)
{
    showLiveDots.setToggleState(SettingsWrapper::getShowLiveDots(deviceType_, pluginState_.state), juce::dontSendNotification);
    showLiveDots.setTooltip("Show live expression values on the curves while playing.");
    showLiveDots.onClick = [this] {
        SettingsWrapper::setShowLiveDots(deviceType_, showLiveDots.getToggleState(), pluginState_.state);
        for (auto& editor : editors)
            editor->setPerformanceDotsEnabled(showLiveDots.getToggleState());
    };
    addAndMakeVisible(showLiveDots);
    const std::array<ExpressionCurveTarget, 6> targets {
        ExpressionCurveTarget::Breath,
        ExpressionCurveTarget::Velocity,
        ExpressionCurveTarget::ReleaseVelocity,
        ExpressionCurveTarget::Pressure,
        ExpressionCurveTarget::Yaw,
        ExpressionCurveTarget::Roll
    };

    for (int i = 0; i < 6; ++i) {
        editors[i] = std::make_unique<ExpressionCurveEditorComponent>(deviceType, targets[(size_t)i], pluginState, midiService, labelForTarget(targets[(size_t)i]));
        editors[i]->setPerformanceDotsEnabled(showLiveDots.getToggleState());
        addAndMakeVisible(editors[i].get());
    }
}

void ExpressionCurvesComponent::resized() {
    auto area = getLocalBounds();
    showLiveDots.setBounds(area.removeFromTop(26).removeFromLeft(150));
    area.removeFromTop(4);
    auto gap = 6;
    auto cols = 3;
    auto rows = 2;
    auto cellW = (area.getWidth() - gap * (cols - 1)) / cols;
    auto cellH = (area.getHeight() - gap * (rows - 1)) / rows;
    auto cellSize = juce::jmin(cellW, cellH);

    for (int i = 0; i < 6; ++i) {
        int col = i % cols;
        int row = i / cols;
        auto cellX = area.getX() + col * (cellSize + gap);
        auto cellY = area.getY() + row * (cellSize + gap);
        editors[i]->setBounds(cellX, cellY, cellSize, cellSize);
    }
}

void ExpressionCurvesComponent::refreshFromState()
{
    const auto enabled = SettingsWrapper::getShowLiveDots(deviceType_, pluginState_.state);
    if (enabled != showLiveDots.getToggleState()) {
        showLiveDots.setToggleState(enabled, juce::dontSendNotification);
        for (auto& editor : editors)
            editor->setPerformanceDotsEnabled(enabled);
    }
    for (auto& editor : editors) {
        if (editor != nullptr)
            editor->refreshFromState();
    }
}

} // namespace ecm
