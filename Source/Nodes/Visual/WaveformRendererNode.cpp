#include "Nodes/Visual/WaveformRendererNode.h"
#include <juce_opengl/juce_opengl.h>
#include <cmath>

namespace pf
{

void WaveformRendererNode::ensureFBO (juce::OpenGLContext& gl, int width, int height)
{
    if (fbo_ != 0 && fboWidth_ == width && fboHeight_ == height)
        return;

    if (fbo_ != 0)
    {
        gl.extensions.glDeleteFramebuffers (1, &fbo_);
        juce::gl::glDeleteTextures (1, &fboTexture_);
    }

    juce::gl::glGenTextures (1, &fboTexture_);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, fboTexture_);
    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                            width, height, 0,
                            juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE, nullptr);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);

    gl.extensions.glGenFramebuffers (1, &fbo_);
    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER, juce::gl::GL_COLOR_ATTACHMENT0,
                                          juce::gl::GL_TEXTURE_2D, fboTexture_, 0);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    fboWidth_ = width;
    fboHeight_ = height;
}

void WaveformRendererNode::renderFrame (juce::OpenGLContext& gl)
{
    int width = 512, height = 256;
    ensureFBO (gl, width, height);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    juce::gl::glViewport (0, 0, width, height);

    juce::gl::glClearColor (0.025f, 0.03f, 0.075f, 1.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    // Soft top-to-bottom gradient.
    juce::gl::glBegin (juce::gl::GL_QUADS);
    juce::gl::glColor4f (0.012f, 0.014f, 0.04f, 1.0f);
    juce::gl::glVertex2f (-1.0f, -1.0f);
    juce::gl::glVertex2f ( 1.0f, -1.0f);
    juce::gl::glColor4f (0.04f, 0.055f, 0.11f, 1.0f);
    juce::gl::glVertex2f ( 1.0f,  1.0f);
    juce::gl::glVertex2f (-1.0f,  1.0f);
    juce::gl::glEnd();

    const auto r = juce::jlimit (0.0f, 1.2f, isInputConnected (1) ? getConnectedVisualValue (1) : 0.3f);
    const auto g = juce::jlimit (0.0f, 1.2f, isInputConnected (2) ? getConnectedVisualValue (2) : 0.8f);
    const auto b = juce::jlimit (0.0f, 1.2f, isInputConnected (3) ? getConnectedVisualValue (3) : 1.0f);

    if (! waveformSnapshot_.empty() && waveformSnapshot_.size() > 1)
    {
        const auto numSamples = static_cast<int> (waveformSnapshot_.size());
        if (static_cast<int> (smoothedWaveform_.size()) != numSamples)
            smoothedWaveform_.assign (static_cast<size_t> (numSamples), 0.0f);

        float rms = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto target = juce::jlimit (-1.0f, 1.0f, waveformSnapshot_[static_cast<size_t> (i)]);
            const auto previous = smoothedWaveform_[static_cast<size_t> (i)];
            const auto smoothed = previous + (target - previous) * 0.34f;
            smoothedWaveform_[static_cast<size_t> (i)] = smoothed;
            rms += smoothed * smoothed;
        }
        rms = std::sqrt (rms / static_cast<float> (numSamples));
        rmsLevel_ = rmsLevel_ * 0.88f + rms * 0.12f;

        auto thickness = getParamAsFloat ("lineThickness", 2.0f);
        if (isInputConnected (4))
        {
            const auto thicknessMod = juce::jlimit (0.0f, 2.0f, getConnectedVisualValue (4));
            thickness *= 0.8f + thicknessMod * 1.35f;
        }
        thickness *= 1.0f + juce::jlimit (0.0f, 1.2f, rmsLevel_ * 2.2f) * 0.35f;
        thickness = juce::jlimit (1.0f, 6.5f, thickness);

        const auto style = getParamAsInt ("style", 0);
        const auto glow = juce::jlimit (0.0f, 1.0f, rmsLevel_ * 4.0f);

        juce::gl::glEnable (juce::gl::GL_BLEND);
        juce::gl::glBlendFunc (juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

        // Pulse band around the centre line.
        const auto bandHalfHeight = 0.06f + glow * 0.28f;
        juce::gl::glBegin (juce::gl::GL_QUADS);
        juce::gl::glColor4f (r * 0.15f, g * 0.18f, b * 0.2f, 0.0f);
        juce::gl::glVertex2f (-1.0f, -bandHalfHeight);
        juce::gl::glVertex2f ( 1.0f, -bandHalfHeight);
        juce::gl::glColor4f (r * 0.35f, g * 0.4f, b * 0.45f, 0.22f + glow * 0.18f);
        juce::gl::glVertex2f ( 1.0f,  bandHalfHeight);
        juce::gl::glVertex2f (-1.0f,  bandHalfHeight);
        juce::gl::glEnd();

        if (style == 1)
        {
            juce::gl::glBegin (juce::gl::GL_TRIANGLE_STRIP);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (numSamples - 1);
                const auto x = t * 2.0f - 1.0f;
                const auto y = smoothedWaveform_[static_cast<size_t> (i)] * 0.9f;

                juce::gl::glColor4f (r * 0.25f, g * 0.28f, b * 0.35f, 0.08f);
                juce::gl::glVertex2f (x, 0.0f);
                juce::gl::glColor4f (r * 0.95f, g * 0.98f, b, 0.58f);
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();
        }
        else if (style == 2)
        {
            juce::gl::glBegin (juce::gl::GL_TRIANGLE_STRIP);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (numSamples - 1);
                const auto x = t * 2.0f - 1.0f;
                const auto y = std::abs (smoothedWaveform_[static_cast<size_t> (i)]) * 0.9f;

                juce::gl::glColor4f (r * 0.2f, g * 0.22f, b * 0.3f, 0.1f);
                juce::gl::glVertex2f (x, -y);
                juce::gl::glColor4f (r * 0.95f, g, b, 0.5f);
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();
        }

        // Glow pass.
        juce::gl::glLineWidth (thickness + 2.3f);
        juce::gl::glColor4f (r, g, b, 0.25f + glow * 0.25f);
        juce::gl::glBegin (juce::gl::GL_LINE_STRIP);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto x = (static_cast<float> (i) / static_cast<float> (numSamples - 1)) * 2.0f - 1.0f;
            const auto y = smoothedWaveform_[static_cast<size_t> (i)] * 0.9f;
            juce::gl::glVertex2f (x, y);
        }
        juce::gl::glEnd();

        if (style == 2)
        {
            juce::gl::glLineWidth (thickness + 1.0f);
            juce::gl::glColor4f (r, g, b, 0.22f + glow * 0.2f);
            juce::gl::glBegin (juce::gl::GL_LINE_STRIP);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto x = (static_cast<float> (i) / static_cast<float> (numSamples - 1)) * 2.0f - 1.0f;
                const auto y = -std::abs (smoothedWaveform_[static_cast<size_t> (i)]) * 0.9f;
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();
        }

        // Core line pass.
        juce::gl::glLineWidth (thickness);
        juce::gl::glColor4f (juce::jlimit (0.0f, 1.0f, r + 0.06f),
                             juce::jlimit (0.0f, 1.0f, g + 0.06f),
                             juce::jlimit (0.0f, 1.0f, b + 0.06f),
                             0.98f);
        juce::gl::glBegin (juce::gl::GL_LINE_STRIP);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto x = (static_cast<float> (i) / static_cast<float> (numSamples - 1)) * 2.0f - 1.0f;
            const auto y = smoothedWaveform_[static_cast<size_t> (i)] * 0.9f;
            juce::gl::glVertex2f (x, y);
        }
        juce::gl::glEnd();

        if (style == 2)
        {
            juce::gl::glBegin (juce::gl::GL_LINE_STRIP);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto x = (static_cast<float> (i) / static_cast<float> (numSamples - 1)) * 2.0f - 1.0f;
                const auto y = -std::abs (smoothedWaveform_[static_cast<size_t> (i)]) * 0.9f;
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();
        }

        juce::gl::glDisable (juce::gl::GL_BLEND);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
