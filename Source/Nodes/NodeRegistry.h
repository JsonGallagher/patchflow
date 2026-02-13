#pragma once
#include "Nodes/NodeBase.h"
#include <functional>
#include <memory>
#include <unordered_map>

namespace pf
{

class NodeRegistry
{
public:
    using FactoryFn = std::function<std::unique_ptr<NodeBase>()>;

    struct NodeInfo
    {
        juce::String typeId;
        juce::String displayName;
        juce::String category;
        FactoryFn    factory;
    };

    static NodeRegistry& instance()
    {
        static NodeRegistry reg;
        return reg;
    }

    template <typename T>
    void registerNode()
    {
        auto proto = std::make_unique<T>();
        NodeInfo info;
        info.typeId      = proto->getTypeId();
        info.displayName = proto->getDisplayName();
        info.category    = proto->getCategory();
        info.factory     = [] { return std::make_unique<T>(); };
        registry_[info.typeId.toStdString()] = std::move (info);
    }

    std::unique_ptr<NodeBase> createNode (const juce::String& typeId) const
    {
        auto it = registry_.find (typeId.toStdString());
        if (it != registry_.end())
            return it->second.factory();
        return nullptr;
    }

    std::vector<NodeInfo> getAllNodeTypes() const
    {
        std::vector<NodeInfo> result;
        result.reserve (registry_.size());
        for (auto& [_, info] : registry_)
            result.push_back (info);
        return result;
    }

    std::vector<NodeInfo> getNodeTypesInCategory (const juce::String& category) const
    {
        std::vector<NodeInfo> result;
        for (auto& [_, info] : registry_)
            if (info.category == category)
                result.push_back (info);
        return result;
    }

private:
    NodeRegistry() = default;
    std::unordered_map<std::string, NodeInfo> registry_;
};

} // namespace pf
