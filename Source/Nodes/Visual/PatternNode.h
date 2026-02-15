#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class PatternNode : public NodeBase
{
public:
    PatternNode()
    {
        addInput  ("frequency", PortType::Visual);
        addInput  ("rotation",  PortType::Visual);
        addInput  ("thickness", PortType::Visual);
        addOutput ("texture",   PortType::Texture);

        addParam ("patternType", 0, 0, 5, "Pattern", "Pattern type", "", "Pattern",
                  juce::StringArray { "Stripes", "Checkerboard", "Concentric", "Spiral", "Grid Dots", "Hexagonal" });
        addParam ("frequency",   8.0f, 1.0f, 64.0f, "Frequency", "Pattern repetition", "", "Pattern");
        addParam ("rotation",    0.0f, -180.0f, 180.0f, "Rotation", "Pattern angle", "deg", "Pattern");
        addParam ("thickness",   0.5f, 0.01f, 1.0f, "Thickness", "Line/dot thickness", "", "Pattern");
        addParam ("speed",       0.0f, 0.0f, 5.0f, "Speed", "Animation speed", "x", "Animation");
        addParam ("softness",    0.02f, 0.001f, 0.2f, "Softness", "Edge anti-aliasing", "", "Pattern");
    }

    juce::String getTypeId()      const override { return "Pattern"; }
    juce::String getDisplayName() const override { return "Pattern"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbo_ = 0, fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
    juce::uint32 shaderProgram_ = 0, quadVBO_ = 0;
    bool shaderCompiled_ = false, shaderError_ = false;
    float time_ = 0.f;
};

} // namespace pf
