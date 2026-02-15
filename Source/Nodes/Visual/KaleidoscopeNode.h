#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class KaleidoscopeNode : public NodeBase
{
public:
    KaleidoscopeNode()
    {
        addInput  ("texture",  PortType::Texture);
        addInput  ("segments", PortType::Visual);
        addInput  ("rotate",   PortType::Visual);
        addInput  ("zoom",     PortType::Visual);
        addOutput ("texture",  PortType::Texture);

        addParam ("segments", 6, 2, 24, "Segments", "Number of kaleidoscope slices", "", "Kaleidoscope");
        addParam ("rotationDeg", 0.0f, -180.0f, 180.0f, "Rotation", "Rotation angle", "deg", "Kaleidoscope");
        addParam ("zoom", 1.0f, 0.1f, 4.0f, "Zoom", "Zoom factor", "x", "Kaleidoscope");
        addParam ("mirror", 1, 0, 1, "Mirror", "Mirror alternate segments", "", "Options",
                  juce::StringArray { "Off", "On" });
        addParam ("wrap", 1, 0, 1, "Wrap", "Edge behavior", "", "Options",
                  juce::StringArray { "Clamp", "Repeat" });
    }

    juce::String getTypeId()      const override { return "Kaleidoscope"; }
    juce::String getDisplayName() const override { return "Kaleidoscope"; }
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
