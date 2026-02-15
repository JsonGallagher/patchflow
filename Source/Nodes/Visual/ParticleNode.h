#pragma once
#include "Nodes/NodeBase.h"
#include <vector>
#include <cmath>

namespace pf
{

class ParticleNode : public NodeBase
{
public:
    ParticleNode()
    {
        addInput  ("emission_rate", PortType::Visual);
        addInput  ("velocity",      PortType::Visual);
        addInput  ("color_r",       PortType::Visual);
        addInput  ("color_g",       PortType::Visual);
        addInput  ("color_b",       PortType::Visual);
        addInput  ("bg_texture",    PortType::Texture);
        addOutput ("texture",       PortType::Texture);

        addParam ("maxParticles", 1, 0, 3, "Max Particles", "Particle pool size", "", "Particles",
                  juce::StringArray { "1K", "4K", "16K", "64K" });
        addParam ("emissionRate", 50.0f, 0.0f, 500.0f, "Emission Rate", "Particles per second", "/s", "Particles");
        addParam ("lifetime",     2.0f, 0.1f, 10.0f, "Lifetime", "Particle lifespan", "s", "Particles");
        addParam ("speed",        0.3f, 0.0f, 2.0f, "Speed", "Initial velocity", "", "Physics");
        addParam ("gravity",      0.0f, -1.0f, 1.0f, "Gravity", "Vertical force", "", "Physics");
        addParam ("turbulence",   0.1f, 0.0f, 1.0f, "Turbulence", "Random force", "", "Physics");
        addParam ("size",         3.0f, 1.0f, 20.0f, "Size", "Particle size", "px", "Rendering");
        addParam ("emitterShape", 0, 0, 2, "Emitter", "Emitter shape", "", "Emitter",
                  juce::StringArray { "Point", "Line", "Circle" });
        addParam ("blendMode",    0, 0, 1, "Blend", "Particle blending", "", "Rendering",
                  juce::StringArray { "Additive", "Alpha" });
    }

    juce::String getTypeId()      const override { return "Particle"; }
    juce::String getDisplayName() const override { return "Particle"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

private:
    struct Particle
    {
        float x = 0, y = 0;
        float vx = 0, vy = 0;
        float life = 0, maxLife = 1;
        float r = 1, g = 1, b = 1;
    };

    std::vector<Particle> particles_;
    float emitAccum_ = 0.0f;
    int nextParticle_ = 0;
    int poolSize_ = 0;

    juce::uint32 fbo_ = 0, fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;
    juce::uint32 fallbackTexture_ = 0;

    // Simple hash-based random
    float pseudoRandom()
    {
        rngState_ = rngState_ * 1103515245u + 12345u;
        return static_cast<float> (rngState_ & 0x7FFFu) / 32767.0f;
    }
    unsigned int rngState_ = 42;
};

} // namespace pf
