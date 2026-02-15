#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class TileNode : public NodeBase
{
public:
    TileNode()
    {
        addInput  ("texture",  PortType::Texture);
        addInput  ("count",    PortType::Visual);
        addOutput ("texture",  PortType::Texture);

        addParam ("countX",    2, 1, 16, "Count X", "Horizontal tile count", "", "Tile");
        addParam ("countY",    2, 1, 16, "Count Y", "Vertical tile count", "", "Tile");
        addParam ("offsetX",   0.0f, -1.0f, 1.0f, "Offset X", "Horizontal offset", "", "Tile");
        addParam ("offsetY",   0.0f, -1.0f, 1.0f, "Offset Y", "Vertical offset", "", "Tile");
        addParam ("rotation",  0.0f, -180.0f, 180.0f, "Rotation", "Tile rotation", "deg", "Transform");
        addParam ("mirror",    0, 0, 1, "Mirror", "Alternate tile flip", "", "Tile",
                  juce::StringArray { "Off", "On" });
    }

    juce::String getTypeId()      const override { return "Tile"; }
    juce::String getDisplayName() const override { return "Tile"; }
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
