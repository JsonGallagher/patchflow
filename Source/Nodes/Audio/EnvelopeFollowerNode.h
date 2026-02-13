#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class EnvelopeFollowerNode : public NodeBase
{
public:
    EnvelopeFollowerNode()
    {
        addInput  ("in",       PortType::Audio);
        addOutput ("envelope", PortType::Signal);
        addParam  ("attackMs",  10.0f,  1.0f, 500.0f);
        addParam  ("releaseMs", 100.0f, 1.0f, 2000.0f);
    }

    juce::String getTypeId()      const override { return "EnvelopeFollower"; }
    juce::String getDisplayName() const override { return "Envelope Follower"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        envelope_ = 0.f;
        updateCoefficients();
    }

    void processBlock (int numSamples) override
    {
        auto* in = getConnectedAudioBuffer (0);
        if (! in) { setSignalOutputValue (0, 0.f); return; }

        updateCoefficients();

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = std::abs (in[i]);
            float coeff = (sample > envelope_) ? attackCoeff_ : releaseCoeff_;
            envelope_ = envelope_ * coeff + sample * (1.f - coeff);
        }

        setSignalOutputValue (0, envelope_);
    }

private:
    void updateCoefficients()
    {
        float atk = getParamAsFloat ("attackMs",  10.f);
        float rel = getParamAsFloat ("releaseMs", 100.f);
        attackCoeff_  = std::exp (-1.0f / (static_cast<float> (sampleRate_) * atk * 0.001f));
        releaseCoeff_ = std::exp (-1.0f / (static_cast<float> (sampleRate_) * rel * 0.001f));
    }

    float envelope_     = 0.f;
    float attackCoeff_  = 0.f;
    float releaseCoeff_ = 0.f;
};

} // namespace pf
