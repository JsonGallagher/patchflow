#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class FeedbackNode : public NodeBase
{
public:
    FeedbackNode()
    {
        addInput  ("texture", PortType::Texture);
        addInput  ("feedback", PortType::Visual);
        addInput  ("offset_x", PortType::Visual);
        addInput  ("offset_y", PortType::Visual);
        addOutput ("texture", PortType::Texture);

        addParam ("feedback", 0.82f, 0.0f, 0.99f);
        addParam ("mix", 1.0f, 0.0f, 1.0f);
        addParam ("offsetX", 0.002f, -0.25f, 0.25f);
        addParam ("offsetY", 0.001f, -0.25f, 0.25f);
        addParam ("zoom", 1.0f, 0.8f, 1.2f);
        addParam ("rotationDeg", 0.0f, -45.0f, 45.0f);
        addParam ("wrap", 1, 0, 1); // 0=black outside, 1=repeat
    }

    juce::String getTypeId()      const override { return "Feedback"; }
    juce::String getDisplayName() const override { return "Feedback"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    void ensureResources (juce::OpenGLContext& gl, int width, int height);
    void compileShader (juce::OpenGLContext& gl);
    void ensureFallbackTexture();

    juce::uint32 fbos_[2] = { 0, 0 };
    juce::uint32 textures_[2] = { 0, 0 };
    int fboWidth_ = 0;
    int fboHeight_ = 0;
    int latestIndex_ = 0;

    juce::uint32 shaderProgram_ = 0;
    juce::uint32 quadVBO_ = 0;
    juce::uint32 fallbackTexture_ = 0;
    bool shaderError_ = false;
};

} // namespace pf
