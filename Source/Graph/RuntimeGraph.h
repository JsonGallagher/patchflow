#pragma once
#include "Nodes/NodeBase.h"
#include "Graph/Connection.h"
#include <memory>
#include <vector>

namespace pf
{

/**
 * Immutable compiled execution plan, consumed by AudioEngine and VisualCanvas.
 * Built by GraphCompiler, published via atomic pointer swap.
 */
class RuntimeGraph
{
public:
    RuntimeGraph() = default;

    /** Process all audio/control-rate nodes in topological order. */
    void processAudioBlock (int numSamples);

    /** Process all visual nodes in topological order. */
    void processVisualFrame (juce::OpenGLContext& gl);

    /** Prepare all nodes for playback. */
    void prepareToPlay (double sampleRate, int blockSize);

    //==============================================================================
    // Accessors used by AudioEngine / VisualCanvas
    NodeBase* findNode (const juce::String& nodeId) const;
    const std::vector<NodeBase*>& getAudioProcessOrder() const  { return audioProcessOrder_; }
    const std::vector<NodeBase*>& getVisualProcessOrder() const { return visualProcessOrder_; }
    const std::vector<std::unique_ptr<NodeBase>>& getAllNodes() const { return nodes_; }

private:
    friend class GraphCompiler;

    std::vector<std::unique_ptr<NodeBase>> nodes_;
    std::vector<NodeBase*> audioProcessOrder_;
    std::vector<NodeBase*> visualProcessOrder_;
};

} // namespace pf
