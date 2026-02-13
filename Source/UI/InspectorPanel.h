#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/GraphModel.h"
#include "Nodes/NodeBase.h"

namespace pf
{

/**
 * Panel for editing parameters of the selected node.
 * Shows sliders, text fields, etc. based on NodeParam definitions.
 */
class InspectorPanel : public juce::Component
{
public:
    explicit InspectorPanel (GraphModel& model);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSelectedNode (const juce::String& nodeId, const std::vector<NodeParam>& params);
    void clearSelection();

private:
    GraphModel& model_;
    juce::String currentNodeId_;

    juce::Label titleLabel_;

    struct ParamControl
    {
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::Slider> slider;
        juce::String paramName;
    };
    std::vector<ParamControl> controls_;

    void rebuildControls (const std::vector<NodeParam>& params);
};

} // namespace pf
