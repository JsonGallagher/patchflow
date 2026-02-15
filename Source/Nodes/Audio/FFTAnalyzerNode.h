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
        addInput  ("in_L",       PortType::Audio);
        addInput  ("in_R",       PortType::Audio);
        addOutput ("magnitudes", PortType::Buffer);
        addOutput ("energy",     PortType::Signal);
        addParam  ("fftOrder",   11, 9, 13, "FFT Size", "Frequency resolution (2^N samples)", "",
                   "", juce::StringArray { "512", "1024", "2048", "4096", "8192" });
        addParam  ("windowType", 0, 0, 2, "Window", "Windowing function applied before FFT", "",
                   "", juce::StringArray { "Hann", "Hamming", "Blackman" });
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
