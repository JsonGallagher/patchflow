#include "Nodes/Audio/FFTAnalyzerNode.h"
#include <cmath>

namespace pf
{

void FFTAnalyzerNode::prepareToPlay (double sampleRate, int blockSize)
{
    NodeBase::prepareToPlay (sampleRate, blockSize);

    int order = getParamAsInt ("fftOrder", 11);
    rebuildFFT (order);

    // Output buffer: fftSize/2 magnitude bins
    resizeBufferOutput (0, fftSize_ / 2);
    writePos_ = 0;
}

void FFTAnalyzerNode::rebuildFFT (int order)
{
    order = juce::jlimit (9, kMaxFFTOrder, order);
    fftOrder_ = order;
    fftSize_  = 1 << order;
    fft_ = std::make_unique<juce::dsp::FFT> (order);

    // Build Hann window (default)
    for (int i = 0; i < fftSize_; ++i)
    {
        float t = static_cast<float> (i) / static_cast<float> (fftSize_);
        windowBuffer_[i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
    }

    ringBuffer_.fill (0.f);
    fftData_.fill (0.f);
    writePos_ = 0;
}

void FFTAnalyzerNode::applyWindow()
{
    for (int i = 0; i < fftSize_; ++i)
        fftData_[i] *= windowBuffer_[i];
}

void FFTAnalyzerNode::processBlock (int numSamples)
{
    auto* in = getConnectedAudioBuffer (0);
    if (! in) return;

    auto mags = getBufferOutputVec (0);
    int numBins = fftSize_ / 2;

    if (static_cast<int> (mags.size()) != numBins)
        resizeBufferOutput (0, numBins);

    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer_[writePos_] = in[i];
        if (++writePos_ >= fftSize_)
        {
            writePos_ = 0;
            frameReady_ = true;
        }
    }

    if (frameReady_)
    {
        frameReady_ = false;

        // Copy ring buffer to fftData (handle wrap-around)
        for (int i = 0; i < fftSize_; ++i)
            fftData_[i] = ringBuffer_[(writePos_ + i) % fftSize_];

        // Zero the imaginary part
        for (int i = fftSize_; i < fftSize_ * 2; ++i)
            fftData_[i] = 0.f;

        applyWindow();
        fft_->performFrequencyOnlyForwardTransform (fftData_.data());

        // Scale and write magnitudes
        float invSize = 1.0f / static_cast<float> (fftSize_);
        float energy = 0.f;
        auto& magVec = getBufferOutputVec (0);
        for (int i = 0; i < numBins; ++i)
        {
            float mag = fftData_[i] * invSize;
            magVec[i] = mag;
            energy += mag;
        }

        setSignalOutputValue (0, energy);
    }
}

} // namespace pf
