#include "UI/PortComponent.h"
#include "UI/NodeComponent.h"
#include "UI/NodeEditorComponent.h"

namespace pf
{

PortComponent::PortComponent (NodeComponent& owner, const juce::String& name,
                              PortType type, PortDirection direction, int index)
    : owner_ (owner), name_ (name), type_ (type), direction_ (direction), index_ (index)
{
    setSize (kDiameter, kDiameter);
}

void PortComponent::paint (juce::Graphics& g)
{
    auto colour = getPortColour (type_);
    g.setColour (colour);
    g.fillEllipse (getLocalBounds().toFloat().reduced (1.f));
    g.setColour (colour.brighter (0.3f));
    g.drawEllipse (getLocalBounds().toFloat().reduced (1.f), 1.f);
}

juce::Point<float> PortComponent::getCentreInParentEditor() const
{
    auto centre = getLocalBounds().getCentre().toFloat();
    // Convert from port local → node local → editor local
    auto inNode = getPosition().toFloat() + centre;
    auto inEditor = owner_.getPosition().toFloat() + inNode;
    return inEditor;
}

} // namespace pf
