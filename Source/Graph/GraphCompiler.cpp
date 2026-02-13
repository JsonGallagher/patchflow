#include "Graph/GraphCompiler.h"
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace pf
{

GraphCompiler::GraphCompiler (GraphModel& model) : model_ (model)
{
    model_.addListener (this);
}

GraphCompiler::~GraphCompiler()
{
    model_.removeListener (this);
    pendingGraph_.store (nullptr, std::memory_order_release);
}

void GraphCompiler::graphChanged()
{
    compile();
}

void GraphCompiler::compile()
{
    auto graph = buildRuntimeGraph();
    if (! graph)
        return;

    graph->prepareToPlay (currentSampleRate_, currentBlockSize_);
    RuntimeGraph* graphPtr = graph.get();
    compiledGraphs_.push_back (std::move (graph));
    latestGraph_ = graphPtr;

    // Publish for audio thread. We retain ownership for app lifetime to ensure
    // GL/audio threads never observe a graph that has been deleted.
    RuntimeGraph* old = pendingGraph_.exchange (graphPtr, std::memory_order_release);
    juce::ignoreUnused (old);
}

bool GraphCompiler::hasCycle (const std::vector<juce::String>& nodeIds,
                               const std::vector<Connection>& connections)
{
    // DFS with white(0)/gray(1)/black(2) coloring
    std::unordered_map<std::string, int> color;
    std::unordered_map<std::string, std::vector<std::string>> adj;

    for (auto& id : nodeIds)
        color[id.toStdString()] = 0;

    for (auto& conn : connections)
        adj[conn.sourceNode.toStdString()].push_back (conn.destNode.toStdString());

    std::function<bool(const std::string&)> dfs = [&] (const std::string& u) -> bool
    {
        color[u] = 1; // gray
        for (auto& v : adj[u])
        {
            if (color[v] == 1) return true;  // back edge → cycle
            if (color[v] == 0 && dfs (v)) return true;
        }
        color[u] = 2; // black
        return false;
    };

    for (auto& id : nodeIds)
        if (color[id.toStdString()] == 0 && dfs (id.toStdString()))
            return true;

    return false;
}

std::vector<juce::String> GraphCompiler::topologicalSort (
    const std::vector<juce::String>& nodeIds,
    const std::vector<Connection>& connections)
{
    // Kahn's algorithm
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adj;

    for (auto& id : nodeIds)
        inDegree[id.toStdString()] = 0;

    for (auto& conn : connections)
    {
        adj[conn.sourceNode.toStdString()].push_back (conn.destNode.toStdString());
        inDegree[conn.destNode.toStdString()]++;
    }

    std::queue<std::string> queue;
    for (auto& [id, deg] : inDegree)
        if (deg == 0)
            queue.push (id);

    std::vector<juce::String> sorted;
    sorted.reserve (nodeIds.size());

    while (! queue.empty())
    {
        auto id = queue.front();
        queue.pop();
        sorted.push_back (juce::String (id));

        for (auto& neighbor : adj[id])
            if (--inDegree[neighbor] == 0)
                queue.push (neighbor);
    }

    return sorted;
}

std::unique_ptr<RuntimeGraph> GraphCompiler::buildRuntimeGraph()
{
    auto nodeIds = model_.getAllNodeIds();
    auto connections = model_.getAllConnections();

    // Cycle detection
    if (hasCycle (nodeIds, connections))
    {
        hasError_ = true;
        errorMessage_ = "Cycle detected in graph!";
        return nullptr;
    }

    hasError_ = false;
    errorMessage_ = {};

    auto sortedIds = topologicalSort (nodeIds, connections);

    auto graph = std::make_unique<RuntimeGraph>();

    // Instantiate nodes
    std::unordered_map<std::string, NodeBase*> nodeMap;
    for (auto& id : sortedIds)
    {
        auto typeId = model_.getNodeTypeId (id);
        auto node = NodeRegistry::instance().createNode (typeId);
        if (! node) continue;

        node->nodeId = id;

        // Apply params from model
        auto paramsTree = model_.getParamsTree (id);
        node->setParamTree (paramsTree);

        // Set default param values if not already in tree
        for (auto& param : node->getParams())
        {
            if (! paramsTree.hasProperty (juce::Identifier (param.name)))
                paramsTree.setProperty (juce::Identifier (param.name), param.defaultValue, nullptr);
        }

        nodeMap[id.toStdString()] = node.get();
        graph->nodes_.push_back (std::move (node));
    }

    // Resolve connections
    for (auto& conn : connections)
    {
        auto srcIt = nodeMap.find (conn.sourceNode.toStdString());
        auto dstIt = nodeMap.find (conn.destNode.toStdString());
        if (srcIt == nodeMap.end() || dstIt == nodeMap.end()) continue;

        dstIt->second->setInputConnection (conn.destPort, srcIt->second, conn.sourcePort);
    }

    // Build process orders — partition into audio and visual
    for (auto& id : sortedIds)
    {
        auto it = nodeMap.find (id.toStdString());
        if (it == nodeMap.end()) continue;

        auto* node = it->second;
        if (node->isVisualNode())
            graph->visualProcessOrder_.push_back (node);
        else
            graph->audioProcessOrder_.push_back (node);
    }

    // Mark disconnected nodes as bypassed
    std::unordered_set<std::string> connectedNodes;
    for (auto& conn : connections)
    {
        connectedNodes.insert (conn.sourceNode.toStdString());
        connectedNodes.insert (conn.destNode.toStdString());
    }
    for (auto& [id, node] : nodeMap)
    {
        // AudioInput is never bypassed (it's a source)
        if (node->getTypeId() == "AudioInput" || node->getTypeId() == "OutputCanvas")
            continue;
        if (connectedNodes.find (id) == connectedNodes.end())
            node->setBypassed (true);
    }

    return graph;
}

} // namespace pf
