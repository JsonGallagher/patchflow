#pragma once
#include "Nodes/NodeBase.h"
#include <cmath>

namespace pf
{

class LFONode : public NodeBase
{
public:
    LFONode()
    {
        addInput  ("freq_mod", PortType::Visual);
        addInput  ("amp_mod",  PortType::Visual);
        addInput  ("sync",     PortType::Visual);
        addOutput ("output",   PortType::Visual);
        addOutput ("inverted", PortType::Visual);

        addParam ("waveform",  0, 0, 4, "Waveform", "LFO shape", "", "LFO",
                  juce::StringArray { "Sine", "Triangle", "Saw", "Square", "Random" });
        addParam ("frequency", 1.0f, 0.01f, 30.0f, "Frequency", "LFO speed", "Hz", "LFO");
        addParam ("amplitude", 1.0f, 0.0f, 1.0f, "Amplitude", "Output range", "", "LFO");
        addParam ("offset",    0.5f, 0.0f, 1.0f, "Offset", "Output DC offset", "", "LFO");
        addParam ("phase",     0.0f, 0.0f, 1.0f, "Phase", "Phase offset", "", "LFO");
    }

    juce::String getTypeId()      const override { return "LFO"; }
    juce::String getDisplayName() const override { return "LFO"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& /*gl*/) override
    {
        float freq = getParamAsFloat ("frequency", 1.0f);
        float amp = getParamAsFloat ("amplitude", 1.0f);
        float offset = getParamAsFloat ("offset", 0.5f);
        float phaseOffset = getParamAsFloat ("phase", 0.0f);
        int waveform = getParamAsInt ("waveform", 0);

        if (isInputConnected (0)) freq *= juce::jlimit (0.01f, 8.0f, getConnectedVisualValue (0) * 4.0f);
        if (isInputConnected (1)) amp *= juce::jlimit (0.0f, 2.0f, getConnectedVisualValue (1));

        // Sync: reset phase on rising edge
        if (isInputConnected (2))
        {
            float syncVal = getConnectedVisualValue (2);
            if (syncVal > 0.5f && lastSync_ <= 0.5f)
                phase_ = 0.0f;
            lastSync_ = syncVal;
        }

        phase_ += freq / 60.0f; // 60fps
        if (phase_ > 1.0f) phase_ -= std::floor (phase_);

        float p = std::fmod (phase_ + phaseOffset, 1.0f);
        float value = 0.0f;

        switch (waveform)
        {
            case 0: // Sine
                value = std::sin (p * 6.283185307f) * 0.5f + 0.5f;
                break;
            case 1: // Triangle
                value = 1.0f - std::abs (p * 2.0f - 1.0f);
                break;
            case 2: // Saw
                value = p;
                break;
            case 3: // Square
                value = p < 0.5f ? 1.0f : 0.0f;
                break;
            case 4: // Random (Sample & Hold)
            {
                float currentStep = std::floor (phase_ * freq);
                if (currentStep != lastRandomStep_)
                {
                    // Simple hash for random value
                    lastRandomValue_ = std::fmod (std::sin (currentStep * 12.9898f) * 43758.5453f, 1.0f);
                    if (lastRandomValue_ < 0.0f) lastRandomValue_ += 1.0f;
                    lastRandomStep_ = currentStep;
                }
                value = lastRandomValue_;
                break;
            }
        }

        float output = value * amp + offset - amp * 0.5f;
        output = juce::jlimit (0.0f, 1.0f, output);

        setVisualOutputValue (0, output);
        setVisualOutputValue (1, 1.0f - output);
    }

private:
    float phase_ = 0.0f;
    float lastSync_ = 0.0f;
    float lastRandomValue_ = 0.5f;
    float lastRandomStep_ = -1.0f;
};

} // namespace pf
