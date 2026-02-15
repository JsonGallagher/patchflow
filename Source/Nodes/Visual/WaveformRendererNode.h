#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class WaveformRendererNode : public NodeBase
{
public:
    WaveformRendererNode()
    {
        addInput  ("audio_L",   PortType::Audio);
        addInput  ("color_r",   PortType::Visual);
        addInput  ("color_g",   PortType::Visual);
        addInput  ("color_b",   PortType::Visual);
        addInput  ("thickness", PortType::Visual);
        addOutput ("texture",   PortType::Texture);
        addParam  ("lineThickness", 2.0f, 1.0f, 5.0f, "Thickness", "Waveform line weight", "px", "Style");
        addParam  ("style", 0, 0, 2, "Style", "Waveform visualization style", "", "Style",
                   juce::StringArray { "Line", "Filled", "Mirrored" });
    }

    juce::String getTypeId()      const override { return "WaveformRenderer"; }
    juce::String getDisplayName() const override { return "Waveform Renderer"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void prepareToPlay (double sampleRate, int blockSize) override
    {
        NodeBase::prepareToPlay (sampleRate, blockSize);
        waveformSnapshot_.resize (static_cast<size_t> (blockSize), 0.f);
    }

    void renderFrame (juce::OpenGLContext& gl) override;

    // Called from GUI thread to snapshot latest audio block
    void updateWaveformSnapshot (const float* data, int numSamples)
    {
        if (data && numSamples > 0)
        {
            waveformSnapshot_.resize (static_cast<size_t> (numSamples));
            std::memcpy (waveformSnapshot_.data(), data, sizeof (float) * numSamples);
        }
    }

private:
    void ensureFBO (juce::OpenGLContext& gl, int width, int height);

    std::vector<float> waveformSnapshot_;
    std::vector<float> smoothedWaveform_;
    float rmsLevel_ = 0.0f;
    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
};

} // namespace pf
