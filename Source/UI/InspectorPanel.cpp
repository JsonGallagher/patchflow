#include "UI/InspectorPanel.h"

namespace pf
{

InspectorPanel::InspectorPanel (GraphModel& model) : model_ (model)
{
    titleLabel_.setFont (juce::Font (16.f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel_.setText ("Inspector", juce::dontSendNotification);
    addAndMakeVisible (titleLabel_);
}

void InspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
    g.setColour (juce::Colour (0xff333344));
    g.drawLine (0.f, 0.f, 0.f, static_cast<float> (getHeight()), 1.f);
}

void InspectorPanel::resized()
{
    auto bounds = getLocalBounds().reduced (8);
    titleLabel_.setBounds (bounds.removeFromTop (30));
    bounds.removeFromTop (4);

    for (auto& ctrl : controls_)
    {
        auto row = bounds.removeFromTop (50);
        ctrl.label->setBounds (row.removeFromTop (18));
        ctrl.slider->setBounds (row.reduced (0, 2));
    }
}

void InspectorPanel::setSelectedNode (const juce::String& nodeId,
                                       const std::vector<NodeParam>& params)
{
    currentNodeId_ = nodeId;
    auto typeId = model_.getNodeTypeId (nodeId);
    titleLabel_.setText (typeId + " (" + nodeId + ")", juce::dontSendNotification);
    rebuildControls (params);
}

void InspectorPanel::clearSelection()
{
    currentNodeId_ = {};
    titleLabel_.setText ("Inspector", juce::dontSendNotification);
    controls_.clear();
}

void InspectorPanel::rebuildControls (const std::vector<NodeParam>& params)
{
    controls_.clear();

    for (auto& param : params)
    {
        ParamControl ctrl;
        ctrl.paramName = param.name;

        ctrl.label = std::make_unique<juce::Label>();
        ctrl.label->setText (param.name, juce::dontSendNotification);
        ctrl.label->setColour (juce::Label::textColourId, juce::Colour (0xffaaaacc));
        ctrl.label->setFont (juce::Font (12.f));
        addAndMakeVisible (*ctrl.label);

        ctrl.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                       juce::Slider::TextBoxRight);
        ctrl.slider->setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a2a3a));
        ctrl.slider->setColour (juce::Slider::trackColourId, juce::Colour (0xff4fc3f7));
        ctrl.slider->setColour (juce::Slider::thumbColourId, juce::Colours::white);
        ctrl.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        ctrl.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a1a2a));

        double minVal = param.minValue.isVoid() ? 0.0 : static_cast<double> (param.minValue);
        double maxVal = param.maxValue.isVoid() ? 1.0 : static_cast<double> (param.maxValue);
        double defVal = param.defaultValue.isVoid() ? 0.0 : static_cast<double> (param.defaultValue);

        ctrl.slider->setRange (minVal, maxVal, 0.001);
        ctrl.slider->setValue (defVal, juce::dontSendNotification);

        // Read current value from model
        auto currentVal = model_.getValueTree().getChildWithName (IDs::NODES)
                            .getChildWithProperty (IDs::id, currentNodeId_)
                            .getChildWithName (IDs::PARAMS)
                            .getProperty (juce::Identifier (param.name));
        if (! currentVal.isVoid())
            ctrl.slider->setValue (static_cast<double> (currentVal), juce::dontSendNotification);

        auto nodeId = currentNodeId_;
        auto paramName = param.name;
        ctrl.slider->onValueChange = [this, nodeId, paramName, slider = ctrl.slider.get()]()
        {
            model_.setNodeParam (nodeId, paramName, slider->getValue());
        };

        addAndMakeVisible (*ctrl.slider);
        controls_.push_back (std::move (ctrl));
    }

    resized();
}

} // namespace pf
