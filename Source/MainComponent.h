#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
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
    struct PresetEntry
    {
        juce::String displayName;
        juce::File file;
        bool isFactory = false;
    };

    void saveToFile();
    void loadFromFile();
    void showAudioSettings();
    void refreshPresets (bool preserveSelection);
    void loadPresetFile (const juce::File& file);
    void saveCurrentAsPreset();
    void updateSelectedPreset();
    void deleteSelectedPreset();
    void revealSelectedPresetInFinder();
    void showPresetActionsMenu();
    void updatePresetStatusLabel();
    void appendPresetRefreshWarning (const juce::String& warning);
    void reloadFactoryPresetsFromSourceTree();
    juce::File resolveFactoryPresetDirectory() const;
    juce::File resolveSourceFactoryPresetDirectory() const;
    juce::File resolveBundledFactoryPresetDirectory() const;
    juce::File getUserPresetDirectory() const;

    GraphModel graphModel_;
    GraphCompiler graphCompiler_;
    AudioEngine audioEngine_;

    InspectorPanel inspectorPanel_;
    NodeEditorComponent nodeEditor_;
    VisualCanvas visualCanvas_;

    juce::StretchableLayoutManager layoutManager_;
    juce::StretchableLayoutResizerBar resizeBar_;

    std::unique_ptr<juce::MenuBarComponent> menuBar_;
    juce::ComboBox presetComboBox_;
    juce::TextButton presetActionsButton_ { "Preset..." };
    juce::Label presetStatusLabel_;

    std::vector<PresetEntry> presetEntries_;
    juce::File selectedPresetFile_;
    juce::File resolvedFactoryPresetDir_;
    juce::String presetRefreshWarning_;
#if JUCE_DEBUG
    juce::File factoryPresetOverrideDir_;
    bool hasLoggedManifestMismatch_ = false;
#endif
    bool isRefreshingPresets_ = false;
    int presetRowHeight_ = 30;
    double cachedAudioSampleRate_ = 0.0;
    int cachedAudioBlockSize_ = 0;
};

} // namespace pf
