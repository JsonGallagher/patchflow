#pragma once
#include "Graph/GraphModel.h"
#include "Graph/RuntimeGraph.h"
#include "Nodes/NodeRegistry.h"
#include <atomic>
#include <memory>
#include <vector>

namespace pf
{

/**
 * Listens to GraphModel changes. On change:
 * 1. Validates graph (cycle check)
 * 2. Topological sort
 * 3. Builds new RuntimeGraph with pre-resolved connections
 * 4. Publishes via atomic pointer for audio thread
 */
class GraphCompiler : public GraphModel::Listener
{
public:
    explicit GraphCompiler (GraphModel& model);
    ~GraphCompiler() override;

    void graphChanged() override;

    /** Force recompile now (e.g. on initial load). */
    void compile();

    /** Audio thread calls this to check for a new graph. Returns nullptr if no new graph. */
    RuntimeGraph* consumeNewGraph()
    {
        return pendingGraph_.exchange (nullptr, std::memory_order_acquire);
    }

    /** Get the last successfully compiled graph (for read-only access on GUI/GL thread). */
    RuntimeGraph* getLatestGraph() const { return latestGraph_; }

    bool hasError() const { return hasError_; }
    juce::String getErrorMessage() const { return errorMessage_; }

private:
    std::unique_ptr<RuntimeGraph> buildRuntimeGraph();
    static bool hasCycle (const std::vector<juce::String>& nodeIds,
                          const std::vector<Connection>& connections);
    static std::vector<juce::String> topologicalSort (const std::vector<juce::String>& nodeIds,
                                                       const std::vector<Connection>& connections);

    GraphModel& model_;
    std::atomic<RuntimeGraph*> pendingGraph_ { nullptr };
    RuntimeGraph* latestGraph_ = nullptr;
    std::vector<std::unique_ptr<RuntimeGraph>> compiledGraphs_;

    double currentSampleRate_ = 44100.0;
    int currentBlockSize_ = 512;

    bool hasError_ = false;
    juce::String errorMessage_;

public:
    void setSampleRateAndBlockSize (double sr, int bs)
    {
        currentSampleRate_ = sr;
        currentBlockSize_ = bs;
    }
};

} // namespace pf
