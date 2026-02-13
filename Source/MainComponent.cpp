#include "MainComponent.h"

namespace pf
{

MainComponent::MainComponent()
    : graphCompiler_ (graphModel_),
      inspectorPanel_ (graphModel_),
      nodeEditor_ (graphModel_, inspectorPanel_),
      resizeBar_ (&layoutManager_, 1, false)
{
    // Menu bar
    menuBar_ = std::make_unique<juce::MenuBarComponent> (this);
    addAndMakeVisible (*menuBar_);

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

    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine_.shutdown();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2a));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    menuBar_->setBounds (bounds.removeFromTop (24));

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
        case 4:  showAudioSettings(); break;
        case 10: graphModel_.getUndoManager().undo(); break;
        case 11: graphModel_.getUndoManager().redo(); break;
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
    auto chooser = std::make_shared<juce::FileChooser> ("Save Patch", juce::File {}, "*.json");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File {})
            {
                auto json = graphModel_.toJSON();
                file.replaceWithText (json);
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
                graphModel_.loadFromJSON (json);
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
    opts.dialogBackgroundColour = juce::Colour (0xff1a1a2a);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}

} // namespace pf
