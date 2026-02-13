#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class SpectrumRendererNode : public NodeBase
{
public:
    SpectrumRendererNode()
    {
        addInput  ("magnitudes", PortType::Buffer);
        addInput  ("color_r",    PortType::Visual);
        addInput  ("color_g",    PortType::Visual);
        addInput  ("color_b",    PortType::Visual);
        addOutput ("texture",    PortType::Texture);
        addParam  ("scale",    0, 0, 1);     // 0=linear, 1=log
        addParam  ("barStyle", 0, 0, 2);     // 0=bars, 1=smooth, 2=filled
        addParam  ("dbRange",  -60.0f, -90.0f, 0.0f);
    }

    juce::String getTypeId()      const override { return "SpectrumRenderer"; }
    juce::String getDisplayName() const override { return "Spectrum Renderer"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

    // Called from GUI thread with latest magnitudes from analysis snapshot
    void updateMagnitudes (const float* data, int numBins)
    {
        if (data && numBins > 0)
        {
            magnitudeSnapshot_.resize (static_cast<size_t> (numBins));
            std::memcpy (magnitudeSnapshot_.data(), data, sizeof (float) * numBins);
        }
    }

private:
    void ensureFBO (juce::OpenGLContext& gl, int width, int height);

    std::vector<float> magnitudeSnapshot_;
    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
};

} // namespace pf
