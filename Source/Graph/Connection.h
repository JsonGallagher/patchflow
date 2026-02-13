#pragma once
#include <juce_core/juce_core.h>

namespace pf
{

using NodeId = juce::String;

struct Connection
{
    NodeId sourceNode;
    int    sourcePort = 0;
    NodeId destNode;
    int    destPort = 0;

    bool operator== (const Connection& other) const
    {
        return sourceNode == other.sourceNode
            && sourcePort == other.sourcePort
            && destNode   == other.destNode
            && destPort   == other.destPort;
    }

    bool operator!= (const Connection& other) const { return ! (*this == other); }
};

} // namespace pf
