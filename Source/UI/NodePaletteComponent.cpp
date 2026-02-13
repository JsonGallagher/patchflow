#include "UI/NodePaletteComponent.h"
#include "UI/NodeEditorComponent.h"

namespace pf
{

NodePaletteComponent::NodePaletteComponent (NodeEditorComponent& editor)
    : editor_ (editor)
{
    searchBox_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1e1e2e));
    searchBox_.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    searchBox_.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff555566));
    searchBox_.setTextToShowWhenEmpty ("Search nodes...", juce::Colour (0xff666677));
    searchBox_.addListener (this);
    addAndMakeVisible (searchBox_);

    listModel_.entries = &entries_;
    listModel_.owner = this;
    listBox_.setModel (&listModel_);
    listBox_.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1a2a));
    listBox_.setRowHeight (32);
    addAndMakeVisible (listBox_);

    buildFilteredList ("");
    setSize (200, 300);
}

void NodePaletteComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2a));
    g.setColour (juce::Colour (0xff555566));
    g.drawRect (getLocalBounds());
}

void NodePaletteComponent::resized()
{
    auto bounds = getLocalBounds().reduced (2);
    searchBox_.setBounds (bounds.removeFromTop (28));
    listBox_.setBounds (bounds);
}

void NodePaletteComponent::show (juce::Point<int> position)
{
    spawnPosition_ = position;
    searchBox_.clear();
    buildFilteredList ("");
    searchBox_.grabKeyboardFocus();
}

void NodePaletteComponent::textEditorTextChanged (juce::TextEditor& /*ed*/)
{
    buildFilteredList (searchBox_.getText());
}

void NodePaletteComponent::buildFilteredList (const juce::String& filter)
{
    entries_.clear();
    auto allTypes = NodeRegistry::instance().getAllNodeTypes();
    for (auto& info : allTypes)
    {
        if (filter.isEmpty()
            || info.displayName.containsIgnoreCase (filter)
            || info.category.containsIgnoreCase (filter)
            || info.typeId.containsIgnoreCase (filter))
        {
            entries_.push_back ({ info.typeId, info.displayName, info.category });
        }
    }
    listBox_.updateContent();
}

void NodePaletteComponent::addNodeFromSelection (int index)
{
    if (index < 0 || index >= static_cast<int> (entries_.size())) return;

    editor_.addNodeAt (entries_[index].typeId,
                       static_cast<float> (spawnPosition_.x),
                       static_cast<float> (spawnPosition_.y));
    setVisible (false);
}

} // namespace pf
