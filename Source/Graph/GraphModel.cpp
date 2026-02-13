#include "Graph/GraphModel.h"
#include <juce_core/juce_core.h>

namespace pf
{

GraphModel::GraphModel()
{
    graphTree_ = juce::ValueTree (IDs::GRAPH);
    graphTree_.addChild (juce::ValueTree (IDs::NODES), -1, nullptr);
    graphTree_.addChild (juce::ValueTree (IDs::CONNECTIONS), -1, nullptr);
    graphTree_.addListener (this);
}

juce::String GraphModel::addNode (const juce::String& typeId, float x, float y)
{
    juce::String nodeId = "node_" + juce::String (nextNodeId_++);

    juce::ValueTree node (IDs::NODE);
    node.setProperty (IDs::id, nodeId, nullptr);
    node.setProperty (IDs::typeId, typeId, nullptr);
    node.setProperty (IDs::x, x, nullptr);
    node.setProperty (IDs::y, y, nullptr);
    node.addChild (juce::ValueTree (IDs::PARAMS), -1, nullptr);

    graphTree_.getChildWithName (IDs::NODES).addChild (node, -1, &undoManager_);
    return nodeId;
}

void GraphModel::removeNode (const juce::String& nodeId)
{
    auto nodesTree = graphTree_.getChildWithName (IDs::NODES);
    auto node = getNodeTree (nodeId);
    if (node.isValid())
    {
        // Remove all connections involving this node
        auto connsTree = graphTree_.getChildWithName (IDs::CONNECTIONS);
        for (int i = connsTree.getNumChildren() - 1; i >= 0; --i)
        {
            auto conn = connsTree.getChild (i);
            if (conn[IDs::sourceNode] == nodeId || conn[IDs::destNode] == nodeId)
                connsTree.removeChild (i, &undoManager_);
        }

        nodesTree.removeChild (node, &undoManager_);
    }
}

bool GraphModel::addConnection (const Connection& conn)
{
    // Check for duplicate
    for (auto& existing : getAllConnections())
        if (existing == conn) return false;

    // Check destination port isn't already connected
    auto allConns = getAllConnections();
    for (auto& existing : allConns)
        if (existing.destNode == conn.destNode && existing.destPort == conn.destPort)
            return false;

    juce::ValueTree c (IDs::CONNECTION);
    c.setProperty (IDs::sourceNode, conn.sourceNode, nullptr);
    c.setProperty (IDs::sourcePort, conn.sourcePort, nullptr);
    c.setProperty (IDs::destNode, conn.destNode, nullptr);
    c.setProperty (IDs::destPort, conn.destPort, nullptr);

    graphTree_.getChildWithName (IDs::CONNECTIONS).addChild (c, -1, &undoManager_);
    return true;
}

void GraphModel::removeConnection (const Connection& conn)
{
    auto connsTree = graphTree_.getChildWithName (IDs::CONNECTIONS);
    for (int i = connsTree.getNumChildren() - 1; i >= 0; --i)
    {
        auto c = connsTree.getChild (i);
        if (c[IDs::sourceNode] == conn.sourceNode
            && static_cast<int> (c[IDs::sourcePort]) == conn.sourcePort
            && c[IDs::destNode] == conn.destNode
            && static_cast<int> (c[IDs::destPort]) == conn.destPort)
        {
            connsTree.removeChild (i, &undoManager_);
            return;
        }
    }
}

void GraphModel::setNodeParam (const juce::String& nodeId, const juce::String& paramName, const juce::var& value)
{
    auto params = getParamsTree (nodeId);
    if (params.isValid())
        params.setProperty (juce::Identifier (paramName), value, &undoManager_);
}

void GraphModel::setNodePosition (const juce::String& nodeId, float x, float y)
{
    auto node = getNodeTree (nodeId);
    if (node.isValid())
    {
        node.setProperty (IDs::x, x, &undoManager_);
        node.setProperty (IDs::y, y, &undoManager_);
    }
}

juce::ValueTree GraphModel::getNodeTree (const juce::String& nodeId) const
{
    auto nodesTree = graphTree_.getChildWithName (IDs::NODES);
    for (int i = 0; i < nodesTree.getNumChildren(); ++i)
    {
        auto node = nodesTree.getChild (i);
        if (node[IDs::id] == nodeId)
            return node;
    }
    return {};
}

juce::ValueTree GraphModel::getParamsTree (const juce::String& nodeId) const
{
    auto node = getNodeTree (nodeId);
    return node.isValid() ? node.getChildWithName (IDs::PARAMS) : juce::ValueTree {};
}

std::vector<juce::String> GraphModel::getAllNodeIds() const
{
    std::vector<juce::String> ids;
    auto nodesTree = graphTree_.getChildWithName (IDs::NODES);
    for (int i = 0; i < nodesTree.getNumChildren(); ++i)
        ids.push_back (nodesTree.getChild (i)[IDs::id].toString());
    return ids;
}

std::vector<Connection> GraphModel::getAllConnections() const
{
    std::vector<Connection> conns;
    auto connsTree = graphTree_.getChildWithName (IDs::CONNECTIONS);
    for (int i = 0; i < connsTree.getNumChildren(); ++i)
    {
        auto c = connsTree.getChild (i);
        conns.push_back ({
            c[IDs::sourceNode].toString(),
            static_cast<int> (c[IDs::sourcePort]),
            c[IDs::destNode].toString(),
            static_cast<int> (c[IDs::destPort])
        });
    }
    return conns;
}

juce::String GraphModel::getNodeTypeId (const juce::String& nodeId) const
{
    auto node = getNodeTree (nodeId);
    return node.isValid() ? node[IDs::typeId].toString() : juce::String {};
}

//==============================================================================
juce::String GraphModel::toJSON() const
{
    auto xml = graphTree_.toXmlString();
    // Use ValueTree's own XML serialization, then wrap as JSON
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    // Serialize nodes
    juce::Array<juce::var> nodesArray;
    auto nodesTree = graphTree_.getChildWithName (IDs::NODES);
    for (int i = 0; i < nodesTree.getNumChildren(); ++i)
    {
        auto nodeTree = nodesTree.getChild (i);
        juce::DynamicObject::Ptr nodeObj = new juce::DynamicObject();
        nodeObj->setProperty ("id",     nodeTree[IDs::id]);
        nodeObj->setProperty ("typeId", nodeTree[IDs::typeId]);
        nodeObj->setProperty ("x",      nodeTree[IDs::x]);
        nodeObj->setProperty ("y",      nodeTree[IDs::y]);

        auto paramsTree = nodeTree.getChildWithName (IDs::PARAMS);
        juce::DynamicObject::Ptr paramsObj = new juce::DynamicObject();
        for (int j = 0; j < paramsTree.getNumProperties(); ++j)
        {
            auto name = paramsTree.getPropertyName (j);
            paramsObj->setProperty (name, paramsTree[name]);
        }
        nodeObj->setProperty ("params", juce::var (paramsObj.get()));
        nodesArray.add (juce::var (nodeObj.get()));
    }
    root->setProperty ("nodes", nodesArray);

    // Serialize connections
    juce::Array<juce::var> connsArray;
    auto connsTree = graphTree_.getChildWithName (IDs::CONNECTIONS);
    for (int i = 0; i < connsTree.getNumChildren(); ++i)
    {
        auto c = connsTree.getChild (i);
        juce::DynamicObject::Ptr connObj = new juce::DynamicObject();
        connObj->setProperty ("sourceNode", c[IDs::sourceNode]);
        connObj->setProperty ("sourcePort", c[IDs::sourcePort]);
        connObj->setProperty ("destNode",   c[IDs::destNode]);
        connObj->setProperty ("destPort",   c[IDs::destPort]);
        connsArray.add (juce::var (connObj.get()));
    }
    root->setProperty ("connections", connsArray);

    return juce::JSON::toString (juce::var (root.get()));
}

bool GraphModel::loadFromJSON (const juce::String& json)
{
    auto parsed = juce::JSON::parse (json);
    if (! parsed.isObject()) return false;

    clear();

    auto* root = parsed.getDynamicObject();
    if (! root) return false;

    // Load nodes
    auto nodesVar = root->getProperty ("nodes");
    if (auto* nodesArr = nodesVar.getArray())
    {
        for (auto& nodeVar : *nodesArr)
        {
            auto* nodeObj = nodeVar.getDynamicObject();
            if (! nodeObj) continue;

            juce::String nodeTypeId = nodeObj->getProperty ("typeId").toString();
            float x = static_cast<float> (nodeObj->getProperty ("x"));
            float y = static_cast<float> (nodeObj->getProperty ("y"));
            juce::String nodeId = nodeObj->getProperty ("id").toString();

            // Manually add to avoid re-generating ID
            juce::ValueTree node (IDs::NODE);
            node.setProperty (IDs::id, nodeId, nullptr);
            node.setProperty (IDs::typeId, nodeTypeId, nullptr);
            node.setProperty (IDs::x, x, nullptr);
            node.setProperty (IDs::y, y, nullptr);
            node.addChild (juce::ValueTree (IDs::PARAMS), -1, nullptr);

            // Load params
            auto paramsVar = nodeObj->getProperty ("params");
            if (auto* paramsObj = paramsVar.getDynamicObject())
            {
                auto paramsTree = node.getChildWithName (IDs::PARAMS);
                for (auto& prop : paramsObj->getProperties())
                    paramsTree.setProperty (prop.name, prop.value, nullptr);
            }

            graphTree_.getChildWithName (IDs::NODES).addChild (node, -1, nullptr);

            // Update nextNodeId_ to avoid collisions
            if (nodeId.startsWith ("node_"))
            {
                int num = nodeId.fromFirstOccurrenceOf ("node_", false, false).getIntValue();
                nextNodeId_ = juce::jmax (nextNodeId_, num + 1);
            }
        }
    }

    // Load connections
    auto connsVar = root->getProperty ("connections");
    if (auto* connsArr = connsVar.getArray())
    {
        for (auto& connVar : *connsArr)
        {
            auto* connObj = connVar.getDynamicObject();
            if (! connObj) continue;

            juce::ValueTree c (IDs::CONNECTION);
            c.setProperty (IDs::sourceNode, connObj->getProperty ("sourceNode"), nullptr);
            c.setProperty (IDs::sourcePort, connObj->getProperty ("sourcePort"), nullptr);
            c.setProperty (IDs::destNode,   connObj->getProperty ("destNode"), nullptr);
            c.setProperty (IDs::destPort,   connObj->getProperty ("destPort"), nullptr);

            graphTree_.getChildWithName (IDs::CONNECTIONS).addChild (c, -1, nullptr);
        }
    }

    notifyListeners();
    return true;
}

void GraphModel::clear()
{
    graphTree_.removeListener (this);
    graphTree_ = juce::ValueTree (IDs::GRAPH);
    graphTree_.addChild (juce::ValueTree (IDs::NODES), -1, nullptr);
    graphTree_.addChild (juce::ValueTree (IDs::CONNECTIONS), -1, nullptr);
    graphTree_.addListener (this);
    nextNodeId_ = 1;
    undoManager_.clearUndoHistory();
    notifyListeners();
}

//==============================================================================
void GraphModel::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) { notifyListeners(); }
void GraphModel::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) { notifyListeners(); }
void GraphModel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) { notifyListeners(); }

void GraphModel::notifyListeners()
{
    listeners_.call (&Listener::graphChanged);
}

} // namespace pf
