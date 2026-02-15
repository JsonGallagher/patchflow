#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class BloomNode : public NodeBase
{
public:
    BloomNode()
    {
        addInput  ("texture",   PortType::Texture);
        addInput  ("threshold", PortType::Visual);
        addInput  ("intensity", PortType::Visual);
        addInput  ("radius",    PortType::Visual);
        addOutput ("texture",   PortType::Texture);

        addParam ("threshold", 0.6f, 0.0f, 1.0f, "Threshold", "Brightness cutoff for bloom", "", "Bloom");
        addParam ("intensity", 0.9f, 0.0f, 2.5f, "Intensity", "Bloom glow strength", "", "Bloom");
        addParam ("radius", 1.6f, 0.1f, 6.0f, "Radius", "Bloom spread size", "px", "Bloom");
    }

    juce::String getTypeId()      const override { return "Bloom"; }
    juce::String getDisplayName() const override { return "Bloom"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    void ensureFBO (juce::OpenGLContext& gl, int width, int height);
    void compileShader (juce::OpenGLContext& gl);
    void ensureFallbackTexture();

    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0;
    int fboHeight_ = 0;

    juce::uint32 shaderProgram_ = 0;
    juce::uint32 quadVBO_ = 0;
    juce::uint32 fallbackTexture_ = 0;
    bool shaderError_ = false;
};

} // namespace pf
