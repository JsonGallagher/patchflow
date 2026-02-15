#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class TransformNode : public NodeBase
{
public:
    TransformNode()
    {
        addInput  ("texture", PortType::Texture);
        addInput  ("tx",      PortType::Visual);
        addInput  ("ty",      PortType::Visual);
        addInput  ("rotate",  PortType::Visual);
        addInput  ("scale",   PortType::Visual);
        addOutput ("texture", PortType::Texture);

        addParam ("tx", 0.0f, -1.0f, 1.0f, "Translate X", "Horizontal offset", "", "Transform");
        addParam ("ty", 0.0f, -1.0f, 1.0f, "Translate Y", "Vertical offset", "", "Transform");
        addParam ("rotationDeg", 0.0f, -180.0f, 180.0f, "Rotation", "Rotation angle", "deg", "Transform");
        addParam ("scale", 1.0f, 0.1f, 4.0f, "Scale", "Zoom factor", "x", "Transform");
        addParam ("wrap", 0, 0, 1, "Wrap Mode", "Edge behavior", "", "Edge",
                  juce::StringArray { "Clamp", "Repeat" });
    }

    juce::String getTypeId()      const override { return "Transform"; }
    juce::String getDisplayName() const override { return "Transform"; }
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
