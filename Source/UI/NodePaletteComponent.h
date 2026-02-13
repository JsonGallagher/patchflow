#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Nodes/NodeRegistry.h"

namespace pf
{

class NodeEditorComponent;

/**
 * Popup palette for adding nodes. Shows categories and node types.
 */
class NodePaletteComponent : public juce::Component,
                              public juce::TextEditor::Listener
{
public:
    explicit NodePaletteComponent (NodeEditorComponent& editor);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void show (juce::Point<int> position);
    void textEditorTextChanged (juce::TextEditor& editor) override;

private:
    void buildFilteredList (const juce::String& filter);
    void addNodeFromSelection (int index);

    NodeEditorComponent& editor_;
    juce::TextEditor searchBox_;
    juce::ListBox listBox_;
    juce::Point<int> spawnPosition_;

    struct ListEntry
    {
        juce::String typeId;
        juce::String displayName;
        juce::String category;
    };
    std::vector<ListEntry> entries_;

    class ListModel : public juce::ListBoxModel
    {
    public:
        std::vector<ListEntry>* entries = nullptr;
        NodePaletteComponent* owner = nullptr;

        int getNumRows() override { return entries ? static_cast<int> (entries->size()) : 0; }

        void paintListBoxItem (int rowNumber, juce::Graphics& g,
                               int width, int height, bool isSelected) override
        {
            if (! entries || rowNumber >= static_cast<int> (entries->size())) return;

            if (isSelected)
                g.fillAll (juce::Colour (0xff3a3a5a));

            auto& entry = (*entries)[rowNumber];
            g.setColour (juce::Colour (0xff888899));
            g.setFont (10.f);
            g.drawText (entry.category, 8, 0, width, height, juce::Justification::topLeft);

            g.setColour (juce::Colours::white);
            g.setFont (13.f);
            g.drawText (entry.displayName, 8, 4, width - 16, height, juce::Justification::centredLeft);
        }

        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
        {
            if (owner) owner->addNodeFromSelection (row);
        }

        void returnKeyPressed (int row) override
        {
            if (owner) owner->addNodeFromSelection (row);
        }
    };

    ListModel listModel_;
};

} // namespace pf
