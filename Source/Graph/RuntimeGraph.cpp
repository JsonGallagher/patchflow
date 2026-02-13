#include "Graph/RuntimeGraph.h"

namespace pf
{

void RuntimeGraph::processAudioBlock (int numSamples)
{
    for (auto* node : audioProcessOrder_)
    {
        if (! node->isBypassed())
            node->processBlock (numSamples);
    }
}

void RuntimeGraph::processVisualFrame (juce::OpenGLContext& gl)
{
    for (auto* node : visualProcessOrder_)
    {
        if (! node->isBypassed())
            node->renderFrame (gl);
    }
}

void RuntimeGraph::prepareToPlay (double sampleRate, int blockSize)
{
    for (auto& node : nodes_)
        node->prepareToPlay (sampleRate, blockSize);
}

NodeBase* RuntimeGraph::findNode (const juce::String& nodeId) const
{
    for (auto& node : nodes_)
        if (node->nodeId == nodeId)
            return node.get();
    return nullptr;
}

} // namespace pf
