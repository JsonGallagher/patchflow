#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/GraphModel.h"
#include "Nodes/NodeRegistry.h"
#include "UI/NodeComponent.h"
#include "UI/CableComponent.h"
#include "UI/NodePaletteComponent.h"
#include <memory>
#include <optional>
#include <unordered_map>

namespace pf
{

class InspectorPanel;

/**
 * The main node editor canvas. Handles zoom, pan, background grid,
 * node placement, cable drawing, and selection.
 */
class NodeEditorComponent : public juce::Component,
                             public GraphModel::Listener
{
public:
    NodeEditorComponent (GraphModel& model, InspectorPanel& inspector);
    ~NodeEditorComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

    //==============================================================================
    // Called by NodeComponent / PortComponent
    void nodeSelected (const juce::String& nodeId, bool addToSelection);
    void nodeMoved (const juce::String& nodeId);
    void nodeFinishedDragging (const juce::String& nodeId, juce::Point<float> newPos);
    void showNodeContextMenu (NodeComponent& node);

    // Port interaction for cable drawing
    void portDragStarted (PortComponent* port);
    void portDragMoved (juce::Point<float> position);
    void portDragEnded (PortComponent* targetPort);
    PortComponent* findPortAt (juce::Point<float> position) const;

    // Add node
    void addNodeAt (const juce::String& typeId, float x, float y);

    // View
    void zoomToFit();

    //==============================================================================
    // GraphModel::Listener
    void graphChanged() override;

private:
    void rebuildFromModel();
    void updateCablePositions();
    void paintGrid (juce::Graphics& g);
    void deleteSelectedNodes();
    void selectAllNodes();
    void duplicateSelectedNodes();
    std::optional<Connection> findConnectionForInputPort (const PortComponent* inputPort) const;
    CableComponent* findCableAt (juce::Point<float> position) const;

    // Port highlighting during cable drag
    void updatePortHighlights();
    void clearPortHighlights();

    // Grid snap
    static juce::Point<float> snapToGrid (juce::Point<float> pos);

    GraphModel& model_;
    InspectorPanel& inspector_;

    std::unordered_map<std::string, std::unique_ptr<NodeComponent>> nodeComponents_;
    std::vector<std::unique_ptr<CableComponent>> cableComponents_;

    std::unique_ptr<NodePaletteComponent> palette_;

    // Selection
    std::vector<juce::String> selectedNodes_;

    // Lasso selection
    bool isLassoing_ = false;
    juce::Point<float> lassoStart_;
    juce::Rectangle<float> lassoRect_;

    // Cable dragging state
    PortComponent* dragSourcePort_ = nullptr;
    juce::Point<float> dragCurrentPos_;
    bool isDraggingCable_ = false;

    // Pan/zoom
    juce::Point<float> viewOffset_ { 0.f, 0.f };
    float zoomScale_ = 1.0f;
    bool isPanning_ = false;
    juce::Point<float> panStart_;

    bool isRebuilding_ = false;
};

} // namespace pf
