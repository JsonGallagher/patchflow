#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/PortTypes.h"
#include "Graph/Connection.h"

namespace pf
{

class CableComponent : public juce::Component,
                        private juce::Timer
{
public:
    CableComponent (const Connection& connection, PortType type);

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;

    void setEndPoints (juce::Point<float> start, juce::Point<float> end);

    const Connection& getConnection() const { return connection_; }

private:
    void timerCallback() override;

    Connection connection_;
    PortType type_;
    juce::Point<float> startPoint_, endPoint_;
    juce::Path cablePath_;
    float flowPhase_ = 0.f;

    void updatePath();
};

} // namespace pf
