#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class ColorMapNode : public NodeBase
{
public:
    ColorMapNode()
    {
        addInput  ("value", PortType::Visual);
        // Output RGBA as 4 separate visual floats
        addOutput ("r", PortType::Visual);
        addOutput ("g", PortType::Visual);
        addOutput ("b", PortType::Visual);
        addOutput ("a", PortType::Visual);
        addParam  ("palette", 0, 0, 3); // 0=heat, 1=rainbow, 2=grayscale, 3=cool
    }

    juce::String getTypeId()      const override { return "ColorMap"; }
    juce::String getDisplayName() const override { return "Color Map"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& /*gl*/) override
    {
        float value = juce::jlimit (0.f, 1.f, getConnectedVisualValue (0));
        int palette = getParamAsInt ("palette", 0);

        float r = 0.f, g = 0.f, b = 0.f;

        switch (palette)
        {
            case 0: // Heat (black → red → yellow → white)
                r = juce::jlimit (0.f, 1.f, value * 3.f);
                g = juce::jlimit (0.f, 1.f, (value - 0.33f) * 3.f);
                b = juce::jlimit (0.f, 1.f, (value - 0.66f) * 3.f);
                break;
            case 1: // Rainbow (HSV sweep)
            {
                float h = value * 6.f;
                float x = 1.f - std::abs (std::fmod (h, 2.f) - 1.f);
                if      (h < 1) { r = 1; g = x; b = 0; }
                else if (h < 2) { r = x; g = 1; b = 0; }
                else if (h < 3) { r = 0; g = 1; b = x; }
                else if (h < 4) { r = 0; g = x; b = 1; }
                else if (h < 5) { r = x; g = 0; b = 1; }
                else             { r = 1; g = 0; b = x; }
                break;
            }
            case 2: // Grayscale
                r = g = b = value;
                break;
            case 3: // Cool (blue → cyan → white)
                r = juce::jlimit (0.f, 1.f, (value - 0.5f) * 2.f);
                g = juce::jlimit (0.f, 1.f, value * 2.f);
                b = 1.f;
                break;
        }

        setVisualOutputValue (0, r);
        setVisualOutputValue (1, g);
        setVisualOutputValue (2, b);
        setVisualOutputValue (3, 1.f);
    }
};

} // namespace pf
