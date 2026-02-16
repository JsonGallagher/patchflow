#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class AudioInputNode : public NodeBase
{
public:
    AudioInputNode()
    {
        addOutput ("audio_L", PortType::Audio);
        addOutput ("audio_R", PortType::Audio);
    }

    juce::String getTypeId()      const override { return "AudioInput"; }
    juce::String getDisplayName() const override { return "Audio Input"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        resizeAudioBuffer (0, blockSize);
        resizeAudioBuffer (1, blockSize);
        currentBufferSize_ = blockSize;
    }

    // AudioEngine writes directly to our output buffers
    void processBlock (int /*numSamples*/) override {}

    void ensureOutputBufferSize (int numSamples)
    {
        if (numSamples <= currentBufferSize_)
            return;

        resizeAudioBuffer (0, numSamples);
        resizeAudioBuffer (1, numSamples);
        currentBufferSize_ = numSamples;
    }

private:
    int currentBufferSize_ = 0;
};

} // namespace pf
