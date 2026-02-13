#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/PortTypes.h"

namespace pf
{

class NodeComponent;

class PortComponent : public juce::Component
{
public:
    PortComponent (NodeComponent& owner, const juce::String& name,
                   PortType type, PortDirection direction, int index);

    void paint (juce::Graphics& g) override;

    juce::Point<float> getCentreInParentEditor() const;

    NodeComponent& getOwner() const { return owner_; }
    PortType getType() const { return type_; }
    PortDirection getDirection() const { return direction_; }
    int getPortIndex() const { return index_; }
    const juce::String& getPortName() const { return name_; }

    static constexpr int kDiameter = 12;

private:
    NodeComponent& owner_;
    juce::String name_;
    PortType type_;
    PortDirection direction_;
    int index_;
};

} // namespace pf
