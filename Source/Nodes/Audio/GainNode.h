#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class GainNode : public NodeBase
{
public:
    GainNode()
    {
        addInput  ("in",   PortType::Audio);
        addInput  ("gain", PortType::Signal);
        addOutput ("out",  PortType::Audio);
        addParam  ("gain", 1.0f, 0.0f, 2.0f);
    }

    juce::String getTypeId()      const override { return "Gain"; }
    juce::String getDisplayName() const override { return "Gain"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        resizeAudioBuffer (0, blockSize);
    }

    void processBlock (int numSamples) override
    {
        auto* in  = getConnectedAudioBuffer (0);
        auto* out = getAudioOutputBuffer (0);
        if (! out) return;

        float gainVal = isInputConnected (1)
                      ? getConnectedSignalValue (1)
                      : getParamAsFloat ("gain", 1.0f);

        if (in)
        {
            for (int i = 0; i < numSamples; ++i)
                out[i] = in[i] * gainVal;
        }
        else
        {
            std::fill_n (out, numSamples, 0.f);
        }
    }
};

} // namespace pf
