#include "Nodes/Visual/SpectrumRendererNode.h"
#include <juce_opengl/juce_opengl.h>
#include <cmath>

namespace pf
{

void SpectrumRendererNode::ensureFBO (juce::OpenGLContext& gl, int width, int height)
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

void SpectrumRendererNode::renderFrame (juce::OpenGLContext& gl)
{
    int width = 512, height = 256;
    ensureFBO (gl, width, height);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    juce::gl::glViewport (0, 0, width, height);

    juce::gl::glClearColor (0.05f, 0.05f, 0.1f, 1.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    float r = isInputConnected (1) ? getConnectedVisualValue (1) : 0.2f;
    float g = isInputConnected (2) ? getConnectedVisualValue (2) : 0.9f;
    float b = isInputConnected (3) ? getConnectedVisualValue (3) : 0.4f;

    if (! magnitudeSnapshot_.empty())
    {
        int numBins = static_cast<int> (magnitudeSnapshot_.size());
        int numBars = juce::jmin (numBins, 128);

        juce::gl::glBegin (juce::gl::GL_QUADS);

        for (int i = 0; i < numBars; ++i)
        {
            // Map bar index to bin (log or linear)
            int bin = i * numBins / numBars;
            float mag = magnitudeSnapshot_[bin];

            // Convert to dB-ish scale for visual range
            float dB = (mag > 1e-6f) ? 20.0f * std::log10 (mag) : -90.f;
            float dbRange = getParamAsFloat ("dbRange", -60.f);
            float normalized = juce::jlimit (0.f, 1.f, 1.f - dB / dbRange);

            float x0 = (static_cast<float> (i) / numBars) * 2.0f - 1.0f;
            float x1 = (static_cast<float> (i + 1) / numBars) * 2.0f - 1.0f;
            float y0 = -1.0f;
            float y1 = -1.0f + normalized * 2.0f;

            float intensity = 0.3f + 0.7f * normalized;
            juce::gl::glColor4f (r * intensity, g * intensity, b * intensity, 1.0f);

            juce::gl::glVertex2f (x0, y0);
            juce::gl::glVertex2f (x1, y0);
            juce::gl::glVertex2f (x1, y1);
            juce::gl::glVertex2f (x0, y1);
        }

        juce::gl::glEnd();
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
