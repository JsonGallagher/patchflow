#include "UI/CableComponent.h"

namespace pf
{

CableComponent::CableComponent (const Connection& connection, PortType type)
    : connection_ (connection), type_ (type)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void CableComponent::timerCallback()
{
    flowPhase_ += 0.08f;
    if (flowPhase_ > 100.f) flowPhase_ -= 100.f;
    repaint();
}

void CableComponent::setEndPoints (juce::Point<float> start, juce::Point<float> end)
{
    startPoint_ = start;
    endPoint_ = end;
    updatePath();

    auto pathBounds = cablePath_.getBounds().expanded (6.f).getSmallestIntegerContainer();
    setBounds (pathBounds);
}

void CableComponent::updatePath()
{
    cablePath_.clear();
    cablePath_.startNewSubPath (startPoint_);

    float dx = std::abs (endPoint_.x - startPoint_.x);
    float controlDist = juce::jmax (50.f, dx * 0.4f);

    cablePath_.cubicTo (
        startPoint_.x + controlDist, startPoint_.y,
        endPoint_.x - controlDist, endPoint_.y,
        endPoint_.x, endPoint_.y
    );
}

void CableComponent::paint (juce::Graphics& g)
{
    auto colour = getPortColour (type_);

    juce::Path localPath (cablePath_);
    localPath.applyTransform (juce::AffineTransform::translation (
        static_cast<float> (-getX()), static_cast<float> (-getY())));

    // Outer glow
    g.setColour (colour.withAlpha (0.1f));
    g.strokePath (localPath, juce::PathStrokeType (8.f, juce::PathStrokeType::curved));

    // Main cable
    g.setColour (colour.withAlpha (0.7f));
    g.strokePath (localPath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));

    // Animated flow dots along path
    float pathLength = localPath.getLength();
    if (pathLength < 1.f) return;

    float dotSpacing = 18.f;
    float dotRadius = 2.2f;
    g.setColour (colour.withAlpha (0.9f));

    float offset = std::fmod (flowPhase_ * dotSpacing, dotSpacing);
    for (float d = offset; d < pathLength; d += dotSpacing)
    {
        auto pt = localPath.getPointAlongPath (d);
        g.fillEllipse (pt.x - dotRadius, pt.y - dotRadius, dotRadius * 2.f, dotRadius * 2.f);
    }
}

bool CableComponent::hitTest (int x, int y)
{
    juce::Path localPath (cablePath_);
    localPath.applyTransform (juce::AffineTransform::translation (
        static_cast<float> (-getX()), static_cast<float> (-getY())));

    juce::PathStrokeType stroke (8.f);
    juce::Path strokePath;
    stroke.createStrokedPath (strokePath, localPath);
    return strokePath.contains (static_cast<float> (x), static_cast<float> (y));
}

} // namespace pf
