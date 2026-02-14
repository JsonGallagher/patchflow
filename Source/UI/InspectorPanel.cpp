#include "UI/InspectorPanel.h"
#include <cmath>

namespace pf
{

namespace
{
bool isStringParam (const NodeParam& param)
{
    return param.defaultValue.isString();
}

bool isDiscreteParam (const NodeParam& param)
{
    if (param.defaultValue.isVoid())
        return false;

    return param.defaultValue.isInt()
        || param.defaultValue.isInt64()
        || param.defaultValue.isBool();
}
} // namespace

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
        const int rowHeight = ctrl.isText ? 190 : 50;
        auto row = bounds.removeFromTop (rowHeight);
        ctrl.label->setBounds (row.removeFromTop (18));

        if (ctrl.slider)
            ctrl.slider->setBounds (row.reduced (0, 2));

        if (ctrl.textEditor)
            ctrl.textEditor->setBounds (row.reduced (0, 2));
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

        // Read current value from model
        auto currentVal = model_.getValueTree().getChildWithName (IDs::NODES)
                            .getChildWithProperty (IDs::id, currentNodeId_)
                            .getChildWithName (IDs::PARAMS)
                            .getProperty (juce::Identifier (param.name));

        if (isStringParam (param))
        {
            ctrl.isText = true;
            ctrl.textEditor = std::make_unique<juce::TextEditor>();
            ctrl.textEditor->setMultiLine (true, true);
            ctrl.textEditor->setReturnKeyStartsNewLine (true);
            ctrl.textEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151524));
            ctrl.textEditor->setColour (juce::TextEditor::textColourId, juce::Colours::white);
            ctrl.textEditor->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff34344a));
            ctrl.textEditor->setFont (juce::Font (12.0f));

            auto textValue = currentVal.isVoid() ? param.defaultValue.toString() : currentVal.toString();
            ctrl.textEditor->setText (textValue, juce::dontSendNotification);

            auto nodeId = currentNodeId_;
            auto paramName = param.name;
            auto* editor = ctrl.textEditor.get();
            ctrl.textEditor->onFocusLost = [this, nodeId, paramName, editor]()
            {
                if (editor != nullptr)
                    model_.setNodeParam (nodeId, paramName, editor->getText());
            };

            addAndMakeVisible (*ctrl.textEditor);
        }
        else
        {
            ctrl.isDiscrete = isDiscreteParam (param);
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

            const auto interval = ctrl.isDiscrete ? 1.0 : 0.001;
            ctrl.slider->setRange (minVal, maxVal, interval);
            ctrl.slider->setNumDecimalPlacesToDisplay (ctrl.isDiscrete ? 0 : 3);
            ctrl.slider->setValue (defVal, juce::dontSendNotification);

            if (! currentVal.isVoid())
            {
                const auto value = ctrl.isDiscrete
                                 ? static_cast<double> (juce::roundToInt (static_cast<double> (currentVal)))
                                 : static_cast<double> (currentVal);
                ctrl.slider->setValue (value, juce::dontSendNotification);
            }

            auto nodeId = currentNodeId_;
            auto paramName = param.name;
            auto discrete = ctrl.isDiscrete;
            ctrl.slider->onValueChange = [this, nodeId, paramName, slider = ctrl.slider.get(), discrete]()
            {
                if (slider == nullptr)
                    return;

                if (discrete)
                {
                    const auto snappedValue = juce::roundToInt (slider->getValue());
                    if (std::abs (slider->getValue() - static_cast<double> (snappedValue)) > 1.0e-6)
                        slider->setValue (static_cast<double> (snappedValue), juce::dontSendNotification);

                    model_.setNodeParam (nodeId, paramName, snappedValue);
                }
                else
                {
                    model_.setNodeParam (nodeId, paramName, slider->getValue());
                }
            };

            addAndMakeVisible (*ctrl.slider);
        }

        controls_.push_back (std::move (ctrl));
    }

    resized();
}

} // namespace pf
