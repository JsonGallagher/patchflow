#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class ColorGradeNode : public NodeBase
{
public:
    ColorGradeNode()
    {
        addInput  ("texture",    PortType::Texture);
        addInput  ("hue_shift",  PortType::Visual);
        addInput  ("saturation", PortType::Visual);
        addInput  ("brightness", PortType::Visual);
        addOutput ("texture",    PortType::Texture);

        addParam ("hueShift",   0.0f, -1.0f, 1.0f, "Hue Shift", "Shift hue around color wheel", "", "Color");
        addParam ("saturation", 1.0f, 0.0f, 3.0f, "Saturation", "Color saturation", "x", "Color");
        addParam ("brightness", 1.0f, 0.0f, 3.0f, "Brightness", "Overall brightness", "x", "Color");
        addParam ("contrast",   1.0f, 0.0f, 3.0f, "Contrast", "Contrast adjustment", "x", "Tone");
        addParam ("gamma",      1.0f, 0.1f, 3.0f, "Gamma", "Gamma correction", "", "Tone");
        addParam ("invert",     0, 0, 1, "Invert", "Invert colors", "", "Tone",
                  juce::StringArray { "Off", "On" });
    }

    juce::String getTypeId()      const override { return "ColorGrade"; }
    juce::String getDisplayName() const override { return "Color Grade"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbo_ = 0, fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
    juce::uint32 shaderProgram_ = 0, quadVBO_ = 0, fallbackTexture_ = 0;
    bool shaderCompiled_ = false, shaderError_ = false;
};

} // namespace pf
