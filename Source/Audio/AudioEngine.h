#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "Graph/RuntimeGraph.h"
#include "Audio/AnalysisSnapshot.h"
#include "Nodes/Audio/AudioInputNode.h"
#include <atomic>

namespace pf
{

/**
 * Owns AudioDeviceManager. Runs the real-time audio callback.
 * Consumes RuntimeGraph via atomic pointer swap from GraphCompiler.
 */
class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise();
    void shutdown();

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager_; }

    //==============================================================================
    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    //==============================================================================
    // Graph management (called from GUI thread)
    void setNewGraph (RuntimeGraph* graph)
    {
        RuntimeGraph* old = pendingGraph_.exchange (graph, std::memory_order_release);
        // If there was an unconsumed pending graph, it's now orphaned — caller manages lifecycle
        juce::ignoreUnused (old);
    }

    //==============================================================================
    // Analysis data (read by GUI thread)
    AnalysisFIFO& getAnalysisFIFO() { return analysisFifo_; }

private:
    juce::AudioDeviceManager deviceManager_;
    std::atomic<RuntimeGraph*> pendingGraph_ { nullptr };
    RuntimeGraph* localGraph_ = nullptr;  // Audio thread's cached pointer

    AnalysisFIFO analysisFifo_;
    AnalysisFrame currentFrame_;

    double sampleRate_ = 44100.0;
    int blockSize_ = 512;
};

} // namespace pf
