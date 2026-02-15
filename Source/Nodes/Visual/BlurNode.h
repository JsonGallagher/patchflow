#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class BlurNode : public NodeBase
{
public:
    BlurNode()
    {
        addInput  ("texture",   PortType::Texture);
        addInput  ("amount",    PortType::Visual);
        addInput  ("direction", PortType::Visual);
        addOutput ("texture",   PortType::Texture);

        addParam ("mode",         0, 0, 2, "Mode", "Blur algorithm", "", "Blur",
                  juce::StringArray { "Gaussian", "Radial", "Directional" });
        addParam ("amount",       1.0f, 0.0f, 8.0f, "Amount", "Blur radius", "px", "Blur");
        addParam ("passes",       2, 1, 4, "Passes", "Number of blur passes", "", "Blur");
        addParam ("directionDeg", 0.0f, -180.0f, 180.0f, "Direction", "Motion blur angle", "deg", "Blur");
        addParam ("center_x",    0.5f, 0.0f, 1.0f, "Center X", "Radial blur center X", "", "Blur");
        addParam ("center_y",    0.5f, 0.0f, 1.0f, "Center Y", "Radial blur center Y", "", "Blur");
    }

    juce::String getTypeId()      const override { return "Blur"; }
    juce::String getDisplayName() const override { return "Blur"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbos_[2] = { 0, 0 };
    juce::uint32 textures_[2] = { 0, 0 };
    int fboWidth_ = 0, fboHeight_ = 0;

    juce::uint32 gaussianProgram_ = 0;
    juce::uint32 radialProgram_ = 0;
    juce::uint32 directionalProgram_ = 0;
    juce::uint32 quadVBO_ = 0, fallbackTexture_ = 0;
    bool shadersCompiled_ = false, shaderError_ = false;
};

} // namespace pf
