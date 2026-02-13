#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace pf
{

static constexpr int kMaxFFTSize = 8192;
static constexpr int kAnalysisFIFOSize = 4;

struct AnalysisFrame
{
    std::array<float, kMaxFFTSize / 2> magnitudes {};
    int numBins = 0;
    float envelope = 0.f;
    float bands[5] = {};  // sub, low, mid, high, presence

    // Latest audio block snapshot for waveform display
    std::array<float, 4096> waveform {};
    int waveformSize = 0;
};

/**
 * Lock-free SPSC FIFO for passing analysis data from audio thread to GUI thread.
 * Uses JUCE's AbstractFifo for correct lock-free indexing.
 */
class AnalysisFIFO
{
public:
    /** Audio thread writes a frame. */
    void pushFrame (const AnalysisFrame& frame)
    {
        auto scope = fifo_.write (1);
        if (scope.blockSize1 > 0)
            buffer_[scope.startIndex1] = frame;
    }

    /** GUI thread drains to latest frame. Returns true if a frame was read. */
    bool popLatestFrame (AnalysisFrame& out)
    {
        bool got = false;
        int numReady = fifo_.getNumReady();
        if (numReady == 0) return false;

        auto scope = fifo_.read (numReady);
        if (scope.blockSize2 > 0)
        {
            out = buffer_[scope.startIndex2 + scope.blockSize2 - 1];
            got = true;
        }
        else if (scope.blockSize1 > 0)
        {
            out = buffer_[scope.startIndex1 + scope.blockSize1 - 1];
            got = true;
        }
        return got;
    }

private:
    juce::AbstractFifo fifo_ { kAnalysisFIFOSize };
    std::array<AnalysisFrame, kAnalysisFIFOSize> buffer_;
};

/**
 * Snapshot of latest analysis data, read by GL thread.
 * Written by GUI thread after consuming AnalysisFIFO.
 */
struct AnalysisSnapshot
{
    AnalysisFrame latestFrame;
    bool hasData = false;
};

} // namespace pf
