#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include "Graph/PortTypes.h"
#include "Graph/Connection.h"

namespace pf
{

/** Identifiers for ValueTree types and properties. */
namespace IDs
{
    static const juce::Identifier GRAPH       { "Graph" };
    static const juce::Identifier NODES       { "Nodes" };
    static const juce::Identifier NODE        { "Node" };
    static const juce::Identifier CONNECTIONS { "Connections" };
    static const juce::Identifier CONNECTION  { "Connection" };
    static const juce::Identifier PARAMS      { "Params" };

    static const juce::Identifier id          { "id" };
    static const juce::Identifier typeId      { "typeId" };
    static const juce::Identifier x           { "x" };
    static const juce::Identifier y           { "y" };
    static const juce::Identifier sourceNode  { "sourceNode" };
    static const juce::Identifier sourcePort  { "sourcePort" };
    static const juce::Identifier destNode    { "destNode" };
    static const juce::Identifier destPort    { "destPort" };
}

/**
 * Editable data model for the node graph, backed by ValueTree.
 * Source of truth for the patch — all mutations go through UndoManager.
 */
class GraphModel : public juce::ValueTree::Listener
{
public:
    GraphModel();

    //==============================================================================
    // Graph mutations (all undoable)
    juce::String addNode (const juce::String& typeId, float x, float y);
    void removeNode (const juce::String& nodeId);
    bool addConnection (const Connection& conn);
    void removeConnection (const Connection& conn);
    void setNodeParam (const juce::String& nodeId, const juce::String& paramName, const juce::var& value);
    void setNodePosition (const juce::String& nodeId, float x, float y);

    //==============================================================================
    // Accessors
    juce::ValueTree getNodeTree (const juce::String& nodeId) const;
    juce::ValueTree getParamsTree (const juce::String& nodeId) const;
    std::vector<juce::String> getAllNodeIds() const;
    std::vector<Connection> getAllConnections() const;
    juce::String getNodeTypeId (const juce::String& nodeId) const;

    //==============================================================================
    // Serialization
    juce::String toJSON() const;
    bool loadFromJSON (const juce::String& json);
    void clear();

    //==============================================================================
    // Undo
    juce::UndoManager& getUndoManager() { return undoManager_; }

    //==============================================================================
    // Listener support
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void graphChanged() = 0;
    };

    void addListener (Listener* l)    { listeners_.add (l); }
    void removeListener (Listener* l) { listeners_.remove (l); }

    const juce::ValueTree& getValueTree() const { return graphTree_; }

private:
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void notifyListeners();

    juce::ValueTree graphTree_;
    juce::UndoManager undoManager_;
    juce::ListenerList<Listener> listeners_;
    int nextNodeId_ = 1;
};

} // namespace pf
