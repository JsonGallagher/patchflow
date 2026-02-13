#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class OutputCanvasNode : public NodeBase
{
public:
    OutputCanvasNode()
    {
        addInput ("texture", PortType::Texture);
        addParam ("bgR", 0.0f, 0.0f, 1.0f);
        addParam ("bgG", 0.0f, 0.0f, 1.0f);
        addParam ("bgB", 0.0f, 0.0f, 1.0f);
    }

    juce::String getTypeId()      const override { return "OutputCanvas"; }
    juce::String getDisplayName() const override { return "Output Canvas"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

    juce::uint32 getInputTexture() const
    {
        return isInputConnected (0) ? getConnectedTexture (0) : 0;
    }
};

} // namespace pf
