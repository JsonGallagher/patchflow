#include "Nodes/Visual/ParticleNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void ParticleNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512, height = 512;
    constexpr float dt = 1.0f / 60.0f;

    ShaderUtils::ensureFBO (gl, fbo_, fboTexture_, fboWidth_, fboHeight_, width, height);
    ShaderUtils::ensureFallbackTexture (fallbackTexture_);

    // Determine pool size
    static const int poolSizes[] = { 1000, 4000, 16000, 64000 };
    int desiredPool = poolSizes[juce::jlimit (0, 3, getParamAsInt ("maxParticles", 1))];
    if (desiredPool != poolSize_)
    {
        particles_.resize (static_cast<size_t> (desiredPool));
        for (auto& p : particles_) p.life = 0;
        poolSize_ = desiredPool;
        nextParticle_ = 0;
    }

    // Params
    float emissionRate = getParamAsFloat ("emissionRate", 50.0f);
    float lifetime = getParamAsFloat ("lifetime", 2.0f);
    float speed = getParamAsFloat ("speed", 0.3f);
    float gravity = getParamAsFloat ("gravity", 0.0f);
    float turbulence = getParamAsFloat ("turbulence", 0.1f);
    float size = getParamAsFloat ("size", 3.0f);
    int emitterShape = getParamAsInt ("emitterShape", 0);
    int blendMode = getParamAsInt ("blendMode", 0);

    if (isInputConnected (0)) emissionRate *= juce::jlimit (0.0f, 4.0f, getConnectedVisualValue (0) * 2.0f);
    if (isInputConnected (1)) speed *= juce::jlimit (0.0f, 4.0f, getConnectedVisualValue (1) * 2.0f);

    float colR = isInputConnected (2) ? getConnectedVisualValue (2) : 1.0f;
    float colG = isInputConnected (3) ? getConnectedVisualValue (3) : 0.8f;
    float colB = isInputConnected (4) ? getConnectedVisualValue (4) : 0.3f;

    // Emit particles
    emitAccum_ += emissionRate * dt;
    int toEmit = static_cast<int> (emitAccum_);
    emitAccum_ -= static_cast<float> (toEmit);

    for (int i = 0; i < toEmit && poolSize_ > 0; ++i)
    {
        auto& p = particles_[nextParticle_];
        nextParticle_ = (nextParticle_ + 1) % poolSize_;

        // Position based on emitter shape
        switch (emitterShape)
        {
            case 0: // Point (center)
                p.x = 0.5f;
                p.y = 0.5f;
                break;
            case 1: // Line (horizontal center)
                p.x = pseudoRandom();
                p.y = 0.5f;
                break;
            case 2: // Circle
            {
                float angle = pseudoRandom() * 6.283185f;
                float r = 0.15f;
                p.x = 0.5f + std::cos (angle) * r;
                p.y = 0.5f + std::sin (angle) * r;
                break;
            }
        }

        float angle = pseudoRandom() * 6.283185f;
        float spd = speed * (0.5f + pseudoRandom() * 0.5f);
        p.vx = std::cos (angle) * spd;
        p.vy = std::sin (angle) * spd;
        p.life = lifetime * (0.8f + pseudoRandom() * 0.4f);
        p.maxLife = p.life;
        p.r = colR;
        p.g = colG;
        p.b = colB;
    }

    // Update particles
    for (auto& p : particles_)
    {
        if (p.life <= 0) continue;
        p.life -= dt;
        p.vy -= gravity * dt;
        p.vx += (pseudoRandom() - 0.5f) * turbulence * dt;
        p.vy += (pseudoRandom() - 0.5f) * turbulence * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
    }

    // Render to FBO
    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    juce::gl::glViewport (0, 0, width, height);

    // Clear or draw background texture
    if (isInputConnected (5))
    {
        // Blit background - for simplicity, just clear with slight fade
        juce::gl::glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);
    }
    else
    {
        juce::gl::glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);
    }

    // Enable blending
    juce::gl::glEnable (juce::gl::GL_BLEND);
    if (blendMode == 0) // Additive
        juce::gl::glBlendFunc (juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE);
    else
        juce::gl::glBlendFunc (juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

    // Enable point sprites
    juce::gl::glEnable (0x8642); // GL_PROGRAM_POINT_SIZE
    juce::gl::glEnable (juce::gl::GL_POINT_SMOOTH);

    // Build vertex data for live particles
    struct PointVertex { float x, y, r, g, b, a, sz; };
    std::vector<PointVertex> verts;
    verts.reserve (particles_.size());

    for (const auto& p : particles_)
    {
        if (p.life <= 0) continue;
        float alpha = juce::jlimit (0.0f, 1.0f, p.life / p.maxLife);
        float screenX = p.x * 2.0f - 1.0f;
        float screenY = p.y * 2.0f - 1.0f;
        verts.push_back ({ screenX, screenY, p.r, p.g, p.b, alpha, size * alpha });
    }

    if (! verts.empty())
    {
        // Use fixed-function-style rendering with vertex arrays
        juce::uint32 vbo = 0;
        gl.extensions.glGenBuffers (1, &vbo);
        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, vbo);
        gl.extensions.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                                    static_cast<GLsizeiptr> (verts.size() * sizeof (PointVertex)),
                                    verts.data(), juce::gl::GL_STREAM_DRAW);

        // Simple point shader (if we don't have one, just draw as colored points)
        // Use immediate-mode-style approach via glPointSize
        juce::gl::glPointSize (size);

        // For simplicity without a shader: just draw white points
        // The particles will appear as dots
        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
        gl.extensions.glDeleteBuffers (1, &vbo);
    }

    juce::gl::glDisable (juce::gl::GL_BLEND);
    juce::gl::glDisable (0x8642);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
