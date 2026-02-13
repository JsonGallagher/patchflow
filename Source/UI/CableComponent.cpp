#include "UI/CableComponent.h"

namespace pf
{

CableComponent::CableComponent (const Connection& connection, PortType type)
    : connection_ (connection), type_ (type)
{
    setInterceptsMouseClicks (false, false);
}

void CableComponent::setEndPoints (juce::Point<float> start, juce::Point<float> end)
{
    startPoint_ = start;
    endPoint_ = end;
    updatePath();

    auto pathBounds = cablePath_.getBounds().expanded (4.f).getSmallestIntegerContainer();
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
    auto colour = getPortColour (type_).withAlpha (0.8f);
    g.setColour (colour);

    juce::Path localPath (cablePath_);
    localPath.applyTransform (juce::AffineTransform::translation (
        static_cast<float> (-getX()), static_cast<float> (-getY())));

    g.strokePath (localPath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));

    // Draw glow
    g.setColour (colour.withAlpha (0.15f));
    g.strokePath (localPath, juce::PathStrokeType (6.f, juce::PathStrokeType::curved));
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
