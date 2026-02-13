#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class SmoothingNode : public NodeBase
{
public:
    SmoothingNode()
    {
        addInput  ("in",  PortType::Signal);
        addOutput ("out", PortType::Signal);
        addParam  ("smoothing", 0.9f, 0.0f, 0.999f);
    }

    juce::String getTypeId()      const override { return "Smoothing"; }
    juce::String getDisplayName() const override { return "Smoothing (Lag)"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        currentValue_ = 0.f;
    }

    void processBlock (int /*numSamples*/) override
    {
        float in = getConnectedSignalValue (0);
        float smooth = getParamAsFloat ("smoothing", 0.9f);
        currentValue_ = currentValue_ * smooth + in * (1.f - smooth);
        setSignalOutputValue (0, currentValue_);
    }

private:
    float currentValue_ = 0.f;
};

} // namespace pf
