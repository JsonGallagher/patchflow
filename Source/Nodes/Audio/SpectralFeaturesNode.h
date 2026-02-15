#pragma once
#include "Nodes/NodeBase.h"
#include <cmath>

namespace pf
{

class SpectralFeaturesNode : public NodeBase
{
public:
    SpectralFeaturesNode()
    {
        addInput  ("magnitudes", PortType::Buffer);
        addOutput ("centroid",   PortType::Signal);
        addOutput ("flux",       PortType::Signal);
        addOutput ("rolloff",    PortType::Signal);
        addOutput ("flatness",   PortType::Signal);

        addParam ("rolloffPercent", 0.85f, 0.5f, 0.99f, "Rolloff %", "Energy rolloff threshold", "", "Analysis");
        addParam ("smoothing",     0.8f, 0.0f, 0.99f, "Smoothing", "Output smoothing factor", "", "Analysis");
    }

    juce::String getTypeId()      const override { return "SpectralFeatures"; }
    juce::String getDisplayName() const override { return "Spectral Features"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        prevMags_.clear();
        smoothedCentroid_ = 0.0f;
        smoothedFlux_ = 0.0f;
        smoothedRolloff_ = 0.0f;
        smoothedFlatness_ = 0.0f;
    }

    void processBlock (int /*numSamples*/) override
    {
        auto mags = getConnectedBufferData (0);
        if (mags.empty())
        {
            for (int i = 0; i < 4; ++i) setSignalOutputValue (i, 0.f);
            return;
        }

        int numBins = static_cast<int> (mags.size());
        float smoothing = getParamAsFloat ("smoothing", 0.8f);
        float rolloffPct = getParamAsFloat ("rolloffPercent", 0.85f);

        // Spectral centroid: weighted mean frequency
        float totalEnergy = 0.0f;
        float weightedSum = 0.0f;
        for (int i = 0; i < numBins; ++i)
        {
            totalEnergy += mags[i];
            weightedSum += mags[i] * static_cast<float> (i);
        }
        float centroid = totalEnergy > 0.0001f ? weightedSum / (totalEnergy * numBins) : 0.0f;

        // Spectral flux: half-wave rectified difference
        if (prevMags_.size() != static_cast<size_t> (numBins))
            prevMags_.assign (static_cast<size_t> (numBins), 0.0f);

        float flux = 0.0f;
        for (int i = 0; i < numBins; ++i)
        {
            float diff = mags[i] - prevMags_[i];
            if (diff > 0.0f) flux += diff;
        }
        flux /= static_cast<float> (numBins);
        prevMags_.assign (mags.begin(), mags.end());

        // Spectral rolloff: frequency below which rolloffPct of energy lies
        float rolloffThreshold = totalEnergy * rolloffPct;
        float cumulative = 0.0f;
        float rolloff = 1.0f;
        for (int i = 0; i < numBins; ++i)
        {
            cumulative += mags[i];
            if (cumulative >= rolloffThreshold)
            {
                rolloff = static_cast<float> (i) / static_cast<float> (numBins);
                break;
            }
        }

        // Spectral flatness: geometric mean / arithmetic mean
        float logSum = 0.0f;
        int nonZeroCount = 0;
        for (int i = 0; i < numBins; ++i)
        {
            if (mags[i] > 1e-10f)
            {
                logSum += std::log (mags[i]);
                nonZeroCount++;
            }
        }
        float arithmeticMean = totalEnergy / static_cast<float> (numBins);
        float geometricMean = nonZeroCount > 0 ? std::exp (logSum / nonZeroCount) : 0.0f;
        float flatness = arithmeticMean > 1e-10f ? geometricMean / arithmeticMean : 0.0f;

        // Apply smoothing
        smoothedCentroid_ = smoothedCentroid_ * smoothing + centroid * (1.0f - smoothing);
        smoothedFlux_ = smoothedFlux_ * smoothing + flux * (1.0f - smoothing);
        smoothedRolloff_ = smoothedRolloff_ * smoothing + rolloff * (1.0f - smoothing);
        smoothedFlatness_ = smoothedFlatness_ * smoothing + flatness * (1.0f - smoothing);

        setSignalOutputValue (0, juce::jlimit (0.0f, 1.0f, smoothedCentroid_));
        setSignalOutputValue (1, juce::jlimit (0.0f, 1.0f, smoothedFlux_ * 10.0f));
        setSignalOutputValue (2, juce::jlimit (0.0f, 1.0f, smoothedRolloff_));
        setSignalOutputValue (3, juce::jlimit (0.0f, 1.0f, smoothedFlatness_));
    }

private:
    std::vector<float> prevMags_;
    float smoothedCentroid_ = 0.0f;
    float smoothedFlux_ = 0.0f;
    float smoothedRolloff_ = 0.0f;
    float smoothedFlatness_ = 0.0f;
};

} // namespace pf
