#include "UI/PortComponent.h"
#include "UI/NodeComponent.h"
#include "UI/NodeEditorComponent.h"
#include "UI/Theme.h"

namespace pf
{

PortComponent::PortComponent (NodeComponent& owner, const juce::String& name,
                              PortType type, PortDirection direction, int index)
    : owner_ (owner), name_ (name), type_ (type), direction_ (direction), index_ (index)
{
    setSize (kDiameter, kDiameter);
    setTooltip (name + " (" + portTypeToString (type) + ")");
}

void PortComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto colour = getPortColour (type_);

    // Compatibility glow ring (during cable drag)
    if (highlight_ == Highlight::Compatible)
    {
        g.setColour (juce::Colour (Theme::kPortCompatible).withAlpha (0.5f));
        g.fillEllipse (bounds.expanded (4.f));
    }
    else if (highlight_ == Highlight::Incompatible)
    {
        g.setColour (juce::Colour (Theme::kPortIncompatible));
        g.fillEllipse (bounds.expanded (2.f));
    }

    // Hover glow
    if (hovered_)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (bounds.expanded (3.f));
    }

    // Port dot
    g.setColour (colour);
    g.fillEllipse (bounds.reduced (1.f));
    g.setColour (colour.brighter (0.3f));
    g.drawEllipse (bounds.reduced (1.f), 1.f);
}

void PortComponent::mouseEnter (const juce::MouseEvent&)
{
    hovered_ = true;
    repaint();
}

void PortComponent::mouseExit (const juce::MouseEvent&)
{
    hovered_ = false;
    repaint();
}

juce::Point<float> PortComponent::getCentreInParentEditor() const
{
    auto centre = getLocalBounds().getCentre().toFloat();
    // Convert from port local -> node local -> editor local
    auto inNode = getPosition().toFloat() + centre;
    auto inEditor = owner_.getPosition().toFloat() + inNode;
    return inEditor;
}

} // namespace pf
