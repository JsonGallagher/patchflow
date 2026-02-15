#include "UI/NodeEditorComponent.h"
#include "UI/InspectorPanel.h"
#include "UI/Theme.h"
#include <cmath>

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
    float gridSize = Theme::kGridSnapSize * zoomScale_;

    float startX = std::fmod (viewOffset_.x * zoomScale_, gridSize);
    float startY = std::fmod (viewOffset_.y * zoomScale_, gridSize);

    g.setColour (juce::Colour (Theme::kGridMinor));
    for (float x = startX; x < bounds.getWidth(); x += gridSize)
        g.drawVerticalLine (static_cast<int> (x), 0, bounds.getHeight());
    for (float y = startY; y < bounds.getHeight(); y += gridSize)
        g.drawHorizontalLine (static_cast<int> (y), 0, bounds.getWidth());

    // Major grid
    float majorGrid = gridSize * 5.f;
    float majorStartX = std::fmod (viewOffset_.x * zoomScale_, majorGrid);
    float majorStartY = std::fmod (viewOffset_.y * zoomScale_, majorGrid);

    g.setColour (juce::Colour (Theme::kGridMajor));
    for (float x = majorStartX; x < bounds.getWidth(); x += majorGrid)
        g.drawVerticalLine (static_cast<int> (x), 0, bounds.getHeight());
    for (float y = majorStartY; y < bounds.getHeight(); y += majorGrid)
        g.drawHorizontalLine (static_cast<int> (y), 0, bounds.getWidth());
}

void NodeEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (Theme::kBgPrimary));
    paintGrid (g);

    // Draw dragging cable with preview snap
    if (isDraggingCable_ && dragSourcePort_)
    {
        auto start = dragSourcePort_->getCentreInParentEditor();
        auto colour = getPortColour (dragSourcePort_->getType());

        // Find nearest compatible port for snap preview
        PortComponent* snapTarget = nullptr;
        float snapDist = 30.f;
        for (auto& [_, nc] : nodeComponents_)
        {
            if (&dragSourcePort_->getOwner() == nc.get()) continue;

            auto checkPorts = [&] (bool isInput)
            {
                int count = isInput ? nc->getNumInputPorts() : nc->getNumOutputPorts();
                for (int i = 0; i < count; ++i)
                {
                    auto* port = isInput ? nc->getInputPort (i) : nc->getOutputPort (i);
                    if (port->getDirection() == dragSourcePort_->getDirection()) continue;

                    PortType srcType = dragSourcePort_->getDirection() == PortDirection::Output
                                     ? dragSourcePort_->getType() : port->getType();
                    PortType dstType = dragSourcePort_->getDirection() == PortDirection::Input
                                     ? dragSourcePort_->getType() : port->getType();
                    if (! canConnect (srcType, dstType)) continue;

                    auto portCentre = port->getCentreInParentEditor();
                    float dist = portCentre.getDistanceFrom (dragCurrentPos_);
                    if (dist < snapDist)
                    {
                        snapDist = dist;
                        snapTarget = port;
                    }
                }
            };
            checkPorts (true);
            checkPorts (false);
        }

        auto endPos = snapTarget ? snapTarget->getCentreInParentEditor() : dragCurrentPos_;

        g.setColour (colour.withAlpha (0.7f));
        juce::Path path;
        path.startNewSubPath (start);
        float dx = std::abs (endPos.x - start.x);
        float ctrl = juce::jmax (50.f, dx * 0.4f);
        path.cubicTo (start.x + ctrl, start.y,
                      endPos.x - ctrl, endPos.y,
                      endPos.x, endPos.y);
        g.strokePath (path, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));

        // Glow
        g.setColour (colour.withAlpha (0.12f));
        g.strokePath (path, juce::PathStrokeType (8.f, juce::PathStrokeType::curved));

        // Snap dot
        if (snapTarget)
        {
            auto sp = snapTarget->getCentreInParentEditor();
            g.setColour (juce::Colour (Theme::kPortCompatible));
            g.fillEllipse (sp.x - 6.f, sp.y - 6.f, 12.f, 12.f);
        }
    }

    // Draw lasso selection rectangle
    if (isLassoing_)
    {
        g.setColour (juce::Colour (Theme::kLassoFill));
        g.fillRect (lassoRect_);
        g.setColour (juce::Colour (Theme::kLassoBorder));
        g.drawRect (lassoRect_, 1.f);
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
        // Right-click: show palette at click position
        palette_->setVisible (true);
        palette_->setBounds (e.getPosition().x, e.getPosition().y, 200, 300);
        palette_->show (e.getPosition());
        return;
    }

    // Check if we clicked on a port
    auto* port = findPortAt (e.getPosition().toFloat());
    if (port != nullptr)
    {
        portDragStarted (port);
        return;
    }

    if (e.mods.isLeftButtonDown() && e.getNumberOfClicks() >= 2)
    {
        if (auto* cable = findCableAt (e.getPosition().toFloat()))
        {
            model_.removeConnection (cable->getConnection());
            return;
        }
    }

    if (e.mods.isMiddleButtonDown() || (e.mods.isLeftButtonDown() && juce::ModifierKeys::currentModifiers.isAltDown()))
    {
        isPanning_ = true;
        panStart_ = viewOffset_;
        return;
    }

    // Left-click on empty space: start lasso selection
    if (e.mods.isLeftButtonDown())
    {
        if (! e.mods.isShiftDown())
        {
            selectedNodes_.clear();
            inspector_.clearSelection();
            for (auto& [_, nc] : nodeComponents_)
                nc->setSelected (false);
        }

        isLassoing_ = true;
        lassoStart_ = e.getPosition().toFloat();
        lassoRect_ = {};
    }
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
        updatePortHighlights();
        repaint();
        return;
    }

    if (isLassoing_)
    {
        auto current = e.getPosition().toFloat();
        lassoRect_ = juce::Rectangle<float> (
            juce::jmin (lassoStart_.x, current.x),
            juce::jmin (lassoStart_.y, current.y),
            std::abs (current.x - lassoStart_.x),
            std::abs (current.y - lassoStart_.y));

        // Highlight nodes within lasso
        for (auto& [id, nc] : nodeComponents_)
        {
            bool inLasso = lassoRect_.intersects (nc->getBounds().toFloat());
            nc->setSelected (inLasso);
        }
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
        clearPortHighlights();
        repaint();
        return;
    }

    if (isLassoing_)
    {
        isLassoing_ = false;

        // Finalize selection
        selectedNodes_.clear();
        for (auto& [id, nc] : nodeComponents_)
        {
            if (nc->isSelected())
                selectedNodes_.push_back (juce::String (id));
        }

        repaint();
    }
}

void NodeEditorComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Zoom toward cursor position
    float delta = wheel.deltaY * 0.1f;
    float newZoom = juce::jlimit (0.25f, 3.0f, zoomScale_ + delta);
    if (newZoom == zoomScale_) return;

    // World position under cursor before zoom
    auto mousePos = e.getPosition().toFloat();
    float worldX = mousePos.x / zoomScale_ - viewOffset_.x;
    float worldY = mousePos.y / zoomScale_ - viewOffset_.y;

    zoomScale_ = newZoom;

    // Adjust offset so world point stays under cursor
    viewOffset_.x = mousePos.x / zoomScale_ - worldX;
    viewOffset_.y = mousePos.y / zoomScale_ - worldY;

    rebuildFromModel();
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

    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        selectAllNodes();
        return true;
    }

    if (key == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0))
    {
        duplicateSelectedNodes();
        return true;
    }

    // Tab to open palette at center
    if (key == juce::KeyPress::tabKey)
    {
        auto centre = getLocalBounds().getCentre();
        palette_->setVisible (true);
        palette_->setBounds (centre.x - 100, centre.y - 150, 200, 300);
        palette_->show (centre);
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
        inspector_.setSelectedNode (nodeId, node->getParams(), node->getDisplayName());
}

void NodeEditorComponent::nodeMoved (const juce::String& /*nodeId*/)
{
    updateCablePositions();
    repaint();
}

void NodeEditorComponent::nodeFinishedDragging (const juce::String& nodeId, juce::Point<float> newPos)
{
    // Snap to grid
    auto snapped = snapToGrid (newPos);
    model_.setNodePosition (nodeId, snapped.x, snapped.y);
}

void NodeEditorComponent::showNodeContextMenu (NodeComponent& node)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Duplicate");
    menu.addItem (2, "Disconnect All");
    menu.addSeparator();
    menu.addItem (3, "Delete");

    auto nodeId = node.getNodeId();
    menu.showMenuAsync (juce::PopupMenu::Options(), [this, nodeId] (int result)
    {
        switch (result)
        {
            case 1: // Duplicate
            {
                auto nodeTree = model_.getNodeTree (nodeId);
                if (! nodeTree.isValid()) break;
                auto typeId = nodeTree[IDs::typeId].toString();
                float x = static_cast<float> (nodeTree[IDs::x]) + 30.f;
                float y = static_cast<float> (nodeTree[IDs::y]) + 30.f;
                auto newId = model_.addNode (typeId, x, y);
                auto srcParams = model_.getParamsTree (nodeId);
                auto dstParams = model_.getParamsTree (newId);
                if (srcParams.isValid() && dstParams.isValid())
                    for (int i = 0; i < srcParams.getNumProperties(); ++i)
                    {
                        auto name = srcParams.getPropertyName (i);
                        dstParams.setProperty (name, srcParams[name], nullptr);
                    }
                break;
            }
            case 2: // Disconnect All
            {
                auto conns = model_.getAllConnections();
                for (auto& conn : conns)
                    if (conn.sourceNode == nodeId || conn.destNode == nodeId)
                        model_.removeConnection (conn);
                break;
            }
            case 3: // Delete
                model_.removeNode (nodeId);
                break;
            default:
                break;
        }
    });
}

//==============================================================================
void NodeEditorComponent::portDragStarted (PortComponent* port)
{
    dragSourcePort_ = port;
    isDraggingCable_ = true;
    dragCurrentPos_ = port->getCentreInParentEditor();
    updatePortHighlights();
}

void NodeEditorComponent::portDragMoved (juce::Point<float> position)
{
    dragCurrentPos_ = position;
}

void NodeEditorComponent::portDragEnded (PortComponent* targetPort)
{
    if (! dragSourcePort_)
        return;

    PortComponent* sourcePort = nullptr;
    PortComponent* destPort = nullptr;

    if (dragSourcePort_->getDirection() == PortDirection::Output)
    {
        sourcePort = dragSourcePort_;
        if (targetPort != nullptr && targetPort->getDirection() == PortDirection::Input)
            destPort = targetPort;
    }
    else
    {
        destPort = dragSourcePort_;
        if (targetPort != nullptr && targetPort->getDirection() == PortDirection::Output)
            sourcePort = targetPort;
    }

    // Dragging from an input to empty space disconnects it.
    if (dragSourcePort_->getDirection() == PortDirection::Input && sourcePort == nullptr)
    {
        if (targetPort == dragSourcePort_)
            return;

        if (auto existing = findConnectionForInputPort (destPort))
            model_.removeConnection (*existing);
        return;
    }

    if (sourcePort == nullptr || destPort == nullptr)
        return;

    if (! canConnect (sourcePort->getType(), destPort->getType()))
        return;

    if (&sourcePort->getOwner() == &destPort->getOwner())
        return; // no self-connections

    if (auto existing = findConnectionForInputPort (destPort))
    {
        const bool sameConnection = (existing->sourceNode == sourcePort->getOwner().getNodeId()
                                     && existing->sourcePort == sourcePort->getPortIndex());
        if (sameConnection)
            return;

        // Replace existing destination connection on drop.
        model_.removeConnection (*existing);
    }

    Connection conn;
    conn.sourceNode = sourcePort->getOwner().getNodeId();
    conn.sourcePort = sourcePort->getPortIndex();
    conn.destNode   = destPort->getOwner().getNodeId();
    conn.destPort   = destPort->getPortIndex();

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
            if (port->getBounds().toFloat().expanded (8.f).contains (localPos))
                return port;
        }
        for (int i = 0; i < nc->getNumOutputPorts(); ++i)
        {
            auto* port = nc->getOutputPort (i);
            if (port->getBounds().toFloat().expanded (8.f).contains (localPos))
                return port;
        }
    }
    return nullptr;
}

std::optional<Connection> NodeEditorComponent::findConnectionForInputPort (const PortComponent* inputPort) const
{
    if (inputPort == nullptr || inputPort->getDirection() != PortDirection::Input)
        return std::nullopt;

    const auto inputNodeId = inputPort->getOwner().getNodeId();
    const auto inputPortIndex = inputPort->getPortIndex();
    const auto connections = model_.getAllConnections();

    for (const auto& connection : connections)
    {
        if (connection.destNode == inputNodeId && connection.destPort == inputPortIndex)
            return connection;
    }

    return std::nullopt;
}

CableComponent* NodeEditorComponent::findCableAt (juce::Point<float> position) const
{
    for (auto it = cableComponents_.rbegin(); it != cableComponents_.rend(); ++it)
    {
        auto* cable = it->get();
        const auto localPos = position - cable->getPosition().toFloat();
        if (cable->hitTest (juce::roundToInt (localPos.x), juce::roundToInt (localPos.y)))
            return cable;
    }

    return nullptr;
}

void NodeEditorComponent::addNodeAt (const juce::String& typeId, float x, float y)
{
    auto snapped = snapToGrid ({ x, y });
    model_.addNode (typeId, snapped.x, snapped.y);
}

//==============================================================================
void NodeEditorComponent::updatePortHighlights()
{
    if (! dragSourcePort_) return;

    for (auto& [_, nc] : nodeComponents_)
    {
        if (&dragSourcePort_->getOwner() == nc.get())
            continue;

        auto highlightPorts = [&] (bool isInput)
        {
            int count = isInput ? nc->getNumInputPorts() : nc->getNumOutputPorts();
            for (int i = 0; i < count; ++i)
            {
                auto* port = isInput ? nc->getInputPort (i) : nc->getOutputPort (i);
                if (port->getDirection() == dragSourcePort_->getDirection())
                {
                    port->setHighlight (PortComponent::Highlight::None);
                    continue;
                }

                PortType srcType = dragSourcePort_->getDirection() == PortDirection::Output
                                 ? dragSourcePort_->getType() : port->getType();
                PortType dstType = dragSourcePort_->getDirection() == PortDirection::Input
                                 ? dragSourcePort_->getType() : port->getType();

                if (canConnect (srcType, dstType))
                    port->setHighlight (PortComponent::Highlight::Compatible);
                else
                    port->setHighlight (PortComponent::Highlight::Incompatible);
            }
        };
        highlightPorts (true);
        highlightPorts (false);
    }
}

void NodeEditorComponent::clearPortHighlights()
{
    for (auto& [_, nc] : nodeComponents_)
    {
        for (int i = 0; i < nc->getNumInputPorts(); ++i)
            nc->getInputPort (i)->setHighlight (PortComponent::Highlight::None);
        for (int i = 0; i < nc->getNumOutputPorts(); ++i)
            nc->getOutputPort (i)->setHighlight (PortComponent::Highlight::None);
    }
}

juce::Point<float> NodeEditorComponent::snapToGrid (juce::Point<float> pos)
{
    float grid = Theme::kGridSnapSize;
    return { std::round (pos.x / grid) * grid,
             std::round (pos.y / grid) * grid };
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
                                                     proto->getCategory(),
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

void NodeEditorComponent::zoomToFit()
{
    if (nodeComponents_.empty())
    {
        viewOffset_ = {};
        zoomScale_ = 1.0f;
        rebuildFromModel();
        return;
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (auto& [_, nc] : nodeComponents_)
    {
        auto bounds = nc->getBounds().toFloat();
        minX = juce::jmin (minX, bounds.getX());
        minY = juce::jmin (minY, bounds.getY());
        maxX = juce::jmax (maxX, bounds.getRight());
        maxY = juce::jmax (maxY, bounds.getBottom());
    }

    float contentW = maxX - minX;
    float contentH = maxY - minY;
    if (contentW <= 0 || contentH <= 0)
        return;

    constexpr float padding = 60.f;
    float viewW = static_cast<float> (getWidth()) - padding * 2.f;
    float viewH = static_cast<float> (getHeight()) - padding * 2.f;
    if (viewW <= 0 || viewH <= 0)
        return;

    float scaleX = viewW / contentW;
    float scaleY = viewH / contentH;
    zoomScale_ = juce::jlimit (0.25f, 3.0f, juce::jmin (scaleX, scaleY));

    float centreX = (minX + maxX) * 0.5f;
    float centreY = (minY + maxY) * 0.5f;
    viewOffset_.x = (static_cast<float> (getWidth()) * 0.5f / zoomScale_) - centreX;
    viewOffset_.y = (static_cast<float> (getHeight()) * 0.5f / zoomScale_) - centreY;

    rebuildFromModel();
}

void NodeEditorComponent::deleteSelectedNodes()
{
    for (auto& id : selectedNodes_)
        model_.removeNode (id);
    selectedNodes_.clear();
    inspector_.clearSelection();
}

void NodeEditorComponent::selectAllNodes()
{
    selectedNodes_.clear();
    for (auto& [id, nc] : nodeComponents_)
    {
        selectedNodes_.push_back (juce::String (id));
        nc->setSelected (true);
    }
    inspector_.clearSelection();
}

void NodeEditorComponent::duplicateSelectedNodes()
{
    if (selectedNodes_.empty())
        return;

    constexpr float kDuplicateOffset = 30.f;
    std::vector<juce::String> newSelection;

    for (auto& id : selectedNodes_)
    {
        auto nodeTree = model_.getNodeTree (id);
        if (! nodeTree.isValid()) continue;

        auto typeId = nodeTree[IDs::typeId].toString();
        float x = static_cast<float> (nodeTree[IDs::x]) + kDuplicateOffset;
        float y = static_cast<float> (nodeTree[IDs::y]) + kDuplicateOffset;

        auto newId = model_.addNode (typeId, x, y);

        // Copy params
        auto srcParams = model_.getParamsTree (id);
        auto dstParams = model_.getParamsTree (newId);
        if (srcParams.isValid() && dstParams.isValid())
        {
            for (int i = 0; i < srcParams.getNumProperties(); ++i)
            {
                auto name = srcParams.getPropertyName (i);
                dstParams.setProperty (name, srcParams[name], nullptr);
            }
        }

        newSelection.push_back (newId);
    }

    // Select duplicated nodes
    selectedNodes_.clear();
    for (auto& [_, nc] : nodeComponents_)
        nc->setSelected (false);

    for (auto& id : newSelection)
    {
        selectedNodes_.push_back (id);
        auto it = nodeComponents_.find (id.toStdString());
        if (it != nodeComponents_.end())
            it->second->setSelected (true);
    }
}

} // namespace pf
