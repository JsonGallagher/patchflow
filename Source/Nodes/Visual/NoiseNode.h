#pragma once
#include "Nodes/NodeBase.h"

namespace pf
{

class NoiseNode : public NodeBase
{
public:
    NoiseNode()
    {
        addInput  ("scale",    PortType::Visual);
        addInput  ("speed",    PortType::Visual);
        addInput  ("offset_x", PortType::Visual);
        addInput  ("offset_y", PortType::Visual);
        addOutput ("texture",  PortType::Texture);

        addParam ("noiseType",   0, 0, 3,   "Noise Type", "Type of noise function", "", "Noise",
                  juce::StringArray { "Perlin", "Simplex", "Worley", "Value" });
        addParam ("scale",       4.0f, 0.5f, 32.0f, "Scale", "Noise frequency scale", "", "Noise");
        addParam ("speed",       0.3f, 0.0f, 5.0f, "Speed", "Animation speed", "", "Noise");
        addParam ("octaves",     4, 1, 8, "Octaves", "FBM layers", "", "FBM");
        addParam ("lacunarity",  2.0f, 1.0f, 4.0f, "Lacunarity", "Frequency multiplier per octave", "x", "FBM");
        addParam ("persistence", 0.5f, 0.0f, 1.0f, "Persistence", "Amplitude falloff per octave", "", "FBM");
        addParam ("domainWarp",  0.0f, 0.0f, 2.0f, "Domain Warp", "Organic distortion amount", "", "Warp");
        addParam ("colorize",    0, 0, 1, "Colorize", "Output color mode", "", "Color",
                  juce::StringArray { "Grayscale", "Rainbow" });
    }

    juce::String getTypeId()      const override { return "Noise"; }
    juce::String getDisplayName() const override { return "Noise"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;

    juce::uint32 shaderProgram_ = 0;
    juce::uint32 quadVBO_ = 0;
    bool shaderCompiled_ = false;
    bool shaderError_ = false;
    float time_ = 0.f;
};

} // namespace pf
