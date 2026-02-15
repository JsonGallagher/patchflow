#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class SDFShapeNode : public NodeBase
{
public:
    SDFShapeNode()
    {
        addInput  ("radius",       PortType::Visual);
        addInput  ("rotation",     PortType::Visual);
        addInput  ("edge_softness", PortType::Visual);
        addInput  ("repetition",   PortType::Visual);
        addOutput ("texture",      PortType::Texture);

        addParam ("shape",         0, 0, 6,   "Shape", "SDF primitive type", "", "Shape",
                  juce::StringArray { "Circle", "Ring", "Triangle", "Square", "Pentagon", "Hexagon", "Star" });
        addParam ("radius",        0.35f, 0.01f, 1.0f, "Radius", "Shape size", "", "Shape");
        addParam ("edgeSoftness",  0.01f, 0.001f, 0.2f, "Edge Softness", "Anti-aliasing width", "", "Shape");
        addParam ("rotation",      0.0f, -180.0f, 180.0f, "Rotation", "Shape rotation", "deg", "Transform");
        addParam ("repeatX",       1, 1, 8, "Repeat X", "Grid repetition horizontal", "", "Grid");
        addParam ("repeatY",       1, 1, 8, "Repeat Y", "Grid repetition vertical", "", "Grid");
        addParam ("ringThickness", 0.05f, 0.005f, 0.3f, "Ring Width", "Ring thickness (Ring shape only)", "", "Shape");
        addParam ("starPoints",    5, 3, 12, "Star Points", "Number of star points", "", "Shape");
        addParam ("fillColor",     0, 0, 3, "Fill", "Fill color mode", "", "Color",
                  juce::StringArray { "White", "Gradient", "Rainbow", "Distance" });
    }

    juce::String getTypeId()      const override { return "SDFShape"; }
    juce::String getDisplayName() const override { return "SDF Shape"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;

    juce::uint32 shaderProgram_ = 0;
    juce::uint32 quadVBO_ = 0;
    bool shaderCompiled_ = false;
    bool shaderError_ = false;
};

} // namespace pf
