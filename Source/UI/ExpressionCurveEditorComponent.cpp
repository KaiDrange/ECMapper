#include "ExpressionCurveEditorComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace ecm {

namespace {

constexpr int headerHeight = 22;
constexpr int presetButtonSize = 16;
constexpr int presetButtonGap = 4;

juce::String getDefaultCurveLabel(ExpressionCurveTarget target)
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

bool isAnchorHandle(int handle)
{
    return handle == 0 || handle == 2 || handle == 4;
}

ExpressionCurveData presetData(int presetId)
{
    switch (presetId) {
        case 1: // Flat diagonal
            return { 0.0f, { 0.25f, 0.25f }, 0.5f, { 0.75f, 0.75f }, 1.0f };
        case 2: // V shape
            return { 1.0f, { 0.25f, 0.0f }, 0.0f, { 0.75f, 0.0f }, 1.0f };
        case 3: // Exponential
            return { 0.0f, { 0.18f, 0.04f }, 0.28f, { 0.78f, 0.60f }, 1.0f };
        case 4: // Logarithmic
            return { 0.0f, { 0.20f, 0.68f }, 0.80f, { 0.92f, 0.96f }, 1.0f };
        case 5: // S curve
            return { 0.0f, { 0.18f, 0.02f }, 0.5f, { 0.82f, 0.98f }, 1.0f };
        default:
            return ExpressionCurveData{ 0.0f, { 0.25f, 0.25f }, 0.5f, { 0.75f, 0.75f }, 1.0f };
    }
}

} // namespace

ExpressionCurveEditorComponent::PresetSwatchButton::PresetSwatchButton(int presetId, const ExpressionCurveData& previewData, juce::Colour curveColour)
    : juce::Button("preset-" + juce::String(presetId)),
      presetId(presetId),
      previewData(previewData),
      curveColour(curveColour)
{
    setTooltip("Apply preset " + juce::String(presetId));
}

void ExpressionCurveEditorComponent::PresetSwatchButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto fill = Style::surfaceRaised();

    if (shouldDrawButtonAsDown)
        fill = fill.interpolatedWith(curveColour, 0.18f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.interpolatedWith(Style::accentStrong(), 0.08f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 2.5f);

    g.setColour(curveColour.withAlpha(0.75f));
    g.drawRoundedRectangle(bounds, 2.5f, 1.0f);

    auto previewArea = bounds.reduced(3.0f, 3.0f);
    auto previewPath = [&] {
        juce::Path path;
        auto toPoint = [previewArea](float x, float y) {
            return juce::Point<float>(
                previewArea.getX() + previewArea.getWidth() * x,
                previewArea.getBottom() - previewArea.getHeight() * y
            );
        };

        auto start = toPoint(0.0f, std::clamp(previewData.startY, 0.0f, 1.0f));
        auto left = toPoint(std::clamp(previewData.leftControl.x, 0.0f, 0.5f), std::clamp(previewData.leftControl.y, 0.0f, 1.0f));
        auto center = toPoint(0.5f, std::clamp(previewData.centerY, 0.0f, 1.0f));
        auto right = toPoint(std::clamp(previewData.rightControl.x, 0.5f, 1.0f), std::clamp(previewData.rightControl.y, 0.0f, 1.0f));
        auto end = toPoint(1.0f, std::clamp(previewData.endY, 0.0f, 1.0f));

        path.startNewSubPath(start);
        path.quadraticTo(left, center);
        path.quadraticTo(right, end);
        return path;
    }();

    g.setColour(curveColour.withAlpha(0.20f));
    g.strokePath(previewPath, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(curveColour.withAlpha(0.95f));
    g.strokePath(previewPath, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(shouldDrawButtonAsDown ? Style::text().withAlpha(0.65f) : Style::text().withAlpha(0.22f));
    g.drawRect(bounds);
    juce::ignoreUnused(presetId);
}

ExpressionCurveEditorComponent::ExpressionCurveEditorComponent(InstrumentType deviceType, ExpressionCurveTarget target, juce::AudioProcessorValueTreeState& pluginState, juce::String labelText)
    : deviceType(deviceType),
      target(target),
      pluginState(pluginState),
      labelText(labelText.isEmpty() ? getDefaultCurveLabel(target) : labelText),
      curve(ExpressionCurveWrapper::getCurve(deviceType, target, pluginState.state)) {
    auto deviceTabIndex = static_cast<int>(deviceType) - 1;
    auto curveColour = Style::tabColour(deviceTabIndex);

    for (int i = 0; i < 5; ++i) {
        presetButtons[(size_t)i] = std::make_unique<PresetSwatchButton>(i + 1, presetData(i + 1), curveColour);
        presetButtons[(size_t)i]->onClick = [this, presetId = i + 1] {
            curve.setData(presetData(presetId));
            commitCurve();
        };
        addAndMakeVisible(presetButtons[(size_t)i].get());
    }
}

void ExpressionCurveEditorComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    juce::ColourGradient panelGradient(Style::surfaceRaised().interpolatedWith(Style::background(), 0.12f),
                                       bounds.getCentreX(), bounds.getY(),
                                       Style::surface(),
                                       bounds.getCentreX(), bounds.getBottom(),
                                       false);
    g.setGradientFill(panelGradient);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(Style::border().withAlpha(0.92f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    auto innerFrame = bounds.reduced(1.2f);
    juce::ColourGradient frameGlow(juce::Colour(0xff3b4a5c).withAlpha(0.25f),
                                   innerFrame.getX(), innerFrame.getY(),
                                   juce::Colours::transparentBlack,
                                   innerFrame.getRight(), innerFrame.getBottom(),
                                   false);
    g.setGradientFill(frameGlow);
    g.drawRoundedRectangle(innerFrame, 3.0f, 1.0f);

    auto titleArea = bounds.removeFromTop((float) headerHeight).reduced(6.0f, 2.0f).withTrimmedRight((float) ((presetButtonSize + presetButtonGap) * 5 + 4));
    g.setColour(Style::text().withAlpha(0.92f));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
    g.drawText(labelText, titleArea.toNearestInt(), juce::Justification::centredLeft, 1);

    auto plotArea = bounds.reduced(8.0f, 6.0f);
    if (plotArea.getWidth() <= 0.0f || plotArea.getHeight() <= 0.0f)
        return;

    juce::ColourGradient plotGradient(juce::Colour(0xff101720),
                                      plotArea.getX(), plotArea.getY(),
                                      juce::Colour(0xff1b2531),
                                      plotArea.getRight(), plotArea.getBottom(),
                                      false);
    g.setGradientFill(plotGradient);
    g.fillRoundedRectangle(plotArea, 3.0f);

    g.setColour(juce::Colour(0xffffffff).withAlpha(0.03f));
    g.fillRoundedRectangle(plotArea.reduced(1.0f, 1.0f), 3.0f);

    auto gridColour = Style::border().withAlpha(0.55f);
    g.setColour(gridColour);
    for (int i = 1; i < 4; ++i) {
        auto x = plotArea.getX() + plotArea.getWidth() * (i / 4.0f);
        g.drawVerticalLine((int)std::round(x), plotArea.getY(), plotArea.getBottom());
        auto y = plotArea.getY() + plotArea.getHeight() * (i / 4.0f);
        g.drawHorizontalLine((int)std::round(y), plotArea.getX(), plotArea.getRight());
    }

    g.setColour(Style::text().withAlpha(0.06f));
    g.fillRect(plotArea.getX(), plotArea.getCentreY() - 0.5f, plotArea.getWidth(), 1.0f);
    g.fillRect(plotArea.getCentreX() - 0.5f, plotArea.getY(), 1.0f, plotArea.getHeight());
    g.fillRect(plotArea.getX(), plotArea.getBottom() - 0.5f, plotArea.getWidth(), 1.0f);

    auto deviceTabIndex = static_cast<int>(deviceType) - 1;
    auto curveColour = Style::tabColour(deviceTabIndex);
    auto path = curve.createPath(plotArea);

    g.setColour(curveColour.withAlpha(0.18f));
    g.strokePath(path, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(curveColour.withAlpha(0.95f));
    g.strokePath(path, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto data = curve.getData();
    auto handles = std::array<ExpressionCurvePoint, 5> {
        ExpressionCurvePoint { 0.0f, data.startY },
        data.leftControl,
        ExpressionCurvePoint { 0.5f, data.centerY },
        data.rightControl,
        ExpressionCurvePoint { 1.0f, data.endY }
    };

    for (int i = 0; i < (int)handles.size(); ++i) {
        auto screen = toScreen(handles[(size_t)i]);
        auto handle = static_cast<Handle>(i);
        auto anchor = isAnchorHandle(i);
        auto handleSize = anchor ? 11.0f : 9.0f;
        auto handleBounds = juce::Rectangle<float>(screen.x - handleSize * 0.5f, screen.y - handleSize * 0.5f, handleSize, handleSize);
        auto selected = handle == activeHandle;

        if (anchor) {
            auto fill = selected ? curveColour.brighter(0.45f) : curveColour.withAlpha(0.82f);
            auto outline = selected ? Style::text().withAlpha(0.95f) : curveColour.withAlpha(0.98f);

            juce::ColourGradient anchorGradient(fill.brighter(0.18f),
                                                handleBounds.getX(), handleBounds.getY(),
                                                fill.darker(0.25f),
                                                handleBounds.getRight(), handleBounds.getBottom(),
                                                false);
            g.setGradientFill(anchorGradient);
            g.fillRoundedRectangle(handleBounds.reduced(0.2f), 2.5f);

            g.setColour(outline);
            g.drawRoundedRectangle(handleBounds.reduced(0.2f), 2.5f, selected ? 1.7f : 1.1f);
        } else {
            auto fill = selected ? Style::surfaceRaised().brighter(0.08f) : Style::surfaceRaised();
            auto outline = selected ? curveColour.brighter(0.35f) : curveColour.withAlpha(0.92f);

            juce::ColourGradient controlGradient(fill.brighter(0.12f),
                                                 handleBounds.getX(), handleBounds.getY(),
                                                 fill.darker(0.22f),
                                                 handleBounds.getRight(), handleBounds.getBottom(),
                                                 false);
            g.setGradientFill(controlGradient);
            g.fillEllipse(handleBounds);

            g.setColour(outline);
            g.drawEllipse(handleBounds, selected ? 1.7f : 1.1f);
        }
    }
}

void ExpressionCurveEditorComponent::resized() {
    auto header = getLocalBounds().removeFromTop(headerHeight).reduced(6, 2);
    auto presetArea = header.removeFromRight((presetButtonSize + presetButtonGap) * 5 - presetButtonGap);
    for (auto i = 0; i < 5; ++i) {
        auto* button = presetButtons[(size_t)i].get();
        button->setBounds(presetArea.removeFromLeft(presetButtonSize));
        if (i < 4)
            presetArea.removeFromLeft(presetButtonGap);
    }
}

juce::Rectangle<float> ExpressionCurveEditorComponent::getPlotArea() const {
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    bounds.removeFromTop((float) headerHeight);
    return bounds.reduced(8.0f, 6.0f);
}

juce::Point<float> ExpressionCurveEditorComponent::toScreen(const ExpressionCurvePoint& point) const {
    auto plotArea = getPlotArea();
    return {
        plotArea.getX() + plotArea.getWidth() * std::clamp(point.x, 0.0f, 1.0f),
        plotArea.getBottom() - plotArea.getHeight() * std::clamp(point.y, 0.0f, 1.0f)
    };
}

ExpressionCurvePoint ExpressionCurveEditorComponent::toCurvePoint(const juce::Point<float>& point, Handle handle) const {
    auto plotArea = getPlotArea();
    auto normalizedX = plotArea.getWidth() > 0.0f ? (point.x - plotArea.getX()) / plotArea.getWidth() : 0.0f;
    auto normalizedY = plotArea.getHeight() > 0.0f ? (plotArea.getBottom() - point.y) / plotArea.getHeight() : 0.0f;
    normalizedX = std::clamp(normalizedX, 0.0f, 1.0f);
    normalizedY = std::clamp(normalizedY, 0.0f, 1.0f);

    switch (handle) {
        case Handle::Start:
            normalizedX = 0.0f;
            break;
        case Handle::Left:
            normalizedX = std::clamp(normalizedX, 0.0f, 0.5f);
            break;
        case Handle::Center:
            normalizedX = 0.5f;
            break;
        case Handle::Right:
            normalizedX = std::clamp(normalizedX, 0.5f, 1.0f);
            break;
        case Handle::End:
            normalizedX = 1.0f;
            break;
        default:
            break;
    }

    return { normalizedX, normalizedY };
}

ExpressionCurveEditorComponent::Handle ExpressionCurveEditorComponent::pickHandle(const juce::Point<float>& point) const {
    auto data = curve.getData();
    std::array<std::pair<Handle, juce::Point<float>>, 5> handles {
        std::make_pair(Handle::Start, toScreen({ 0.0f, data.startY })),
        std::make_pair(Handle::Left, toScreen(data.leftControl)),
        std::make_pair(Handle::Center, toScreen({ 0.5f, data.centerY })),
        std::make_pair(Handle::Right, toScreen(data.rightControl)),
        std::make_pair(Handle::End, toScreen({ 1.0f, data.endY }))
    };

    Handle best = Handle::Center;
    float bestDistance = std::numeric_limits<float>::max();
    for (const auto& [handle, handlePoint] : handles) {
        auto distance = handlePoint.getDistanceFrom(point);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = handle;
        }
    }

    juce::ignoreUnused(bestDistance);
    return best;
}

void ExpressionCurveEditorComponent::commitCurve() {
    ExpressionCurveWrapper::setCurve(deviceType, target, curve, pluginState.state);
    repaint();
}

void ExpressionCurveEditorComponent::mouseDown(const juce::MouseEvent& e) {
    activeHandle = pickHandle(e.position);
    mouseDrag(e);
}

void ExpressionCurveEditorComponent::mouseDrag(const juce::MouseEvent& e) {
    if (activeHandle == Handle::None)
        return;

    auto data = curve.getData();
    auto point = toCurvePoint(e.position, activeHandle);

    switch (activeHandle) {
        case Handle::Start:
            data.startY = point.y;
            break;
        case Handle::Left:
            data.leftControl = point;
            break;
        case Handle::Center:
            data.centerY = point.y;
            break;
        case Handle::Right:
            data.rightControl = point;
            break;
        case Handle::End:
            data.endY = point.y;
            break;
        case Handle::None:
            break;
    }

    curve.setData(data);
    commitCurve();
}

void ExpressionCurveEditorComponent::mouseUp(const juce::MouseEvent&) {
    activeHandle = Handle::None;
}

} // namespace ecm
