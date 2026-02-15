#pragma once
#include "Nodes/NodeBase.h"
#include <cmath>

namespace pf
{

class StepSequencerNode : public NodeBase
{
public:
    StepSequencerNode()
    {
        addInput  ("clock", PortType::Visual);
        addInput  ("reset", PortType::Visual);
        addOutput ("value",      PortType::Visual);
        addOutput ("gate",       PortType::Visual);
        addOutput ("step_index", PortType::Visual);

        addParam ("steps",  8, 2, 16, "Steps", "Number of sequence steps", "", "Sequencer");
        addParam ("step1",  1.0f, 0.0f, 1.0f, "Step 1", "", "", "Values");
        addParam ("step2",  0.5f, 0.0f, 1.0f, "Step 2", "", "", "Values");
        addParam ("step3",  0.75f, 0.0f, 1.0f, "Step 3", "", "", "Values");
        addParam ("step4",  0.25f, 0.0f, 1.0f, "Step 4", "", "", "Values");
        addParam ("step5",  0.8f, 0.0f, 1.0f, "Step 5", "", "", "Values");
        addParam ("step6",  0.3f, 0.0f, 1.0f, "Step 6", "", "", "Values");
        addParam ("step7",  0.6f, 0.0f, 1.0f, "Step 7", "", "", "Values");
        addParam ("step8",  0.1f, 0.0f, 1.0f, "Step 8", "", "", "Values");
        addParam ("mode",   0, 0, 2, "Mode", "Playback direction", "", "Sequencer",
                  juce::StringArray { "Forward", "Ping-Pong", "Random" });
        addParam ("speed",  2.0f, 0.1f, 30.0f, "Speed", "Internal clock speed", "Hz", "Sequencer");
    }

    juce::String getTypeId()      const override { return "StepSequencer"; }
    juce::String getDisplayName() const override { return "Step Sequencer"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& /*gl*/) override
    {
        int numSteps = getParamAsInt ("steps", 8);
        int mode = getParamAsInt ("mode", 0);
        numSteps = juce::jlimit (2, 16, numSteps);

        // Reset on rising edge
        if (isInputConnected (1))
        {
            float resetVal = getConnectedVisualValue (1);
            if (resetVal > 0.5f && lastReset_ <= 0.5f)
            {
                currentStep_ = 0;
                direction_ = 1;
            }
            lastReset_ = resetVal;
        }

        // Clock: external or internal
        bool advance = false;
        if (isInputConnected (0))
        {
            float clockVal = getConnectedVisualValue (0);
            if (clockVal > 0.5f && lastClock_ <= 0.5f)
                advance = true;
            lastClock_ = clockVal;
        }
        else
        {
            // Internal clock
            float speed = getParamAsFloat ("speed", 2.0f);
            internalPhase_ += speed / 60.0f;
            if (internalPhase_ >= 1.0f)
            {
                internalPhase_ -= std::floor (internalPhase_);
                advance = true;
            }
        }

        if (advance)
        {
            switch (mode)
            {
                case 0: // Forward
                    currentStep_ = (currentStep_ + 1) % numSteps;
                    break;
                case 1: // Ping-Pong
                    currentStep_ += direction_;
                    if (currentStep_ >= numSteps - 1) { currentStep_ = numSteps - 1; direction_ = -1; }
                    if (currentStep_ <= 0) { currentStep_ = 0; direction_ = 1; }
                    break;
                case 2: // Random
                {
                    randomSeed_ = (randomSeed_ * 1103515245 + 12345) & 0x7fffffff;
                    currentStep_ = randomSeed_ % numSteps;
                    break;
                }
            }
            gateDecay_ = 1.0f;
        }
        else
        {
            gateDecay_ *= 0.85f;
        }

        // Get step value
        static const char* stepNames[] = { "step1","step2","step3","step4","step5","step6","step7","step8" };
        float value = 0.5f;
        if (currentStep_ < 8)
            value = getParamAsFloat (stepNames[currentStep_], 0.5f);

        setVisualOutputValue (0, value);
        setVisualOutputValue (1, gateDecay_);
        setVisualOutputValue (2, static_cast<float> (currentStep_) / static_cast<float> (juce::jmax (1, numSteps - 1)));
    }

private:
    int currentStep_ = 0;
    int direction_ = 1;
    float lastClock_ = 0.0f;
    float lastReset_ = 0.0f;
    float internalPhase_ = 0.0f;
    float gateDecay_ = 0.0f;
    int randomSeed_ = 42;
};

} // namespace pf
