#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class AddNode : public NodeBase
{
public:
    AddNode()
    {
        addInput  ("a",   PortType::Signal);
        addInput  ("b",   PortType::Signal);
        addOutput ("out", PortType::Signal);
    }

    juce::String getTypeId()      const override { return "Add"; }
    juce::String getDisplayName() const override { return "Add"; }
    juce::String getCategory()    const override { return "Math"; }

    void processBlock (int /*numSamples*/) override
    {
        float a = getConnectedSignalValue (0);
        float b = getConnectedSignalValue (1);
        setSignalOutputValue (0, a + b);
    }
};

} // namespace pf
