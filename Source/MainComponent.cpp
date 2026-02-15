#include "MainComponent.h"
#include "UI/Theme.h"
#include <algorithm>

namespace pf
{

namespace
{
constexpr int kMenuBarHeight = 24;

enum PresetActionIds
{
    presetActionRefresh = 1,
    presetActionSaveAs,
    presetActionUpdate,
    presetActionReveal,
    presetActionDelete
};

juce::String sanitisePresetFileStem (juce::String name)
{
    auto stem = name.trim();
    for (auto c : juce::String ("\\/:*?\"<>|"))
        stem = stem.replaceCharacter (c, '_');

    stem = stem.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
    stem = stem.trim().replaceCharacter (' ', '_');

    if (stem.isEmpty())
        stem = "preset";

    return stem;
}
} // namespace

MainComponent::MainComponent()
    : graphCompiler_ (graphModel_),
      inspectorPanel_ (graphModel_),
      nodeEditor_ (graphModel_, inspectorPanel_),
      resizeBar_ (&layoutManager_, 1, false)
{
    // Menu bar
    menuBar_ = std::make_unique<juce::MenuBarComponent> (this);
    addAndMakeVisible (*menuBar_);

    presetComboBox_.setTextWhenNothingSelected ("Select preset");
    presetComboBox_.setTooltip ("Factory and user presets");
    presetComboBox_.onChange = [this]
    {
        if (isRefreshingPresets_)
            return;

        auto idx = presetComboBox_.getSelectedItemIndex();
        if (idx >= 0 && idx < static_cast<int> (presetEntries_.size()))
            loadPresetFile (presetEntries_[static_cast<size_t> (idx)].file);
    };
    addAndMakeVisible (presetComboBox_);

    presetActionsButton_.onClick = [this] { showPresetActionsMenu(); };
    addAndMakeVisible (presetActionsButton_);

    presetStatusLabel_.setJustificationType (juce::Justification::centredLeft);
    presetStatusLabel_.setColour (juce::Label::textColourId, juce::Colour (Theme::kPresetStatusText));
    presetStatusLabel_.setText ("No preset loaded", juce::dontSendNotification);
    addAndMakeVisible (presetStatusLabel_);

    addAndMakeVisible (nodeEditor_);
    addAndMakeVisible (resizeBar_);
    addAndMakeVisible (visualCanvas_);
    addAndMakeVisible (inspectorPanel_);

    // Layout: [nodeEditor | resizeBar | visualCanvas | inspector]
    layoutManager_.setItemLayout (0, 200, -1.0, -0.55);  // node editor: 55%
    layoutManager_.setItemLayout (1, 4, 4, 4);            // resize bar
    layoutManager_.setItemLayout (2, 150, -1.0, -0.30);   // visual canvas: 30%
    layoutManager_.setItemLayout (3, 180, 300, 220);       // inspector: fixed

    // Audio setup
    audioEngine_.initialise();
    graphCompiler_.setSampleRateAndBlockSize (44100.0, 512);

    // Wire up analysis FIFO to visual canvas
    visualCanvas_.setAnalysisFIFO (&audioEngine_.getAnalysisFIFO());

    // Initial compile
    graphCompiler_.compile();

    // Timer to push compiled graphs to audio engine + visual canvas
    startTimerHz (30);

    refreshPresets (false);
    if (! presetEntries_.empty())
    {
        presetComboBox_.setSelectedItemIndex (0, juce::dontSendNotification);
        loadPresetFile (presetEntries_.front().file);
    }

    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine_.shutdown();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (Theme::kBgPrimary));

    // Preset row background
    auto presetRowBounds = juce::Rectangle<int> (0, kMenuBarHeight, getWidth(), presetRowHeight_);
    g.setColour (juce::Colour (Theme::kPresetRowBg));
    g.fillRect (presetRowBounds);
    g.setColour (juce::Colour (Theme::kBorderSubtle));
    g.drawHorizontalLine (kMenuBarHeight + presetRowHeight_ - 1, 0.f, static_cast<float> (getWidth()));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    menuBar_->setBounds (bounds.removeFromTop (kMenuBarHeight));

    auto presetRow = bounds.removeFromTop (presetRowHeight_).reduced (8, 4);
    const auto comboWidth = juce::jlimit (240, 440, presetRow.getWidth() / 3);

    presetComboBox_.setBounds (presetRow.removeFromLeft (comboWidth));
    presetRow.removeFromLeft (8);
    presetActionsButton_.setBounds (presetRow.removeFromLeft (96));
    presetRow.removeFromLeft (10);
    presetStatusLabel_.setBounds (presetRow);

    juce::Component* comps[] = { &nodeEditor_, &resizeBar_, &visualCanvas_, &inspectorPanel_ };
    layoutManager_.layOutComponents (comps, 4,
                                     bounds.getX(), bounds.getY(),
                                     bounds.getWidth(), bounds.getHeight(),
                                     false, true);
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "View" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem (1, "New");
        menu.addSeparator();
        menu.addItem (2, "Open...", true, false);
        menu.addItem (3, "Save...", true, false);
        menu.addSeparator();
        menu.addItem (5, "Refresh Presets");
        menu.addItem (6, "Save As Preset...");
        menu.addItem (7, "Update Selected Preset", selectedPresetFile_ != juce::File {});
        menu.addSeparator();
        menu.addItem (4, "Audio Settings...");
    }
    else if (menuIndex == 1) // Edit
    {
        menu.addItem (10, "Undo", graphModel_.getUndoManager().canUndo());
        menu.addItem (11, "Redo", graphModel_.getUndoManager().canRedo());
    }
    else if (menuIndex == 2) // View
    {
        menu.addItem (20, "Zoom to Fit");
    }

    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/)
{
    switch (menuItemID)
    {
        case 1:  graphModel_.clear(); break;
        case 2:  loadFromFile(); break;
        case 3:  saveToFile(); break;
        case 5:  refreshPresets (true); break;
        case 6:  saveCurrentAsPreset(); break;
        case 7:  updateSelectedPreset(); break;
        case 4:  showAudioSettings(); break;
        case 10: graphModel_.getUndoManager().undo(); break;
        case 11: graphModel_.getUndoManager().redo(); break;
        case 20: nodeEditor_.zoomToFit(); break;
        default: break;
    }
}

//==============================================================================
void MainComponent::timerCallback()
{
    // Push compiled graph to audio engine and visual canvas
    auto* graph = graphCompiler_.getLatestGraph();
    if (graph)
    {
        audioEngine_.setNewGraph (graph);
        visualCanvas_.setRuntimeGraph (graph);
    }
}

//==============================================================================
void MainComponent::saveToFile()
{
    auto startDir = selectedPresetFile_.existsAsFile() ? selectedPresetFile_.getParentDirectory() : juce::File {};
    auto chooser = std::make_shared<juce::FileChooser> ("Save Patch", startDir, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File {})
            {
                auto json = graphModel_.toJSON();
                if (file.replaceWithText (json))
                {
                    selectedPresetFile_ = file;
                    refreshPresets (true);
                }
            }
        });
}

void MainComponent::loadFromFile()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Open Patch", juce::File {}, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File {} && file.existsAsFile())
            {
                auto json = file.loadFileAsString();
                if (graphModel_.loadFromJSON (json))
                {
                    selectedPresetFile_ = file;
                    refreshPresets (true);
                }
            }
        });
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        audioEngine_.getDeviceManager(), 1, 2, 0, 0, false, false, true, true);
    selector->setSize (500, 300);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (selector);
    opts.dialogTitle = "Audio Settings";
    opts.dialogBackgroundColour = juce::Colour (Theme::kBgPrimary);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}

void MainComponent::refreshPresets (bool preserveSelection)
{
    const auto previousSelection = preserveSelection ? selectedPresetFile_ : juce::File {};

    isRefreshingPresets_ = true;
    presetComboBox_.clear (juce::dontSendNotification);
    presetEntries_.clear();

    auto appendPresetsFrom = [this] (const juce::File& directory, bool isFactory)
    {
        if (! directory.isDirectory())
            return;

        std::vector<juce::File> files;
        for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.json", juce::File::findFiles))
            files.push_back (entry.getFile());

        std::sort (files.begin(), files.end(),
                   [] (const juce::File& a, const juce::File& b)
                   {
                       return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
                   });

        for (const auto& file : files)
        {
            presetEntries_.push_back ({ file.getFileNameWithoutExtension(), file, isFactory });
        }
    };

    appendPresetsFrom (resolveFactoryPresetDirectory(), true);

    auto userPresetDir = getUserPresetDirectory();
    userPresetDir.createDirectory();
    appendPresetsFrom (userPresetDir, false);

    int selectedId = 0;
    for (size_t i = 0; i < presetEntries_.size(); ++i)
    {
        const auto itemId = static_cast<int> (i) + 1;
        const auto& entry = presetEntries_[i];
        const auto label = (entry.isFactory ? "Factory / " : "User / ") + entry.displayName;
        presetComboBox_.addItem (label, itemId);

        if (previousSelection != juce::File {} && entry.file == previousSelection)
            selectedId = itemId;
    }

    if (selectedId > 0)
        presetComboBox_.setSelectedId (selectedId, juce::dontSendNotification);
    else
        presetComboBox_.setSelectedId (0, juce::dontSendNotification);

    if (preserveSelection)
        selectedPresetFile_ = previousSelection;
    else
        selectedPresetFile_ = juce::File {};

    isRefreshingPresets_ = false;
    updatePresetStatusLabel();
}

void MainComponent::loadPresetFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Preset Missing",
                                                "The selected preset file no longer exists.");
        refreshPresets (true);
        return;
    }

    const auto json = file.loadFileAsString();
    if (! graphModel_.loadFromJSON (json))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Preset Load Failed",
                                                "This preset could not be parsed.");
        return;
    }

    selectedPresetFile_ = file;
    updatePresetStatusLabel();
}

void MainComponent::saveCurrentAsPreset()
{
    auto userPresetDir = getUserPresetDirectory();
    if (! userPresetDir.createDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Preset Save Failed",
                                                "Could not create the user preset directory.");
        return;
    }

    auto suggestedStem = selectedPresetFile_ != juce::File {}
                       ? selectedPresetFile_.getFileNameWithoutExtension()
                       : juce::String ("new_preset");
    suggestedStem = sanitisePresetFileStem (suggestedStem);

    auto chooser = std::make_shared<juce::FileChooser> (
        "Save Preset As",
        userPresetDir.getChildFile (suggestedStem + ".json"),
        "*.json");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                          | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, chooser] (const juce::FileChooser& fc)
                          {
                              auto targetFile = fc.getResult();
                              if (targetFile == juce::File {})
                                  return;

                              if (! targetFile.hasFileExtension ("json"))
                                  targetFile = targetFile.withFileExtension (".json");

                              if (! targetFile.replaceWithText (graphModel_.toJSON()))
                              {
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Preset Save Failed",
                                                                          "Could not write the preset file.");
                                  return;
                              }

                              selectedPresetFile_ = targetFile;
                              refreshPresets (true);
                          });
}

void MainComponent::updateSelectedPreset()
{
    if (selectedPresetFile_ == juce::File {})
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                "No Preset Selected",
                                                "Choose a preset first, then run Update.");
        return;
    }

    auto parentDir = selectedPresetFile_.getParentDirectory();
    if (! parentDir.exists() && ! parentDir.createDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Preset Update Failed",
                                                "Could not create the preset directory.");
        return;
    }

    if (selectedPresetFile_.existsAsFile() && ! selectedPresetFile_.hasWriteAccess())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Preset Is Read-Only",
                                                "This preset cannot be updated in place. Use Save As Preset instead.");
        return;
    }

    if (! selectedPresetFile_.replaceWithText (graphModel_.toJSON()))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Preset Update Failed",
                                                "Could not write to the selected preset file.");
        return;
    }

    refreshPresets (true);
}

void MainComponent::deleteSelectedPreset()
{
    if (selectedPresetFile_ == juce::File {})
        return;

    auto userPresetDir = getUserPresetDirectory();
    if (! selectedPresetFile_.isAChildOf (userPresetDir))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                "Cannot Delete Factory Preset",
                                                "Only user presets can be deleted.");
        return;
    }

    const auto confirmed = juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset?",
        "Delete \"" + selectedPresetFile_.getFileName() + "\"?",
        "Delete",
        "Cancel",
        this,
        nullptr);

    if (! confirmed)
        return;

    if (! selectedPresetFile_.deleteFile())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Delete Failed",
                                                "Could not delete the selected preset.");
        return;
    }

    selectedPresetFile_ = juce::File {};
    refreshPresets (false);
}

void MainComponent::revealSelectedPresetInFinder()
{
    if (selectedPresetFile_ != juce::File {} && selectedPresetFile_.existsAsFile())
        selectedPresetFile_.revealToUser();
}

void MainComponent::showPresetActionsMenu()
{
    juce::PopupMenu menu;

    const auto hasSelection = selectedPresetFile_ != juce::File {};
    const auto canDelete = hasSelection && selectedPresetFile_.isAChildOf (getUserPresetDirectory());

    menu.addItem (presetActionRefresh, "Refresh Presets");
    menu.addSeparator();
    menu.addItem (presetActionSaveAs, "Save As Preset...");
    menu.addItem (presetActionUpdate, "Update Selected Preset", hasSelection);
    menu.addItem (presetActionReveal, "Reveal Selected Preset", hasSelection);
    menu.addItem (presetActionDelete, "Delete Selected User Preset", canDelete);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetActionsButton_),
                        [this] (int result)
                        {
                            switch (result)
                            {
                                case presetActionRefresh: refreshPresets (true); break;
                                case presetActionSaveAs:  saveCurrentAsPreset(); break;
                                case presetActionUpdate:  updateSelectedPreset(); break;
                                case presetActionReveal:  revealSelectedPresetInFinder(); break;
                                case presetActionDelete:  deleteSelectedPreset(); break;
                                default: break;
                            }
                        });
}

void MainComponent::updatePresetStatusLabel()
{
    if (selectedPresetFile_ == juce::File {})
    {
        if (presetEntries_.empty())
            presetStatusLabel_.setText ("No presets found", juce::dontSendNotification);
        else
            presetStatusLabel_.setText ("No preset loaded", juce::dontSendNotification);

        presetStatusLabel_.setTooltip ({});
        return;
    }

    const auto userDir = getUserPresetDirectory();
    const auto factoryDir = resolveFactoryPresetDirectory();

    juce::String origin = "External";
    if (selectedPresetFile_.isAChildOf (userDir))
        origin = "User";
    else if (factoryDir != juce::File {} && selectedPresetFile_.isAChildOf (factoryDir))
        origin = "Factory";

    presetStatusLabel_.setText (origin + ": " + selectedPresetFile_.getFileNameWithoutExtension(),
                                juce::dontSendNotification);
    presetStatusLabel_.setTooltip (selectedPresetFile_.getFullPathName());
}

juce::File MainComponent::resolveFactoryPresetDirectory() const
{
    const auto hasPresetJson = [] (const juce::File& dir)
    {
        return dir.isDirectory() && dir.findChildFiles (juce::File::findFiles, false, "*.json").size() > 0;
    };

    std::vector<juce::File> candidates;

    const auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    const auto executableDir = appFile.getParentDirectory();       // .../Contents/MacOS
    const auto contentsDir = executableDir.getParentDirectory();   // .../Contents

    candidates.push_back (contentsDir.getChildFile ("Resources/ExamplePatches")); // bundled resources
    candidates.push_back (juce::File::getCurrentWorkingDirectory().getChildFile ("Resources/ExamplePatches"));

    const auto addWalkUpCandidates = [&] (juce::File startDir)
    {
        auto dir = startDir;
        for (int depth = 0; depth < 8; ++depth)
        {
            candidates.push_back (dir.getChildFile ("Resources/ExamplePatches"));

            auto parent = dir.getParentDirectory();
            if (parent == dir || parent == juce::File {})
                break;

            dir = parent;
        }
    };

    addWalkUpCandidates (juce::File::getCurrentWorkingDirectory());
    addWalkUpCandidates (executableDir);

    for (const auto& candidate : candidates)
        if (hasPresetJson (candidate))
            return candidate;

    return {};
}

juce::File MainComponent::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("PatchFlow")
        .getChildFile ("Presets");
}

} // namespace pf
