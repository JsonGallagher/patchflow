#include "MainComponent.h"
#include "UI/Theme.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace pf
{

namespace
{
constexpr int kMenuBarHeight = 24;
constexpr int kMenuActionReloadFactoryFromSourceTree = 8;
constexpr auto kPresetManifestFileName = ".preset_manifest.json";

bool hasPresetJson (const juce::File& dir)
{
    return dir.isDirectory() && dir.findChildFiles (juce::File::findFiles, false, "*.json").size() > 0;
}

void appendUniqueCandidate (std::vector<juce::File>& candidates, const juce::File& candidate)
{
    if (candidate == juce::File {})
        return;

    if (std::find (candidates.begin(), candidates.end(), candidate) == candidates.end())
        candidates.push_back (candidate);
}

void addWalkUpCandidates (std::vector<juce::File>& candidates, juce::File startDir)
{
    auto dir = startDir;
    for (int depth = 0; depth < 8; ++depth)
    {
        appendUniqueCandidate (candidates, dir.getChildFile ("Resources/ExamplePatches"));

        auto parent = dir.getParentDirectory();
        if (parent == dir || parent == juce::File {})
            break;

        dir = parent;
    }
}

juce::File firstValidPresetDirectory (const std::vector<juce::File>& candidates)
{
    for (const auto& candidate : candidates)
        if (hasPresetJson (candidate))
            return candidate;

    return {};
}

juce::String readPresetManifestVersion (const juce::File& presetDir)
{
    auto manifestFile = presetDir.getChildFile (kPresetManifestFileName);
    if (! manifestFile.existsAsFile())
        return {};

    auto parsed = juce::JSON::parse (manifestFile.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return {};

    auto version = root->getProperty ("version").toString().trim();
    return version.isNotEmpty() ? version : juce::String {};
}

#if JUCE_DEBUG
juce::String validateFactoryPresetFile (const juce::File& presetFile,
                                        const std::unordered_set<std::string>& knownTypeIds)
{
    auto parsed = juce::JSON::parse (presetFile.loadFileAsString());
    if (! parsed.isObject())
        return "JSON parse failed";

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return "Preset root is not an object";

    auto* nodes = root->getProperty ("nodes").getArray();
    if (nodes == nullptr)
        return "Missing nodes array";

    auto* connections = root->getProperty ("connections").getArray();
    if (connections == nullptr)
        return "Missing connections array";

    juce::StringArray outputCanvasIds;
    outputCanvasIds.ensureStorageAllocated (nodes->size());

    for (const auto& nodeVar : *nodes)
    {
        auto* nodeObj = nodeVar.getDynamicObject();
        if (nodeObj == nullptr)
            return "Node entry is not an object";

        auto nodeId = nodeObj->getProperty ("id").toString();
        auto typeId = nodeObj->getProperty ("typeId").toString();

        if (nodeId.isEmpty())
            return "Node entry missing id";
        if (typeId.isEmpty())
            return "Node \"" + nodeId + "\" missing typeId";

        if (knownTypeIds.find (typeId.toStdString()) == knownTypeIds.end())
            return "Unknown node typeId \"" + typeId + "\"";

        if (typeId == "OutputCanvas")
            outputCanvasIds.addIfNotAlreadyThere (nodeId);
    }

    if (outputCanvasIds.isEmpty())
        return "No OutputCanvas node";

    bool hasCanvasTextureConnection = false;
    for (const auto& connVar : *connections)
    {
        auto* connObj = connVar.getDynamicObject();
        if (connObj == nullptr)
            return "Connection entry is not an object";

        auto destNode = connObj->getProperty ("destNode").toString();
        auto destPortVar = connObj->getProperty ("destPort");
        auto destPort = destPortVar.isVoid() ? -1 : static_cast<int> (destPortVar);

        if (destPort == 0 && outputCanvasIds.contains (destNode))
        {
            hasCanvasTextureConnection = true;
            break;
        }
    }

    if (! hasCanvasTextureConnection)
        return "No connection into OutputCanvas.texture";

    return {};
}
#endif

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
    cachedAudioSampleRate_ = audioEngine_.getSampleRate();
    cachedAudioBlockSize_ = audioEngine_.getBlockSize();
    graphCompiler_.setSampleRateAndBlockSize (cachedAudioSampleRate_, cachedAudioBlockSize_);

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
#if JUCE_DEBUG
        menu.addItem (kMenuActionReloadFactoryFromSourceTree, "Reload Factory Presets From Source Tree");
#endif
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
#if JUCE_DEBUG
        case kMenuActionReloadFactoryFromSourceTree: reloadFactoryPresetsFromSourceTree(); break;
#endif
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
    const auto sampleRate = audioEngine_.getSampleRate();
    const auto blockSize = audioEngine_.getBlockSize();
    const auto sampleRateChanged = std::abs (sampleRate - cachedAudioSampleRate_) > 0.01;
    const auto blockSizeChanged = blockSize != cachedAudioBlockSize_;

    if (sampleRateChanged || blockSizeChanged)
    {
        cachedAudioSampleRate_ = sampleRate;
        cachedAudioBlockSize_ = blockSize;
        graphCompiler_.setSampleRateAndBlockSize (sampleRate, blockSize);
        graphCompiler_.compile();
    }

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
    presetRefreshWarning_.clear();
    resolvedFactoryPresetDir_ = resolveFactoryPresetDirectory();

    isRefreshingPresets_ = true;
    presetComboBox_.clear (juce::dontSendNotification);
    presetEntries_.clear();

#if JUCE_DEBUG
    std::unordered_set<std::string> knownTypeIds;
    for (const auto& nodeInfo : NodeRegistry::instance().getAllNodeTypes())
        knownTypeIds.insert (nodeInfo.typeId.toStdString());

    int invalidFactoryPresetCount = 0;

    auto sourceFactoryDir = resolveSourceFactoryPresetDirectory();
    auto bundledFactoryDir = resolveBundledFactoryPresetDirectory();
    if (hasPresetJson (sourceFactoryDir) && hasPresetJson (bundledFactoryDir) && sourceFactoryDir != bundledFactoryDir)
    {
        const auto sourceVersion = readPresetManifestVersion (sourceFactoryDir);
        const auto bundleVersion = readPresetManifestVersion (bundledFactoryDir);

        if (sourceVersion.isNotEmpty() && bundleVersion.isNotEmpty() && sourceVersion != bundleVersion)
        {
            const auto warning = "Factory bundle/source mismatch: src " + sourceVersion + ", bundle " + bundleVersion;
            appendPresetRefreshWarning (warning);

            if (! hasLoggedManifestMismatch_)
            {
                DBG ("PatchFlow: " + warning
                     + "\n  source: " + sourceFactoryDir.getFullPathName()
                     + "\n  bundle: " + bundledFactoryDir.getFullPathName());
                hasLoggedManifestMismatch_ = true;
            }
        }
    }
#endif

#if JUCE_DEBUG
    auto appendPresetsFrom = [this, &knownTypeIds, &invalidFactoryPresetCount] (const juce::File& directory, bool isFactory)
#else
    auto appendPresetsFrom = [this] (const juce::File& directory, bool isFactory)
#endif
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
#if JUCE_DEBUG
            if (isFactory)
            {
                auto validationError = validateFactoryPresetFile (file, knownTypeIds);
                if (validationError.isNotEmpty())
                {
                    ++invalidFactoryPresetCount;
                    DBG ("PatchFlow: preset validation failed for \""
                         + file.getFullPathName() + "\": " + validationError);
                }
            }
#endif
            presetEntries_.push_back ({ file.getFileNameWithoutExtension(), file, isFactory });
        }
    };

    appendPresetsFrom (resolvedFactoryPresetDir_, true);

    auto userPresetDir = getUserPresetDirectory();
    userPresetDir.createDirectory();
    appendPresetsFrom (userPresetDir, false);

#if JUCE_DEBUG
    if (invalidFactoryPresetCount > 0)
        appendPresetRefreshWarning ("Factory preset issues detected: " + juce::String (invalidFactoryPresetCount));
#endif

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

void MainComponent::appendPresetRefreshWarning (const juce::String& warning)
{
    const auto trimmed = warning.trim();
    if (trimmed.isEmpty())
        return;

    if (presetRefreshWarning_.isEmpty())
    {
        presetRefreshWarning_ = trimmed;
        return;
    }

    if (! presetRefreshWarning_.contains (trimmed))
        presetRefreshWarning_ += " | " + trimmed;
}

void MainComponent::reloadFactoryPresetsFromSourceTree()
{
#if JUCE_DEBUG
    auto sourceDir = resolveSourceFactoryPresetDirectory();
    if (sourceDir == juce::File {})
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Factory Presets Not Found",
                                                "Could not find Resources/ExamplePatches in the source tree.");
        return;
    }

    factoryPresetOverrideDir_ = sourceDir;
    refreshPresets (true);
#endif
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
        juce::String statusText;
        if (presetEntries_.empty())
            statusText = "No presets found";
        else
            statusText = "No preset loaded";

        if (presetRefreshWarning_.isNotEmpty())
            statusText += " | " + presetRefreshWarning_;

        presetStatusLabel_.setText (statusText, juce::dontSendNotification);

        presetStatusLabel_.setTooltip (presetRefreshWarning_.isNotEmpty() ? presetRefreshWarning_ : juce::String {});
        return;
    }

    const auto userDir = getUserPresetDirectory();
    const auto factoryDir = resolvedFactoryPresetDir_;

    juce::String origin = "External";
    if (selectedPresetFile_.isAChildOf (userDir))
        origin = "User";
    else if (factoryDir != juce::File {} && selectedPresetFile_.isAChildOf (factoryDir))
        origin = "Factory";

    auto statusText = origin + ": " + selectedPresetFile_.getFileNameWithoutExtension();
    if (presetRefreshWarning_.isNotEmpty())
        statusText += " | " + presetRefreshWarning_;

    auto tooltip = selectedPresetFile_.getFullPathName();
    if (origin == "Factory" && factoryDir != juce::File {})
        tooltip << "\nFactory dir: " << factoryDir.getFullPathName();
    if (presetRefreshWarning_.isNotEmpty())
        tooltip << "\nWarning: " << presetRefreshWarning_;

    presetStatusLabel_.setText (statusText, juce::dontSendNotification);
    presetStatusLabel_.setTooltip (tooltip);
}

juce::File MainComponent::resolveFactoryPresetDirectory() const
{
    auto bundledDir = resolveBundledFactoryPresetDirectory();
    auto sourceDir = resolveSourceFactoryPresetDirectory();

#if JUCE_DEBUG
    if (hasPresetJson (factoryPresetOverrideDir_))
        return factoryPresetOverrideDir_;

    std::vector<juce::File> candidates { sourceDir, bundledDir };
#else
    std::vector<juce::File> candidates { bundledDir, sourceDir };
#endif
    return firstValidPresetDirectory (candidates);
}

juce::File MainComponent::resolveSourceFactoryPresetDirectory() const
{
    std::vector<juce::File> sourceCandidates;
    appendUniqueCandidate (sourceCandidates,
                           juce::File::getCurrentWorkingDirectory().getChildFile ("Resources/ExamplePatches"));
    addWalkUpCandidates (sourceCandidates, juce::File::getCurrentWorkingDirectory());

    const auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    const auto executableDir = appFile.getParentDirectory();
    addWalkUpCandidates (sourceCandidates, executableDir);

    return firstValidPresetDirectory (sourceCandidates);
}

juce::File MainComponent::resolveBundledFactoryPresetDirectory() const
{
    const auto appFile = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    const auto executableDir = appFile.getParentDirectory();      // .../Contents/MacOS
    const auto contentsDir = executableDir.getParentDirectory();  // .../Contents
    return contentsDir.getChildFile ("Resources/ExamplePatches");
}

juce::File MainComponent::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("PatchFlow")
        .getChildFile ("Presets");
}

} // namespace pf
