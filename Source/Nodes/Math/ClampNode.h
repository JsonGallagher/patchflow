#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class ClampNode : public NodeBase
{
public:
    ClampNode()
    {
        addInput  ("in",  PortType::Signal);
        addOutput ("out", PortType::Signal);
        addParam  ("min", 0.0f, {}, {}, "Min", "Minimum output value");
        addParam  ("max", 1.0f, {}, {}, "Max", "Maximum output value");
    }

    juce::String getTypeId()      const override { return "Clamp"; }
    juce::String getDisplayName() const override { return "Clamp"; }
    juce::String getCategory()    const override { return "Math"; }

    void processBlock (int /*numSamples*/) override
    {
        float in = getConnectedSignalValue (0);
        float lo = getParamAsFloat ("min", 0.f);
        float hi = getParamAsFloat ("max", 1.f);
        setSignalOutputValue (0, juce::jlimit (lo, hi, in));
    }
};

} // namespace pf
