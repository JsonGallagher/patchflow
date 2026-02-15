#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class BlendNode : public NodeBase
{
public:
    BlendNode()
    {
        addInput  ("tex_a", PortType::Texture);
        addInput  ("tex_b", PortType::Texture);
        addInput  ("mix",   PortType::Visual);
        addOutput ("texture", PortType::Texture);

        addParam ("mode", 0, 0, 4, "Blend Mode", "How layers combine", "", "Blending",
                  juce::StringArray { "Mix", "Add", "Multiply", "Screen", "Difference" });
        addParam ("mix", 0.5f, 0.0f, 1.0f, "Mix", "Balance between inputs", "", "Blending");
    }

    juce::String getTypeId()      const override { return "Blend"; }
    juce::String getDisplayName() const override { return "Blend"; }
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
