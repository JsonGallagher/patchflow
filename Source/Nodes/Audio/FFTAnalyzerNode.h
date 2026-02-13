#pragma once
#include "Nodes/NodeBase.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace pf
{

class FFTAnalyzerNode : public NodeBase
{
public:
    static constexpr int kMaxFFTOrder = 13;
    static constexpr int kMaxFFTSize  = 1 << kMaxFFTOrder; // 8192

    FFTAnalyzerNode()
    {
        addInput  ("in",         PortType::Audio);
        addOutput ("magnitudes", PortType::Buffer);
        addOutput ("energy",     PortType::Signal);
        addParam  ("fftOrder",   11, 9, 13);
        addParam  ("windowType", 0, 0, 2); // 0=Hann, 1=Hamming, 2=Blackman
    }

    juce::String getTypeId()      const override { return "FFTAnalyzer"; }
    juce::String getDisplayName() const override { return "FFT Analyzer"; }
    juce::String getCategory()    const override { return "Audio"; }

    void prepareToPlay (double sampleRate, int blockSize) override;
    void processBlock (int numSamples) override;

private:
    void rebuildFFT (int order);
    void applyWindow();

    std::unique_ptr<juce::dsp::FFT> fft_;
    int fftOrder_ = 11;
    int fftSize_  = 2048;

    std::array<float, kMaxFFTSize>     ringBuffer_ {};
    std::array<float, kMaxFFTSize * 2> fftData_ {};
    std::array<float, kMaxFFTSize>     windowBuffer_ {};
    int writePos_ = 0;
    bool frameReady_ = false;
};

} // namespace pf
