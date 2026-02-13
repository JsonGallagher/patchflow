#include "UI/NodeEditorComponent.h"
#include "UI/InspectorPanel.h"

namespace pf
{

NodeEditorComponent::NodeEditorComponent (GraphModel& model, InspectorPanel& inspector)
    : model_ (model), inspector_ (inspector)
{
    model_.addListener (this);
    palette_ = std::make_unique<NodePaletteComponent> (*this);
    addChildComponent (*palette_);
    setWantsKeyboardFocus (true);
}

NodeEditorComponent::~NodeEditorComponent()
{
    model_.removeListener (this);
}

void NodeEditorComponent::paintGrid (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float gridSize = 20.f * zoomScale_;

    float startX = std::fmod (viewOffset_.x * zoomScale_, gridSize);
    float startY = std::fmod (viewOffset_.y * zoomScale_, gridSize);

    g.setColour (juce::Colour (0xff252535));
    for (float x = startX; x < bounds.getWidth(); x += gridSize)
        g.drawVerticalLine (static_cast<int> (x), 0, bounds.getHeight());
    for (float y = startY; y < bounds.getHeight(); y += gridSize)
        g.drawHorizontalLine (static_cast<int> (y), 0, bounds.getWidth());

    // Major grid
    float majorGrid = gridSize * 5.f;
    float majorStartX = std::fmod (viewOffset_.x * zoomScale_, majorGrid);
    float majorStartY = std::fmod (viewOffset_.y * zoomScale_, majorGrid);

    g.setColour (juce::Colour (0xff303045));
    for (float x = majorStartX; x < bounds.getWidth(); x += majorGrid)
        g.drawVerticalLine (static_cast<int> (x), 0, bounds.getHeight());
    for (float y = majorStartY; y < bounds.getHeight(); y += majorGrid)
        g.drawHorizontalLine (static_cast<int> (y), 0, bounds.getWidth());
}

void NodeEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2a));
    paintGrid (g);

    // Draw dragging cable
    if (isDraggingCable_ && dragSourcePort_)
    {
        auto start = dragSourcePort_->getCentreInParentEditor();
        auto colour = getPortColour (dragSourcePort_->getType()).withAlpha (0.6f);
        g.setColour (colour);

        juce::Path path;
        path.startNewSubPath (start);
        float dx = std::abs (dragCurrentPos_.x - start.x);
        float ctrl = juce::jmax (50.f, dx * 0.4f);
        path.cubicTo (start.x + ctrl, start.y,
                      dragCurrentPos_.x - ctrl, dragCurrentPos_.y,
                      dragCurrentPos_.x, dragCurrentPos_.y);
        g.strokePath (path, juce::PathStrokeType (2.f, juce::PathStrokeType::curved));
    }
}

void NodeEditorComponent::resized()
{
    updateCablePositions();
}

void NodeEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    if (palette_->isVisible())
    {
        palette_->setVisible (false);
        return;
    }

    if (e.mods.isRightButtonDown())
    {
        // Right-click: show palette
        palette_->setVisible (true);
        palette_->setBounds (e.getPosition().x, e.getPosition().y, 200, 300);
        palette_->show (e.getPosition());
        return;
    }

    // Check if we clicked on a port
    auto* port = findPortAt (e.getPosition().toFloat());
    if (port && port->getDirection() == PortDirection::Output)
    {
        portDragStarted (port);
        return;
    }

    if (e.mods.isMiddleButtonDown() || (e.mods.isLeftButtonDown() && juce::ModifierKeys::currentModifiers.isAltDown()))
    {
        isPanning_ = true;
        panStart_ = viewOffset_;
        return;
    }

    // Click on empty space: deselect
    selectedNodes_.clear();
    inspector_.clearSelection();
    for (auto& [_, nc] : nodeComponents_)
        nc->setSelected (false);
}

void NodeEditorComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (isPanning_)
    {
        viewOffset_ = panStart_ + (e.getPosition() - e.getMouseDownPosition()).toFloat() / zoomScale_;
        rebuildFromModel();
        return;
    }

    if (isDraggingCable_)
    {
        portDragMoved (e.getPosition().toFloat());
        repaint();
    }
}

void NodeEditorComponent::mouseUp (const juce::MouseEvent& e)
{
    if (isPanning_)
    {
        isPanning_ = false;
        return;
    }

    if (isDraggingCable_)
    {
        auto* target = findPortAt (e.getPosition().toFloat());
        portDragEnded (target);
        isDraggingCable_ = false;
        dragSourcePort_ = nullptr;
        repaint();
    }
}

void NodeEditorComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        float delta = wheel.deltaY * 0.1f;
        float newZoom = juce::jlimit (0.25f, 3.0f, zoomScale_ + delta);
        zoomScale_ = newZoom;
        rebuildFromModel();
    }
}

bool NodeEditorComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedNodes();
        return true;
    }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        model_.getUndoManager().undo();
        return true;
    }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        model_.getUndoManager().redo();
        return true;
    }

    return false;
}

//==============================================================================
void NodeEditorComponent::nodeSelected (const juce::String& nodeId, bool addToSelection)
{
    if (! addToSelection)
    {
        selectedNodes_.clear();
        for (auto& [_, nc] : nodeComponents_)
            nc->setSelected (false);
    }

    selectedNodes_.push_back (nodeId);
    auto it = nodeComponents_.find (nodeId.toStdString());
    if (it != nodeComponents_.end())
        it->second->setSelected (true);

    // Update inspector
    auto typeId = model_.getNodeTypeId (nodeId);
    auto node = NodeRegistry::instance().createNode (typeId);
    if (node)
        inspector_.setSelectedNode (nodeId, node->getParams());
}

void NodeEditorComponent::nodeMoved (const juce::String& /*nodeId*/)
{
    updateCablePositions();
    repaint();
}

void NodeEditorComponent::nodeFinishedDragging (const juce::String& nodeId, juce::Point<float> newPos)
{
    model_.setNodePosition (nodeId, newPos.x, newPos.y);
}

void NodeEditorComponent::showNodeContextMenu (NodeComponent& node)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Delete");
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, id = node.getNodeId()] (int result) {
        if (result == 1)
            model_.removeNode (id);
    });
}

//==============================================================================
void NodeEditorComponent::portDragStarted (PortComponent* port)
{
    dragSourcePort_ = port;
    isDraggingCable_ = true;
    dragCurrentPos_ = port->getCentreInParentEditor();
}

void NodeEditorComponent::portDragMoved (juce::Point<float> position)
{
    dragCurrentPos_ = position;
}

void NodeEditorComponent::portDragEnded (PortComponent* targetPort)
{
    if (! dragSourcePort_ || ! targetPort) return;
    if (targetPort->getDirection() != PortDirection::Input) return;
    if (! canConnect (dragSourcePort_->getType(), targetPort->getType())) return;
    if (&dragSourcePort_->getOwner() == &targetPort->getOwner()) return; // no self-connections

    Connection conn;
    conn.sourceNode = dragSourcePort_->getOwner().getNodeId();
    conn.sourcePort = dragSourcePort_->getPortIndex();
    conn.destNode   = targetPort->getOwner().getNodeId();
    conn.destPort   = targetPort->getPortIndex();

    model_.addConnection (conn);
}

PortComponent* NodeEditorComponent::findPortAt (juce::Point<float> position) const
{
    for (auto& [_, nc] : nodeComponents_)
    {
        auto localPos = position - nc->getPosition().toFloat();

        for (int i = 0; i < nc->getNumInputPorts(); ++i)
        {
            auto* port = nc->getInputPort (i);
            if (port->getBounds().toFloat().expanded (4.f).contains (localPos))
                return port;
        }
        for (int i = 0; i < nc->getNumOutputPorts(); ++i)
        {
            auto* port = nc->getOutputPort (i);
            if (port->getBounds().toFloat().expanded (4.f).contains (localPos))
                return port;
        }
    }
    return nullptr;
}

void NodeEditorComponent::addNodeAt (const juce::String& typeId, float x, float y)
{
    model_.addNode (typeId, x, y);
}

//==============================================================================
void NodeEditorComponent::graphChanged()
{
    if (isRebuilding_) return;
    juce::MessageManager::callAsync ([this] { rebuildFromModel(); });
}

void NodeEditorComponent::rebuildFromModel()
{
    isRebuilding_ = true;

    // Remove old components
    nodeComponents_.clear();
    cableComponents_.clear();

    auto nodeIds = model_.getAllNodeIds();
    for (auto& id : nodeIds)
    {
        auto typeId = model_.getNodeTypeId (id);
        auto proto = NodeRegistry::instance().createNode (typeId);
        if (! proto) continue;

        auto nodeTree = model_.getNodeTree (id);
        float x = static_cast<float> (nodeTree[IDs::x]);
        float y = static_cast<float> (nodeTree[IDs::y]);

        auto nc = std::make_unique<NodeComponent> (*this, id, typeId,
                                                     proto->getDisplayName(),
                                                     proto->getInputs(),
                                                     proto->getOutputs());
        nc->setTopLeftPosition (static_cast<int> (x), static_cast<int> (y));

        // Restore selection
        for (auto& sel : selectedNodes_)
            if (sel == id)
                nc->setSelected (true);

        addAndMakeVisible (*nc);
        nodeComponents_[id.toStdString()] = std::move (nc);
    }

    // Build cables
    auto connections = model_.getAllConnections();
    for (auto& conn : connections)
    {
        auto srcIt = nodeComponents_.find (conn.sourceNode.toStdString());
        auto dstIt = nodeComponents_.find (conn.destNode.toStdString());
        if (srcIt == nodeComponents_.end() || dstIt == nodeComponents_.end()) continue;

        auto* srcPort = srcIt->second->getOutputPort (conn.sourcePort);
        if (! srcPort) continue;

        auto cable = std::make_unique<CableComponent> (conn, srcPort->getType());
        addAndMakeVisible (*cable);
        cable->toBack();
        cableComponents_.push_back (std::move (cable));
    }

    updateCablePositions();
    isRebuilding_ = false;
    repaint();
}

void NodeEditorComponent::updateCablePositions()
{
    for (auto& cable : cableComponents_)
    {
        auto& conn = cable->getConnection();
        auto srcIt = nodeComponents_.find (conn.sourceNode.toStdString());
        auto dstIt = nodeComponents_.find (conn.destNode.toStdString());
        if (srcIt == nodeComponents_.end() || dstIt == nodeComponents_.end()) continue;

        auto* srcPort = srcIt->second->getOutputPort (conn.sourcePort);
        auto* dstPort = dstIt->second->getInputPort (conn.destPort);
        if (! srcPort || ! dstPort) continue;

        cable->setEndPoints (srcPort->getCentreInParentEditor(),
                             dstPort->getCentreInParentEditor());
    }
}

void NodeEditorComponent::deleteSelectedNodes()
{
    for (auto& id : selectedNodes_)
        model_.removeNode (id);
    selectedNodes_.clear();
    inspector_.clearSelection();
}

} // namespace pf
