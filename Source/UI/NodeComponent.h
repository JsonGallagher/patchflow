#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Nodes/NodeBase.h"
#include "UI/PortComponent.h"
#include <vector>
#include <memory>

namespace pf
{

class NodeEditorComponent;

class NodeComponent : public juce::Component
{
public:
    NodeComponent (NodeEditorComponent& editor, const juce::String& nodeId,
                   const juce::String& typeId, const juce::String& displayName,
                   const juce::String& category,
                   const std::vector<Port>& inputs, const std::vector<Port>& outputs);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    const juce::String& getNodeId() const { return nodeId_; }
    const juce::String& getTypeId() const { return typeId_; }
    bool isSelected() const { return selected_; }
    void setSelected (bool s) { selected_ = s; repaint(); }

    PortComponent* getInputPort (int index) const;
    PortComponent* getOutputPort (int index) const;

    int getNumInputPorts() const  { return static_cast<int> (inputPorts_.size()); }
    int getNumOutputPorts() const { return static_cast<int> (outputPorts_.size()); }

    static constexpr int kHeaderHeight = 26;
    static constexpr int kPortRowHeight = 20;
    static constexpr int kMinWidth = 150;
    static constexpr int kPortPadding = 6;

private:
    NodeEditorComponent& editor_;
    juce::String nodeId_;
    juce::String typeId_;
    juce::String displayName_;
    juce::String category_;
    bool selected_ = false;

    std::vector<std::unique_ptr<PortComponent>> inputPorts_;
    std::vector<std::unique_ptr<PortComponent>> outputPorts_;

    juce::ComponentDragger dragger_;
    juce::Point<int> dragStart_;
    bool isDragging_ = false;

    juce::Colour getHeaderColour() const;
};

} // namespace pf
