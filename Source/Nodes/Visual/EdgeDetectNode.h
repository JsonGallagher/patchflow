#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class EdgeDetectNode : public NodeBase
{
public:
    EdgeDetectNode()
    {
        addInput  ("texture",  PortType::Texture);
        addInput  ("strength", PortType::Visual);
        addOutput ("texture",  PortType::Texture);

        addParam ("mode",     0, 0, 1, "Mode", "Edge detection algorithm", "", "Edge",
                  juce::StringArray { "Sobel", "Laplacian" });
        addParam ("strength", 1.0f, 0.0f, 5.0f, "Strength", "Edge intensity", "x", "Edge");
        addParam ("invert",   0, 0, 1, "Invert", "Invert output", "", "Edge",
                  juce::StringArray { "Off", "On" });
        addParam ("overlay",  0, 0, 1, "Overlay", "Show edges over source", "", "Edge",
                  juce::StringArray { "Edges Only", "Over Source" });
    }

    juce::String getTypeId()      const override { return "EdgeDetect"; }
    juce::String getDisplayName() const override { return "Edge Detect"; }
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
