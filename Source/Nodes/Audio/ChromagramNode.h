#pragma once
#include "Nodes/NodeBase.h"
#include <cmath>
#include <array>

namespace pf
{

class ChromagramNode : public NodeBase
{
public:
    ChromagramNode()
    {
        addInput  ("magnitudes",    PortType::Buffer);
        addOutput ("chroma",        PortType::Buffer);
        addOutput ("dominant_note", PortType::Signal);

        addParam ("tuning",    440.0f, 420.0f, 460.0f, "Tuning", "A4 reference frequency", "Hz", "Analysis");
        addParam ("smoothing", 0.8f, 0.0f, 0.99f, "Smoothing", "Chroma smoothing", "", "Analysis");
    }

    juce::String getTypeId()      const override { return "Chromagram"; }
    juce::String getDisplayName() const override { return "Chromagram"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        resizeBufferOutput (0, 12);
        smoothedChroma_.fill (0.0f);
    }

    void processBlock (int /*numSamples*/) override
    {
        auto mags = getConnectedBufferData (0);
        if (mags.empty())
        {
            setSignalOutputValue (0, 0.f);
            return;
        }

        int numBins = static_cast<int> (mags.size());
        float tuning = getParamAsFloat ("tuning", 440.0f);
        float smoothing = getParamAsFloat ("smoothing", 0.8f);
        float binHz = static_cast<float> (sampleRate_) / static_cast<float> (numBins * 2);

        std::array<float, 12> chroma {};

        for (int i = 1; i < numBins; ++i)
        {
            float freq = static_cast<float> (i) * binHz;
            if (freq < 20.0f || freq > 8000.0f) continue;

            // Map frequency to pitch class (0-11)
            float midiNote = 12.0f * std::log2 (freq / tuning) + 69.0f;
            int pitchClass = static_cast<int> (std::round (midiNote)) % 12;
            if (pitchClass < 0) pitchClass += 12;

            chroma[pitchClass] += mags[i];
        }

        // Normalize
        float maxChroma = *std::max_element (chroma.begin(), chroma.end());
        if (maxChroma > 0.0001f)
        {
            for (auto& c : chroma) c /= maxChroma;
        }

        // Smooth and find dominant note
        int dominantNote = 0;
        float maxVal = 0.0f;

        auto& bufOut = getBufferOutputVec (0);
        if (bufOut.size() != 12) bufOut.resize (12, 0.0f);

        for (int i = 0; i < 12; ++i)
        {
            smoothedChroma_[i] = smoothedChroma_[i] * smoothing + chroma[i] * (1.0f - smoothing);
            bufOut[i] = smoothedChroma_[i];

            if (smoothedChroma_[i] > maxVal)
            {
                maxVal = smoothedChroma_[i];
                dominantNote = i;
            }
        }

        setSignalOutputValue (0, static_cast<float> (dominantNote) / 11.0f);
    }

private:
    std::array<float, 12> smoothedChroma_ {};
};

} // namespace pf
