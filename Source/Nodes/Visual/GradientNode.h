#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class GradientNode : public NodeBase
{
public:
    GradientNode()
    {
        addInput  ("rotation", PortType::Visual);
        addInput  ("offset",   PortType::Visual);
        addInput  ("spread",   PortType::Visual);
        addOutput ("texture",  PortType::Texture);

        addParam ("gradientType", 0, 0, 3, "Type", "Gradient shape", "", "Gradient",
                  juce::StringArray { "Linear", "Radial", "Angular", "Diamond" });
        addParam ("rotation",     0.0f, -180.0f, 180.0f, "Rotation", "Gradient angle", "deg", "Gradient");
        addParam ("offset",       0.0f, -1.0f, 1.0f, "Offset", "Center offset", "", "Gradient");
        addParam ("spread",       1.0f, 0.1f, 4.0f, "Spread", "Gradient spread", "x", "Gradient");
        addParam ("colorA_r",     0.0f, 0.0f, 1.0f, "Color A Red", "Start color red", "", "Colors");
        addParam ("colorA_g",     0.0f, 0.0f, 1.0f, "Color A Green", "Start color green", "", "Colors");
        addParam ("colorA_b",     0.0f, 0.0f, 1.0f, "Color A Blue", "Start color blue", "", "Colors");
        addParam ("colorB_r",     1.0f, 0.0f, 1.0f, "Color B Red", "End color red", "", "Colors");
        addParam ("colorB_g",     1.0f, 0.0f, 1.0f, "Color B Green", "End color green", "", "Colors");
        addParam ("colorB_b",     1.0f, 0.0f, 1.0f, "Color B Blue", "End color blue", "", "Colors");
    }

    juce::String getTypeId()      const override { return "Gradient"; }
    juce::String getDisplayName() const override { return "Gradient"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbo_ = 0, fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
    juce::uint32 shaderProgram_ = 0, quadVBO_ = 0;
    bool shaderCompiled_ = false, shaderError_ = false;
};

} // namespace pf
