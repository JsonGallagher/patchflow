#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Graph/GraphModel.h"
#include "Graph/GraphCompiler.h"
#include "Audio/AudioEngine.h"
#include "UI/NodeEditorComponent.h"
#include "UI/InspectorPanel.h"
#include "Rendering/VisualCanvas.h"

namespace pf
{

class MainComponent : public juce::Component,
                       public juce::MenuBarModel,
                       public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    //==============================================================================
    // Timer for periodic graph recompile
    void timerCallback() override;

private:
    void saveToFile();
    void loadFromFile();
    void showAudioSettings();

    GraphModel graphModel_;
    GraphCompiler graphCompiler_;
    AudioEngine audioEngine_;

    InspectorPanel inspectorPanel_;
    NodeEditorComponent nodeEditor_;
    VisualCanvas visualCanvas_;

    juce::StretchableLayoutManager layoutManager_;
    juce::StretchableLayoutResizerBar resizeBar_;

    std::unique_ptr<juce::MenuBarComponent> menuBar_;
};

} // namespace pf
