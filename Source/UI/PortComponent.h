#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/PortTypes.h"

namespace pf
{

class NodeComponent;

class PortComponent : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PortComponent (NodeComponent& owner, const juce::String& name,
                   PortType type, PortDirection direction, int index);

    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    juce::Point<float> getCentreInParentEditor() const;

    NodeComponent& getOwner() const { return owner_; }
    PortType getType() const { return type_; }
    PortDirection getDirection() const { return direction_; }
    int getPortIndex() const { return index_; }
    const juce::String& getPortName() const { return name_; }

    // Highlight state set by NodeEditorComponent during cable drag
    enum class Highlight { None, Compatible, Incompatible };
    void setHighlight (Highlight h) { highlight_ = h; repaint(); }
    Highlight getHighlight() const { return highlight_; }

    static constexpr int kDiameter = 12;

private:
    NodeComponent& owner_;
    juce::String name_;
    PortType type_;
    PortDirection direction_;
    int index_;
    bool hovered_ = false;
    Highlight highlight_ = Highlight::None;
};

} // namespace pf
