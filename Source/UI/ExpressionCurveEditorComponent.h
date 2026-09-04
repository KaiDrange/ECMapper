#pragma once

#include <JuceHeader.h>
#include "../Core/ExpressionCurve.h"
#include "../Core/ExpressionCurveWrapper.h"
#include "AppStyle.h"

namespace ecm {
class MidiService;

class ExpressionCurveEditorComponent : public juce::Component {
public:
    ExpressionCurveEditorComponent(InstrumentType deviceType, ExpressionCurveTarget target, juce::AudioProcessorValueTreeState& pluginState, MidiService& midiService, juce::String labelText);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void refreshFromState();

private:
    class PresetSwatchButton : public juce::Button {
    public:
        PresetSwatchButton(int presetId, const ExpressionCurveData& previewData, juce::Colour curveColour);

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    private:
        int presetId;
        ExpressionCurveData previewData;
        juce::Colour curveColour;
    };

    enum class Handle {
        None = -1,
        Start = 0,
        Left = 1,
        Center = 2,
        Right = 3,
        End = 4
    };

    InstrumentType deviceType;
    ExpressionCurveTarget target;
    juce::AudioProcessorValueTreeState& pluginState;
    MidiService& midiService;
    juce::String labelText;
    ExpressionCurve curve;
    Handle activeHandle = Handle::None;
    std::array<std::unique_ptr<PresetSwatchButton>, 5> presetButtons;

    juce::Rectangle<float> getPlotArea() const;
    juce::Point<float> toScreen(const ExpressionCurvePoint& point) const;
    ExpressionCurvePoint toCurvePoint(const juce::Point<float>& point, Handle handle) const;
    Handle pickHandle(const juce::Point<float>& point) const;
    void commitCurve();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpressionCurveEditorComponent)
};

} // namespace ecm
