#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/GraphModel.h"
#include "Nodes/NodeBase.h"

namespace pf
{

/**
 * Panel for editing parameters of the selected node.
 * Shows sliders, combo boxes, text fields, grouped with headers.
 */
class InspectorPanel : public juce::Component
{
public:
    explicit InspectorPanel (GraphModel& model);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSelectedNode (const juce::String& nodeId, const std::vector<NodeParam>& params,
                          const juce::String& displayName = {});
    void clearSelection();

private:
    GraphModel& model_;
    juce::String currentNodeId_;

    juce::Label titleLabel_;

    // Viewport for scrolling when many params
    juce::Viewport viewport_;
    std::unique_ptr<juce::Component> contentComponent_;

    struct ParamControl
    {
        std::unique_ptr<juce::Label>    label;
        std::unique_ptr<juce::Slider>   slider;
        std::unique_ptr<juce::TextEditor> textEditor;
        std::unique_ptr<juce::ComboBox> comboBox;
        bool isDiscrete = false;
        bool isText     = false;
        bool isEnum     = false;
        juce::String paramName;
        juce::String group;
    };

    struct GroupHeader
    {
        std::unique_ptr<juce::Label> label;
    };

    // Interleaved layout items
    struct LayoutItem
    {
        enum Type { Param, Group };
        Type type;
        int index; // index into controls_ or groupHeaders_
    };

    std::vector<ParamControl> controls_;
    std::vector<GroupHeader>  groupHeaders_;
    std::vector<LayoutItem>   layoutItems_;

    void rebuildControls (const std::vector<NodeParam>& params);
};

} // namespace pf
