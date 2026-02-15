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

        addParam ("feedback", 0.82f, 0.0f, 0.99f, "Decay", "How much previous frame persists", "", "Feedback");
        addParam ("mix", 1.0f, 0.0f, 1.0f, "Mix", "Blend with input", "", "Feedback");
        addParam ("offsetX", 0.002f, -0.25f, 0.25f, "Offset X", "Horizontal drift per frame", "", "Motion");
        addParam ("offsetY", 0.001f, -0.25f, 0.25f, "Offset Y", "Vertical drift per frame", "", "Motion");
        addParam ("zoom", 1.0f, 0.8f, 1.2f, "Zoom", "Scale per frame", "x", "Motion");
        addParam ("rotationDeg", 0.0f, -45.0f, 45.0f, "Rotation", "Rotation per frame", "deg", "Motion");
        addParam ("wrap", 1, 0, 1, "Wrap", "Edge behavior", "", "Edge",
                  juce::StringArray { "Clamp", "Repeat" });
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
