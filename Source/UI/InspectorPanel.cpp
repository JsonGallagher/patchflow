#include "UI/InspectorPanel.h"
#include "UI/Theme.h"
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
    titleLabel_.setFont (juce::Font (Theme::kFontTitle, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (Theme::kTextPrimary));
    titleLabel_.setText ("Inspector", juce::dontSendNotification);
    addAndMakeVisible (titleLabel_);

    viewport_.setScrollBarsShown (true, false);
    viewport_.setScrollBarThickness (6);
    addAndMakeVisible (viewport_);
}

void InspectorPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (Theme::kBgSecondary));
    g.setColour (juce::Colour (Theme::kBorderSubtle));
    g.drawLine (0.f, 0.f, 0.f, static_cast<float> (getHeight()), 1.f);
}

void InspectorPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Theme::kInspectorPadding);
    titleLabel_.setBounds (bounds.removeFromTop (Theme::kTitleHeight));
    bounds.removeFromTop (4);
    viewport_.setBounds (bounds);

    if (contentComponent_ == nullptr)
        return;

    // Calculate total content height
    int totalHeight = 0;
    for (auto& item : layoutItems_)
    {
        if (item.type == LayoutItem::Group)
            totalHeight += Theme::kGroupHeaderHeight;
        else
        {
            auto& ctrl = controls_[static_cast<size_t> (item.index)];
            totalHeight += ctrl.isText ? Theme::kTextParamHeight : Theme::kParamRowHeight;
        }
    }

    contentComponent_->setSize (viewport_.getWidth() - (viewport_.isVerticalScrollBarShown() ? 6 : 0),
                                 juce::jmax (totalHeight, viewport_.getHeight()));

    int y = 0;
    int contentWidth = contentComponent_->getWidth();

    for (auto& item : layoutItems_)
    {
        if (item.type == LayoutItem::Group)
        {
            auto& gh = groupHeaders_[static_cast<size_t> (item.index)];
            gh.label->setBounds (0, y + 6, contentWidth, Theme::kGroupHeaderHeight - 6);
            y += Theme::kGroupHeaderHeight;
        }
        else
        {
            auto& ctrl = controls_[static_cast<size_t> (item.index)];
            const int rowHeight = ctrl.isText ? Theme::kTextParamHeight : Theme::kParamRowHeight;
            auto row = juce::Rectangle<int> (0, y, contentWidth, rowHeight);
            ctrl.label->setBounds (row.removeFromTop (18));

            if (ctrl.slider)
                ctrl.slider->setBounds (row.reduced (0, 2));
            if (ctrl.textEditor)
                ctrl.textEditor->setBounds (row.reduced (0, 2));
            if (ctrl.comboBox)
                ctrl.comboBox->setBounds (row.removeFromTop (24).reduced (0, 2));

            y += rowHeight;
        }
    }
}

void InspectorPanel::setSelectedNode (const juce::String& nodeId,
                                       const std::vector<NodeParam>& params,
                                       const juce::String& displayName)
{
    currentNodeId_ = nodeId;

    // Show clean display name instead of "TypeId (node_7)"
    if (displayName.isNotEmpty())
        titleLabel_.setText (displayName, juce::dontSendNotification);
    else
        titleLabel_.setText (model_.getNodeTypeId (nodeId), juce::dontSendNotification);

    rebuildControls (params);
}

void InspectorPanel::clearSelection()
{
    currentNodeId_ = {};
    titleLabel_.setText ("Inspector", juce::dontSendNotification);
    controls_.clear();
    groupHeaders_.clear();
    layoutItems_.clear();
    contentComponent_.reset();
    viewport_.setViewedComponent (nullptr, false);
}

void InspectorPanel::rebuildControls (const std::vector<NodeParam>& params)
{
    controls_.clear();
    groupHeaders_.clear();
    layoutItems_.clear();

    contentComponent_ = std::make_unique<juce::Component>();
    juce::String currentGroup;

    for (auto& param : params)
    {
        // Insert group header when group changes
        if (param.group.isNotEmpty() && param.group != currentGroup)
        {
            currentGroup = param.group;
            GroupHeader gh;
            gh.label = std::make_unique<juce::Label>();
            gh.label->setText (param.group, juce::dontSendNotification);
            gh.label->setFont (juce::Font (Theme::kFontGroupHeader, juce::Font::bold));
            gh.label->setColour (juce::Label::textColourId, juce::Colour (Theme::kGroupLabel));
            contentComponent_->addAndMakeVisible (*gh.label);

            layoutItems_.push_back ({ LayoutItem::Group, static_cast<int> (groupHeaders_.size()) });
            groupHeaders_.push_back (std::move (gh));
        }
        else if (param.group.isEmpty() && currentGroup.isNotEmpty())
        {
            currentGroup = {};
        }

        ParamControl ctrl;
        ctrl.paramName = param.name;
        ctrl.group = param.group;

        // Use displayName if available, fallback to internal name
        auto labelText = param.displayName.isNotEmpty() ? param.displayName : param.name;
        ctrl.label = std::make_unique<juce::Label>();
        ctrl.label->setText (labelText, juce::dontSendNotification);
        ctrl.label->setColour (juce::Label::textColourId, juce::Colour (Theme::kTextMuted));
        ctrl.label->setFont (juce::Font (Theme::kFontLabel));

        // Set tooltip from description
        if (param.description.isNotEmpty())
        {
            ctrl.label->setTooltip (param.description);
        }

        contentComponent_->addAndMakeVisible (*ctrl.label);

        // Read current value from model
        auto currentVal = model_.getValueTree().getChildWithName (IDs::NODES)
                            .getChildWithProperty (IDs::id, currentNodeId_)
                            .getChildWithName (IDs::PARAMS)
                            .getProperty (juce::Identifier (param.name));

        if (isStringParam (param))
        {
            // Text editor for string params (e.g. shader code)
            ctrl.isText = true;
            ctrl.textEditor = std::make_unique<juce::TextEditor>();
            ctrl.textEditor->setMultiLine (true, true);
            ctrl.textEditor->setReturnKeyStartsNewLine (true);
            ctrl.textEditor->setColour (juce::TextEditor::backgroundColourId, juce::Colour (Theme::kBgInput));
            ctrl.textEditor->setColour (juce::TextEditor::textColourId, juce::Colour (Theme::kTextPrimary));
            ctrl.textEditor->setColour (juce::TextEditor::outlineColourId, juce::Colour (Theme::kBorderInput));
            ctrl.textEditor->setFont (juce::Font (Theme::kFontLabel));

            auto textValue = currentVal.isVoid() ? param.defaultValue.toString() : currentVal.toString();
            ctrl.textEditor->setText (textValue, juce::dontSendNotification);

            if (param.description.isNotEmpty())
                ctrl.textEditor->setTooltip (param.description);

            auto nodeId = currentNodeId_;
            auto paramName = param.name;
            auto* editor = ctrl.textEditor.get();
            ctrl.textEditor->onFocusLost = [this, nodeId, paramName, editor]()
            {
                if (editor != nullptr)
                    model_.setNodeParam (nodeId, paramName, editor->getText());
            };

            contentComponent_->addAndMakeVisible (*ctrl.textEditor);
        }
        else if (param.enumLabels.size() > 0 && isDiscreteParam (param))
        {
            // ComboBox for enum params
            ctrl.isEnum = true;
            ctrl.isDiscrete = true;
            ctrl.comboBox = std::make_unique<juce::ComboBox>();
            ctrl.comboBox->setColour (juce::ComboBox::backgroundColourId, juce::Colour (Theme::kBgSurface));
            ctrl.comboBox->setColour (juce::ComboBox::textColourId, juce::Colour (Theme::kTextPrimary));
            ctrl.comboBox->setColour (juce::ComboBox::outlineColourId, juce::Colour (Theme::kBorderSubtle));
            ctrl.comboBox->setColour (juce::ComboBox::arrowColourId, juce::Colour (Theme::kTextMuted));

            for (int i = 0; i < param.enumLabels.size(); ++i)
                ctrl.comboBox->addItem (param.enumLabels[i], i + 1); // ComboBox IDs are 1-based

            // Set current value
            int currentInt = 0;
            if (! currentVal.isVoid())
                currentInt = static_cast<int> (currentVal);
            else if (! param.defaultValue.isVoid())
                currentInt = static_cast<int> (param.defaultValue);

            ctrl.comboBox->setSelectedId (currentInt + 1, juce::dontSendNotification);

            if (param.description.isNotEmpty())
                ctrl.comboBox->setTooltip (param.description);

            auto nodeId = currentNodeId_;
            auto paramName = param.name;
            ctrl.comboBox->onChange = [this, nodeId, paramName, combo = ctrl.comboBox.get()]()
            {
                if (combo == nullptr) return;
                int selectedIdx = combo->getSelectedId() - 1; // back to 0-based
                model_.setNodeParam (nodeId, paramName, selectedIdx);
            };

            contentComponent_->addAndMakeVisible (*ctrl.comboBox);
        }
        else
        {
            // Slider for numeric params
            ctrl.isDiscrete = isDiscreteParam (param);
            ctrl.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal,
                                                           juce::Slider::TextBoxRight);
            ctrl.slider->setColour (juce::Slider::backgroundColourId, juce::Colour (Theme::kSliderBackground));
            ctrl.slider->setColour (juce::Slider::trackColourId, juce::Colour (Theme::kSliderTrack));
            ctrl.slider->setColour (juce::Slider::thumbColourId, juce::Colour (Theme::kSliderThumb));
            ctrl.slider->setColour (juce::Slider::textBoxTextColourId, juce::Colour (Theme::kTextPrimary));
            ctrl.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (Theme::kSliderTextBg));

            double minVal = param.minValue.isVoid() ? 0.0 : static_cast<double> (param.minValue);
            double maxVal = param.maxValue.isVoid() ? 1.0 : static_cast<double> (param.maxValue);
            double defVal = param.defaultValue.isVoid() ? 0.0 : static_cast<double> (param.defaultValue);

            const auto interval = ctrl.isDiscrete ? 1.0 : 0.001;
            ctrl.slider->setRange (minVal, maxVal, interval);
            ctrl.slider->setNumDecimalPlacesToDisplay (ctrl.isDiscrete ? 0 : 3);
            ctrl.slider->setValue (defVal, juce::dontSendNotification);

            // Apply suffix
            if (param.suffix.isNotEmpty())
                ctrl.slider->setTextValueSuffix (" " + param.suffix);

            if (param.description.isNotEmpty())
                ctrl.slider->setTooltip (param.description);

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

            contentComponent_->addAndMakeVisible (*ctrl.slider);
        }

        layoutItems_.push_back ({ LayoutItem::Param, static_cast<int> (controls_.size()) });
        controls_.push_back (std::move (ctrl));
    }

    viewport_.setViewedComponent (contentComponent_.get(), false);
    resized();
}

} // namespace pf
