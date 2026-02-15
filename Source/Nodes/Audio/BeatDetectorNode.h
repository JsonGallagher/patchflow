#pragma once
#include "Nodes/NodeBase.h"
#include <cmath>
#include <array>

namespace pf
{

class BeatDetectorNode : public NodeBase
{
public:
    BeatDetectorNode()
    {
        addInput  ("magnitudes", PortType::Buffer);
        addOutput ("beat",       PortType::Signal);
        addOutput ("bpm",        PortType::Signal);
        addOutput ("phase",      PortType::Signal);
        addOutput ("onset",      PortType::Signal);

        addParam ("sensitivity", 1.5f, 0.5f, 4.0f, "Sensitivity", "Beat detection threshold multiplier", "x", "Detection");
        addParam ("minBPM",      60.0f, 30.0f, 120.0f, "Min BPM", "Minimum expected BPM", "BPM", "Detection");
        addParam ("maxBPM",      200.0f, 120.0f, 300.0f, "Max BPM", "Maximum expected BPM", "BPM", "Detection");
        addParam ("mode",        0, 0, 1, "Mode", "Detection algorithm", "", "Detection",
                  juce::StringArray { "Energy", "Spectral Flux" });
    }

    juce::String getTypeId()      const override { return "BeatDetector"; }
    juce::String getDisplayName() const override { return "Beat Detector"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        energyHistory_.fill (0.0f);
        historyIndex_ = 0;
        beatDecay_ = 0.0f;
        lastFlux_ = 0.0f;
        beatTimestamps_.fill (0.0);
        beatCount_ = 0;
        currentBPM_ = 120.0f;
        phase_ = 0.0f;
        lastBeatTime_ = 0.0;
        totalTime_ = 0.0;
        prevMagnitudes_.clear();
    }

    void processBlock (int numSamples) override
    {
        auto mags = getConnectedBufferData (0);
        if (mags.empty())
        {
            setSignalOutputValue (0, 0.f);
            setSignalOutputValue (1, 0.f);
            setSignalOutputValue (2, 0.f);
            setSignalOutputValue (3, 0.f);
            return;
        }

        int numBins = static_cast<int> (mags.size());
        float sensitivity = getParamAsFloat ("sensitivity", 1.5f);
        int mode = getParamAsInt ("mode", 0);

        totalTime_ += static_cast<double> (numSamples) / sampleRate_;

        float onset = 0.0f;

        if (mode == 0) // Energy mode
        {
            float energy = 0.0f;
            for (int i = 0; i < numBins; ++i)
                energy += mags[i] * mags[i];
            energy /= static_cast<float> (numBins);

            // Running average of energy
            energyHistory_[historyIndex_] = energy;
            historyIndex_ = (historyIndex_ + 1) % kHistorySize;

            float avgEnergy = 0.0f;
            for (auto e : energyHistory_) avgEnergy += e;
            avgEnergy /= static_cast<float> (kHistorySize);

            onset = energy / juce::jmax (0.0001f, avgEnergy);
        }
        else // Spectral flux mode
        {
            if (prevMagnitudes_.size() != static_cast<size_t> (numBins))
                prevMagnitudes_.assign (static_cast<size_t> (numBins), 0.0f);

            float flux = 0.0f;
            for (int i = 0; i < numBins; ++i)
            {
                float diff = mags[i] - prevMagnitudes_[i];
                if (diff > 0.0f) flux += diff; // half-wave rectified
            }
            flux /= static_cast<float> (numBins);
            prevMagnitudes_.assign (mags.begin(), mags.end());

            energyHistory_[historyIndex_] = flux;
            historyIndex_ = (historyIndex_ + 1) % kHistorySize;

            float avgFlux = 0.0f;
            for (auto e : energyHistory_) avgFlux += e;
            avgFlux /= static_cast<float> (kHistorySize);

            onset = flux / juce::jmax (0.0001f, avgFlux);
        }

        // Beat detection: onset exceeds threshold
        bool beatDetected = (onset > sensitivity) && (totalTime_ - lastBeatTime_ > 60.0 / getParamAsFloat ("maxBPM", 200.0f));

        if (beatDetected)
        {
            beatDecay_ = 1.0f;
            beatTimestamps_[beatCount_ % kMaxBeats] = totalTime_;
            beatCount_++;
            lastBeatTime_ = totalTime_;
            phase_ = 0.0f;

            // Estimate BPM from recent beats
            if (beatCount_ >= 4)
            {
                std::array<float, kMaxBeats - 1> intervals;
                int numIntervals = 0;
                int start = juce::jmax (0, beatCount_ - static_cast<int> (kMaxBeats));
                for (int i = start + 1; i < beatCount_ && numIntervals < static_cast<int> (kMaxBeats - 1); ++i)
                {
                    double dt = beatTimestamps_[i % kMaxBeats] - beatTimestamps_[(i - 1) % kMaxBeats];
                    if (dt > 0.15 && dt < 2.0)
                        intervals[numIntervals++] = static_cast<float> (dt);
                }

                if (numIntervals >= 2)
                {
                    // Sort to find median
                    std::sort (intervals.begin(), intervals.begin() + numIntervals);
                    float median = intervals[numIntervals / 2];
                    float bpm = 60.0f / median;
                    float minBPM = getParamAsFloat ("minBPM", 60.0f);
                    float maxBPM = getParamAsFloat ("maxBPM", 200.0f);
                    currentBPM_ = juce::jlimit (minBPM, maxBPM, bpm);
                }
            }
        }
        else
        {
            // Exponential decay
            beatDecay_ *= 0.92f;
            // Phase accumulator
            if (currentBPM_ > 0.0f)
                phase_ += (static_cast<float> (numSamples) / static_cast<float> (sampleRate_)) * (currentBPM_ / 60.0f);
            if (phase_ > 1.0f) phase_ = std::fmod (phase_, 1.0f);
        }

        setSignalOutputValue (0, beatDecay_);
        setSignalOutputValue (1, currentBPM_ / 300.0f); // Normalize to 0-1 range
        setSignalOutputValue (2, phase_);
        setSignalOutputValue (3, juce::jlimit (0.0f, 1.0f, onset / 4.0f));
    }

private:
    static constexpr int kHistorySize = 43;
    static constexpr int kMaxBeats = 32;

    std::array<float, kHistorySize> energyHistory_ {};
    int historyIndex_ = 0;

    float beatDecay_ = 0.0f;
    float lastFlux_ = 0.0f;

    std::array<double, kMaxBeats> beatTimestamps_ {};
    int beatCount_ = 0;
    float currentBPM_ = 120.0f;
    float phase_ = 0.0f;
    double lastBeatTime_ = 0.0;
    double totalTime_ = 0.0;

    std::vector<float> prevMagnitudes_;
};

} // namespace pf
