#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class BandSplitterNode : public NodeBase
{
public:
    BandSplitterNode()
    {
        addInput  ("magnitudes", PortType::Buffer);
        addOutput ("sub",        PortType::Signal);
        addOutput ("low",        PortType::Signal);
        addOutput ("mid",        PortType::Signal);
        addOutput ("high",       PortType::Signal);
        addOutput ("presence",   PortType::Signal);
        addParam  ("crossover1", 60.0f,    20.0f, 200.0f,   "Sub/Low",       "Sub to low crossover frequency", "Hz", "Crossovers");
        addParam  ("crossover2", 250.0f,   100.0f, 1000.0f,  "Low/Mid",       "Low to mid crossover frequency", "Hz", "Crossovers");
        addParam  ("crossover3", 2000.0f,  500.0f, 5000.0f,  "Mid/High",      "Mid to high crossover frequency", "Hz", "Crossovers");
        addParam  ("crossover4", 6000.0f,  2000.0f, 12000.0f, "High/Presence", "High to presence crossover frequency", "Hz", "Crossovers");
    }

    juce::String getTypeId()      const override { return "BandSplitter"; }
    juce::String getDisplayName() const override { return "Band Splitter"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
    }

    void processBlock (int /*numSamples*/) override
    {
        auto mags = getConnectedBufferData (0);
        if (mags.empty())
        {
            for (int i = 0; i < 5; ++i) setSignalOutputValue (i, 0.f);
            return;
        }

        int numBins = static_cast<int> (mags.size());
        float binHz = static_cast<float> (sampleRate_) / static_cast<float> (numBins * 2);

        float c1 = getParamAsFloat ("crossover1", 60.f);
        float c2 = getParamAsFloat ("crossover2", 250.f);
        float c3 = getParamAsFloat ("crossover3", 2000.f);
        float c4 = getParamAsFloat ("crossover4", 6000.f);

        int bin1 = juce::jlimit (0, numBins, static_cast<int> (c1 / binHz));
        int bin2 = juce::jlimit (0, numBins, static_cast<int> (c2 / binHz));
        int bin3 = juce::jlimit (0, numBins, static_cast<int> (c3 / binHz));
        int bin4 = juce::jlimit (0, numBins, static_cast<int> (c4 / binHz));

        auto avgRange = [&] (int start, int end) -> float
        {
            if (start >= end) return 0.f;
            float sum = 0.f;
            for (int i = start; i < end; ++i) sum += mags[i];
            return sum / static_cast<float> (end - start);
        };

        setSignalOutputValue (0, avgRange (0, bin1));
        setSignalOutputValue (1, avgRange (bin1, bin2));
        setSignalOutputValue (2, avgRange (bin2, bin3));
        setSignalOutputValue (3, avgRange (bin3, bin4));
        setSignalOutputValue (4, avgRange (bin4, numBins));
    }
};

} // namespace pf
