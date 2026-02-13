#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class MapRangeNode : public NodeBase
{
public:
    MapRangeNode()
    {
        addInput  ("in",  PortType::Signal);
        addOutput ("out", PortType::Signal);
        addParam  ("inMin",  0.0f);
        addParam  ("inMax",  1.0f);
        addParam  ("outMin", 0.0f);
        addParam  ("outMax", 1.0f);
    }

    juce::String getTypeId()      const override { return "MapRange"; }
    juce::String getDisplayName() const override { return "Map Range"; }
    juce::String getCategory()    const override { return "Math"; }

    void processBlock (int /*numSamples*/) override
    {
        float in     = getConnectedSignalValue (0);
        float inMin  = getParamAsFloat ("inMin", 0.f);
        float inMax  = getParamAsFloat ("inMax", 1.f);
        float outMin = getParamAsFloat ("outMin", 0.f);
        float outMax = getParamAsFloat ("outMax", 1.f);

        float range = inMax - inMin;
        float t = (range != 0.f) ? (in - inMin) / range : 0.f;
        t = juce::jlimit (0.f, 1.f, t);
        setSignalOutputValue (0, outMin + t * (outMax - outMin));
    }
};

} // namespace pf
