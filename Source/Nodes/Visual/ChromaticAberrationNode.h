#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class ChromaticAberrationNode : public NodeBase
{
public:
    ChromaticAberrationNode()
    {
        addInput  ("texture", PortType::Texture);
        addInput  ("amount",  PortType::Visual);
        addOutput ("texture", PortType::Texture);

        addParam ("amount", 0.005f, 0.0f, 0.05f, "Amount", "RGB split distance", "", "Effect");
        addParam ("angle",  0.0f, -180.0f, 180.0f, "Angle", "Split direction", "deg", "Effect");
        addParam ("radial", 0, 0, 1, "Mode", "Aberration mode", "", "Effect",
                  juce::StringArray { "Directional", "Radial" });
    }

    juce::String getTypeId()      const override { return "ChromaticAberration"; }
    juce::String getDisplayName() const override { return "Chromatic Aberration"; }
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
