#pragma once
#include "Nodes/NodeBase.h"
#include <cmath>

namespace pf
{

class TriggerNode : public NodeBase
{
public:
    TriggerNode()
    {
        addInput  ("trigger",    PortType::Visual);
        addInput  ("attack_mod", PortType::Visual);
        addOutput ("output",     PortType::Visual);

        addParam ("attackMs",  5.0f, 0.0f, 500.0f, "Attack", "Attack time", "ms", "Envelope");
        addParam ("decayMs",   200.0f, 10.0f, 5000.0f, "Decay", "Decay time", "ms", "Envelope");
        addParam ("shape",     0, 0, 2, "Shape", "Envelope curve", "", "Envelope",
                  juce::StringArray { "Exponential", "Linear", "Logarithmic" });
    }

    juce::String getTypeId()      const override { return "Trigger"; }
    juce::String getDisplayName() const override { return "Trigger"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& /*gl*/) override
    {
        float triggerVal = isInputConnected (0) ? getConnectedVisualValue (0) : 0.0f;

        // Rising edge detection
        if (triggerVal > 0.5f && lastTrigger_ <= 0.5f)
        {
            triggered_ = true;
            envelopePhase_ = 0.0f; // Start attack
        }
        lastTrigger_ = triggerVal;

        float attackMs = getParamAsFloat ("attackMs", 5.0f);
        float decayMs = getParamAsFloat ("decayMs", 200.0f);
        int shape = getParamAsInt ("shape", 0);

        if (isInputConnected (1))
            attackMs *= juce::jlimit (0.1f, 4.0f, getConnectedVisualValue (1) * 2.0f);

        constexpr float frameDt = 1000.0f / 60.0f; // ms per frame at 60fps

        float output = 0.0f;

        if (triggered_)
        {
            if (envelopePhase_ < attackMs)
            {
                // Attack phase
                float t = (attackMs > 0.0f) ? envelopePhase_ / attackMs : 1.0f;
                output = juce::jlimit (0.0f, 1.0f, t);
                envelopePhase_ += frameDt;
            }
            else
            {
                // Decay phase
                float decayElapsed = envelopePhase_ - attackMs;
                float t = (decayMs > 0.0f) ? decayElapsed / decayMs : 1.0f;

                switch (shape)
                {
                    case 0: // Exponential
                        output = std::exp (-t * 4.0f);
                        break;
                    case 1: // Linear
                        output = juce::jmax (0.0f, 1.0f - t);
                        break;
                    case 2: // Logarithmic
                        output = juce::jmax (0.0f, 1.0f - std::log1p (t * 2.718f));
                        break;
                }

                envelopePhase_ += frameDt;

                if (t >= 1.0f)
                {
                    output = 0.0f;
                    triggered_ = false;
                }
            }
        }

        setVisualOutputValue (0, output);
    }

private:
    float lastTrigger_ = 0.0f;
    bool triggered_ = false;
    float envelopePhase_ = 0.0f; // in milliseconds
};

} // namespace pf
