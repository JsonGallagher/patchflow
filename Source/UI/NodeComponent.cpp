#include "UI/NodeComponent.h"
#include "UI/NodeEditorComponent.h"

namespace pf
{

NodeComponent::NodeComponent (NodeEditorComponent& editor, const juce::String& nodeId,
                              const juce::String& typeId, const juce::String& displayName,
                              const std::vector<Port>& inputs, const std::vector<Port>& outputs)
    : editor_ (editor), nodeId_ (nodeId), typeId_ (typeId), displayName_ (displayName)
{
    for (auto& port : inputs)
    {
        auto pc = std::make_unique<PortComponent> (*this, port.name, port.type,
                                                    PortDirection::Input, port.index);
        addAndMakeVisible (*pc);
        inputPorts_.push_back (std::move (pc));
    }

    for (auto& port : outputs)
    {
        auto pc = std::make_unique<PortComponent> (*this, port.name, port.type,
                                                    PortDirection::Output, port.index);
        addAndMakeVisible (*pc);
        outputPorts_.push_back (std::move (pc));
    }

    int numRows = juce::jmax (static_cast<int> (inputs.size()),
                               static_cast<int> (outputs.size()));
    int height = kHeaderHeight + numRows * kPortRowHeight + kPortPadding;
    setSize (kMinWidth, juce::jmax (height, kHeaderHeight + kPortRowHeight));
}

void NodeComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Body
    g.setColour (juce::Colour (0xff2a2a3a));
    g.fillRoundedRectangle (bounds, 6.f);

    // Header
    auto headerBounds = bounds.removeFromTop (kHeaderHeight);
    g.setColour (getHeaderColour());
    g.fillRoundedRectangle (headerBounds.withBottom (headerBounds.getBottom() + 3), 6.f);
    g.setColour (juce::Colour (0xff2a2a3a)); // patch the bottom corners
    g.fillRect (headerBounds.getX(), headerBounds.getBottom() - 3, headerBounds.getWidth(), 3.f);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (13.f));
    g.drawText (displayName_, headerBounds.reduced (8, 0), juce::Justification::centredLeft);

    // Port labels
    g.setFont (juce::Font (10.f));
    g.setColour (juce::Colour (0xffcccccc));
    for (auto& port : inputPorts_)
    {
        auto labelBounds = juce::Rectangle<float> (
            PortComponent::kDiameter + 4.f,
            static_cast<float> (port->getY()),
            static_cast<float> (getWidth() / 2 - PortComponent::kDiameter - 8),
            static_cast<float> (PortComponent::kDiameter));
        g.drawText (port->getPortName(), labelBounds, juce::Justification::centredLeft);
    }
    for (auto& port : outputPorts_)
    {
        auto labelBounds = juce::Rectangle<float> (
            static_cast<float> (getWidth() / 2),
            static_cast<float> (port->getY()),
            static_cast<float> (getWidth() / 2 - PortComponent::kDiameter - 4),
            static_cast<float> (PortComponent::kDiameter));
        g.drawText (port->getPortName(), labelBounds, juce::Justification::centredRight);
    }

    // Selection outline
    if (selected_)
    {
        g.setColour (juce::Colour (0xff64b5f6));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.f, 2.f);
    }
    else
    {
        g.setColour (juce::Colour (0xff555566));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.f, 1.f);
    }
}

void NodeComponent::resized()
{
    int y = kHeaderHeight + kPortPadding;

    for (auto& port : inputPorts_)
    {
        port->setBounds (-PortComponent::kDiameter / 2, y, PortComponent::kDiameter, PortComponent::kDiameter);
        y += kPortRowHeight;
    }

    y = kHeaderHeight + kPortPadding;
    for (auto& port : outputPorts_)
    {
        port->setBounds (getWidth() - PortComponent::kDiameter / 2, y, PortComponent::kDiameter, PortComponent::kDiameter);
        y += kPortRowHeight;
    }
}

void NodeComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        editor_.showNodeContextMenu (*this);
        return;
    }

    editor_.nodeSelected (nodeId_, e.mods.isShiftDown());
    dragStart_ = getPosition();
    isDragging_ = true;
    toFront (true);
}

void NodeComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDragging_) return;

    auto delta = e.getEventRelativeTo (&editor_).getPosition()
               - e.getMouseDownPosition() - dragStart_;
    auto newPos = dragStart_ + delta + e.getMouseDownPosition();
    setTopLeftPosition (newPos);
    editor_.nodeMoved (nodeId_);
}

void NodeComponent::mouseUp (const juce::MouseEvent& /*e*/)
{
    if (isDragging_)
    {
        isDragging_ = false;
        editor_.nodeFinishedDragging (nodeId_, getPosition().toFloat());
    }
}

PortComponent* NodeComponent::getInputPort (int index) const
{
    return (index < static_cast<int> (inputPorts_.size())) ? inputPorts_[index].get() : nullptr;
}

PortComponent* NodeComponent::getOutputPort (int index) const
{
    return (index < static_cast<int> (outputPorts_.size())) ? outputPorts_[index].get() : nullptr;
}

juce::Colour NodeComponent::getHeaderColour() const
{
    if (typeId_.containsIgnoreCase ("Audio") || typeId_ == "Gain"
        || typeId_ == "FFTAnalyzer" || typeId_ == "EnvelopeFollower"
        || typeId_ == "BandSplitter" || typeId_ == "Smoothing")
        return juce::Colour (0xff2196f3);
    if (typeId_ == "Add" || typeId_ == "Multiply" || typeId_ == "MapRange" || typeId_ == "Clamp")
        return juce::Colour (0xff4caf50);
    if (typeId_.containsIgnoreCase ("Renderer") || typeId_.containsIgnoreCase ("Shader")
        || typeId_.containsIgnoreCase ("Canvas") || typeId_.containsIgnoreCase ("Color"))
        return juce::Colour (0xff9c27b0);
    return juce::Colour (0xff607d8b);
}

} // namespace pf
