#include "UI/NodeComponent.h"
#include "UI/NodeEditorComponent.h"
#include "UI/Theme.h"

namespace pf
{

NodeComponent::NodeComponent (NodeEditorComponent& editor, const juce::String& nodeId,
                              const juce::String& typeId, const juce::String& displayName,
                              const juce::String& category,
                              const std::vector<Port>& inputs, const std::vector<Port>& outputs)
    : editor_ (editor), nodeId_ (nodeId), typeId_ (typeId), displayName_ (displayName),
      category_ (category)
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
    const float cr = static_cast<float> (Theme::kNodeCornerRadius);

    // Drop shadow
    juce::DropShadow shadow (juce::Colour (0x55000000), 10, { 0, 3 });
    shadow.drawForRectangle (g, getLocalBounds());

    // Body
    auto bodyColour = juce::Colour (Theme::kBgSurface);
    g.setColour (bodyColour);
    g.fillRoundedRectangle (bounds, cr);

    // Header with gradient
    auto headerBounds = bounds.removeFromTop (kHeaderHeight);
    auto headerColour = getHeaderColour();
    {
        juce::Path headerPath;
        headerPath.addRoundedRectangle (headerBounds.getX(), headerBounds.getY(),
                                         headerBounds.getWidth(), headerBounds.getHeight(),
                                         cr, cr, true, true, false, false);

        juce::ColourGradient grad (headerColour, headerBounds.getX(), headerBounds.getY(),
                                    headerColour.darker (0.3f), headerBounds.getX(), headerBounds.getBottom(),
                                    false);
        g.setGradientFill (grad);
        g.fillPath (headerPath);
    }

    // Header bottom separator line
    g.setColour (juce::Colour (0x20000000));
    g.drawHorizontalLine (static_cast<int> (headerBounds.getBottom()), bounds.getX(), bounds.getX() + bounds.getWidth());

    // Title
    g.setColour (juce::Colour (Theme::kTextPrimary));
    g.setFont (juce::Font (Theme::kFontNodeHeader));
    g.drawText (displayName_, headerBounds.reduced (8, 0), juce::Justification::centredLeft);

    // Port labels
    g.setFont (juce::Font (Theme::kFontSmall));
    g.setColour (juce::Colour (Theme::kTextSecondary));
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

    // Selection outline with glow
    if (selected_)
    {
        auto focusColour = juce::Colour (Theme::kBorderFocus);
        g.setColour (focusColour.withAlpha (0.15f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().expanded (2.f), cr + 2.f, 4.f);
        g.setColour (focusColour);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), cr, 2.f);
    }
    else
    {
        g.setColour (juce::Colour (Theme::kBorderNormal).withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), cr, 1.f);
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
    if (dynamic_cast<PortComponent*> (e.originalComponent) != nullptr)
        return;

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
    return Theme::headerColourForCategory (category_);
}

} // namespace pf
