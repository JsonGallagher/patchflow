#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class MirrorNode : public NodeBase
{
public:
    MirrorNode()
    {
        addInput  ("texture", PortType::Texture);
        addInput  ("offset",  PortType::Visual);
        addOutput ("texture", PortType::Texture);

        addParam ("mode",     0, 0, 3, "Mode", "Mirror type", "", "Mirror",
                  juce::StringArray { "Horizontal", "Vertical", "Quad", "Radial" });
        addParam ("offset",   0.5f, 0.0f, 1.0f, "Offset", "Mirror axis position", "", "Mirror");
        addParam ("segments", 4, 2, 16, "Segments", "Radial mirror segments", "", "Mirror");
    }

    juce::String getTypeId()      const override { return "Mirror"; }
    juce::String getDisplayName() const override { return "Mirror"; }
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
