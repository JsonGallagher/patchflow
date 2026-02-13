#include "Audio/AudioEngine.h"
#include "Nodes/Audio/FFTAnalyzerNode.h"
#include "Nodes/Audio/EnvelopeFollowerNode.h"
#include "Nodes/Audio/BandSplitterNode.h"
#include <cstring>

namespace pf
{

namespace
{
juce::String findPreferredBlackHoleInput (juce::AudioDeviceManager& deviceManager)
{
    const auto& types = deviceManager.getAvailableDeviceTypes();

    juce::String fallbackMatch;
    for (auto* type : types)
    {
        if (type == nullptr)
            continue;

        type->scanForDevices();
        const auto inputNames = type->getDeviceNames (true);

        for (const auto& inputName : inputNames)
        {
            if (inputName.equalsIgnoreCase ("BlackHole 2ch"))
                return inputName;

            if (fallbackMatch.isEmpty() && inputName.containsIgnoreCase ("BlackHole"))
                fallbackMatch = inputName;
        }
    }

    return fallbackMatch;
}
} // namespace

AudioEngine::AudioEngine()
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    auto result = deviceManager_.initialiseWithDefaultDevices (2, 0); // 2 inputs, 0 outputs
    if (result.isNotEmpty())
        DBG ("AudioEngine init error: " + result);

    const auto preferredInput = findPreferredBlackHoleInput (deviceManager_);
    if (preferredInput.isNotEmpty())
    {
        auto setup = deviceManager_.getAudioDeviceSetup();
        if (setup.inputDeviceName != preferredInput)
        {
            setup.inputDeviceName = preferredInput;

            // Ensure the input is actually enabled when switching devices.
            if (setup.inputChannels.countNumberOfSetBits() == 0)
                setup.inputChannels.setRange (0, 2, true);

            auto setResult = deviceManager_.setAudioDeviceSetup (setup, true);
            if (setResult.isNotEmpty())
                DBG ("AudioEngine BlackHole input setup error: " + setResult);
            else
                DBG ("AudioEngine: default input set to " + preferredInput);
        }
    }

    deviceManager_.addAudioCallback (this);
}

void AudioEngine::shutdown()
{
    deviceManager_.removeAudioCallback (this);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate_ = device->getCurrentSampleRate();
    blockSize_  = device->getCurrentBufferSizeSamples();
    DBG ("AudioEngine: SR=" + juce::String (sampleRate_) + " BS=" + juce::String (blockSize_));
}

void AudioEngine::audioDeviceStopped()
{
    DBG ("AudioEngine: device stopped");
}

void AudioEngine::audioDeviceIOCallbackWithContext (
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    // Check for new graph (atomic swap)
    RuntimeGraph* newGraph = pendingGraph_.exchange (nullptr, std::memory_order_acquire);
    if (newGraph)
    {
        // Old localGraph_ lifecycle managed externally (GraphCompiler holds the unique_ptr)
        localGraph_ = newGraph;
    }

    // Silence outputs
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch])
            std::memset (outputChannelData[ch], 0, sizeof (float) * static_cast<size_t> (numSamples));

    if (! localGraph_) return;

    // Find AudioInput node and write device input into its buffers
    for (auto* node : localGraph_->getAudioProcessOrder())
    {
        if (node->getTypeId() == "AudioInput")
        {
            auto* inputNode = static_cast<AudioInputNode*> (node);
            auto* outL = inputNode->getAudioOutputBuffer (0);
            auto* outR = inputNode->getAudioOutputBuffer (1);

            if (outL && numInputChannels > 0 && inputChannelData[0])
                std::memcpy (outL, inputChannelData[0], sizeof (float) * static_cast<size_t> (numSamples));
            else if (outL)
                std::memset (outL, 0, sizeof (float) * static_cast<size_t> (numSamples));

            if (outR && numInputChannels > 1 && inputChannelData[1])
                std::memcpy (outR, inputChannelData[1], sizeof (float) * static_cast<size_t> (numSamples));
            else if (outR)
                std::memset (outR, 0, sizeof (float) * static_cast<size_t> (numSamples));

            break;
        }
    }

    // Process the audio graph
    localGraph_->processAudioBlock (numSamples);

    // Gather analysis data into a frame and push to FIFO
    currentFrame_ = {};
    bool hasAnalysis = false;

    for (auto* node : localGraph_->getAudioProcessOrder())
    {
        if (node->getTypeId() == "FFTAnalyzer")
        {
            auto data = node->getBufferOutputData (0);
            if (! data.empty())
            {
                currentFrame_.numBins = static_cast<int> (data.size());
                int copySize = juce::jmin (currentFrame_.numBins,
                                           static_cast<int> (currentFrame_.magnitudes.size()));
                std::memcpy (currentFrame_.magnitudes.data(), data.data(),
                             sizeof (float) * static_cast<size_t> (copySize));
                hasAnalysis = true;
            }
        }
        else if (node->getTypeId() == "EnvelopeFollower")
        {
            currentFrame_.envelope = node->getSignalOutputValue (0);
            hasAnalysis = true;
        }
        else if (node->getTypeId() == "BandSplitter")
        {
            for (int i = 0; i < 5; ++i)
                currentFrame_.bands[i] = node->getSignalOutputValue (i);
            hasAnalysis = true;
        }
    }

    // Snapshot waveform from first AudioInput
    for (auto* node : localGraph_->getAudioProcessOrder())
    {
        if (node->getTypeId() == "AudioInput")
        {
            auto* buf = node->getAudioOutputBuffer (0);
            if (buf)
            {
                int copySize = juce::jmin (numSamples, static_cast<int> (currentFrame_.waveform.size()));
                std::memcpy (currentFrame_.waveform.data(), buf, sizeof (float) * static_cast<size_t> (copySize));
                currentFrame_.waveformSize = copySize;
                hasAnalysis = true;
            }
            break;
        }
    }

    if (hasAnalysis)
        analysisFifo_.pushFrame (currentFrame_);
}

} // namespace pf
