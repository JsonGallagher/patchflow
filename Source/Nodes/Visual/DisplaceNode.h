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

        addParam ("amount", 0.03f, 0.0f, 0.5f, "Amount", "Displacement strength", "", "Displacement");
        addParam ("rotationDeg", 0.0f, -180.0f, 180.0f, "Rotation", "Displacement direction", "deg", "Displacement");
        addParam ("mode", 1, 0, 1, "Mode", "How displacement is computed", "", "Displacement",
                  juce::StringArray { "Luma Axis", "RG Vector" });
        addParam ("wrap", 1, 0, 1, "Wrap", "Edge behavior", "", "Edge",
                  juce::StringArray { "Clamp", "Repeat" });
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
