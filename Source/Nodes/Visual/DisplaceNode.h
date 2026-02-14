#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class DisplaceNode : public NodeBase
{
public:
    DisplaceNode()
    {
        addInput  ("source",       PortType::Texture);
        addInput  ("displacement", PortType::Texture);
        addInput  ("amount",       PortType::Visual);
        addInput  ("rotate",       PortType::Visual);
        addOutput ("texture",      PortType::Texture);

        addParam ("amount", 0.03f, 0.0f, 0.5f);
        addParam ("rotationDeg", 0.0f, -180.0f, 180.0f);
        addParam ("mode", 1, 0, 1); // 0=luma axis, 1=RG vector
        addParam ("wrap", 1, 0, 1);
    }

    juce::String getTypeId()      const override { return "Displace"; }
    juce::String getDisplayName() const override { return "Displace"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    void ensureFBO (juce::OpenGLContext& gl, int width, int height);
    void compileShader (juce::OpenGLContext& gl);
    void ensureFallbackTextures();

    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0;
    int fboHeight_ = 0;

    juce::uint32 shaderProgram_ = 0;
    juce::uint32 quadVBO_ = 0;
    juce::uint32 fallbackSourceTexture_ = 0;
    juce::uint32 fallbackDisplacementTexture_ = 0;
    bool shaderError_ = false;
};

} // namespace pf
