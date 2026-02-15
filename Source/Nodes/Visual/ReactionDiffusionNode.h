#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class ReactionDiffusionNode : public NodeBase
{
public:
    ReactionDiffusionNode()
    {
        addInput  ("feed_mod",      PortType::Visual);
        addInput  ("kill_mod",      PortType::Visual);
        addInput  ("reset_trigger", PortType::Visual);
        addInput  ("seed_texture",  PortType::Texture);
        addOutput ("texture",       PortType::Texture);

        addParam ("feed",     0.055f, 0.01f, 0.1f, "Feed", "Feed rate (A chemical supply)", "", "Simulation");
        addParam ("kill",     0.062f, 0.04f, 0.08f, "Kill", "Kill rate (B chemical removal)", "", "Simulation");
        addParam ("diffuseA", 1.0f, 0.1f, 2.0f, "Diffuse A", "Diffusion rate of chemical A", "x", "Simulation");
        addParam ("diffuseB", 0.5f, 0.1f, 1.5f, "Diffuse B", "Diffusion rate of chemical B", "x", "Simulation");
        addParam ("speed",    4, 1, 16, "Speed", "Simulation steps per frame", "", "Simulation");
        addParam ("preset",   0, 0, 4, "Preset", "Parameter preset", "", "Simulation",
                  juce::StringArray { "Mitosis", "Coral", "Maze", "Spots", "Custom" });
    }

    juce::String getTypeId()      const override { return "ReactionDiffusion"; }
    juce::String getDisplayName() const override { return "Reaction-Diffusion"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 simFBOs_[2] = { 0, 0 };
    juce::uint32 simTextures_[2] = { 0, 0 };
    juce::uint32 renderFBO_ = 0, renderTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
    int renderWidth_ = 0, renderHeight_ = 0;

    juce::uint32 simProgram_ = 0;
    juce::uint32 renderProgram_ = 0;
    juce::uint32 quadVBO_ = 0;
    bool shadersCompiled_ = false, shaderError_ = false;
    int latestIdx_ = 0;
    bool needsSeed_ = true;
    float lastReset_ = 0.0f;
};

} // namespace pf
