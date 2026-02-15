#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class GlitchNode : public NodeBase
{
public:
    GlitchNode()
    {
        addInput  ("texture",   PortType::Texture);
        addInput  ("intensity", PortType::Visual);
        addInput  ("trigger",   PortType::Visual);
        addOutput ("texture",   PortType::Texture);

        addParam ("intensity",   0.3f, 0.0f, 1.0f, "Intensity", "Overall glitch amount", "", "Glitch");
        addParam ("blockSize",   0.05f, 0.01f, 0.2f, "Block Size", "Size of displaced blocks", "", "Glitch");
        addParam ("rgbSplit",    0.01f, 0.0f, 0.05f, "RGB Split", "Channel separation amount", "", "Glitch");
        addParam ("scanlines",   0.5f, 0.0f, 1.0f, "Scanlines", "Scanline overlay intensity", "", "Glitch");
        addParam ("noiseAmount", 0.1f, 0.0f, 1.0f, "Noise", "Random noise injection", "", "Glitch");
        addParam ("speed",       1.0f, 0.1f, 5.0f, "Speed", "Animation speed", "x", "Glitch");
    }

    juce::String getTypeId()      const override { return "Glitch"; }
    juce::String getDisplayName() const override { return "Glitch"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbo_ = 0, fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
    juce::uint32 shaderProgram_ = 0, quadVBO_ = 0, fallbackTexture_ = 0;
    bool shaderCompiled_ = false, shaderError_ = false;
    float time_ = 0.f;
};

} // namespace pf
