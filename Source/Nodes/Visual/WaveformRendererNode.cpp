#include "Nodes/Visual/WaveformRendererNode.h"
#include <juce_opengl/juce_opengl.h>

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

    // Clear to dark background
    juce::gl::glClearColor (0.05f, 0.05f, 0.1f, 1.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    // Get color from inputs or defaults
    float r = isInputConnected (1) ? getConnectedVisualValue (1) : 0.3f;
    float g = isInputConnected (2) ? getConnectedVisualValue (2) : 0.8f;
    float b = isInputConnected (3) ? getConnectedVisualValue (3) : 1.0f;

    if (! waveformSnapshot_.empty())
    {
        juce::gl::glColor4f (r, g, b, 1.0f);
        juce::gl::glLineWidth (getParamAsFloat ("lineThickness", 2.0f));
        juce::gl::glBegin (juce::gl::GL_LINE_STRIP);

        int numSamples = static_cast<int> (waveformSnapshot_.size());
        for (int i = 0; i < numSamples; ++i)
        {
            float x = (static_cast<float> (i) / static_cast<float> (numSamples - 1)) * 2.0f - 1.0f;
            float y = waveformSnapshot_[i];
            juce::gl::glVertex2f (x, y);
        }

        juce::gl::glEnd();
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
