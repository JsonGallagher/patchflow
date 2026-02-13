#include "Rendering/VisualCanvas.h"
#include "Nodes/Visual/WaveformRendererNode.h"
#include "Nodes/Visual/SpectrumRendererNode.h"
#include "Nodes/Visual/ShaderVisualNode.h"

namespace pf
{

VisualCanvas::VisualCanvas()
{
    glContext_.setRenderer (this);
    glContext_.setContinuousRepainting (true);
    glContext_.setComponentPaintingEnabled (false);
    glContext_.attachTo (*this);

    startTimerHz (60);
}

VisualCanvas::~VisualCanvas()
{
    stopTimer();
    glContext_.detach();
}

void VisualCanvas::newOpenGLContextCreated()
{
    compileBlitShader();
}

void VisualCanvas::renderOpenGL()
{
    auto startTick = juce::Time::getHighResolutionTicks();
    auto scale = static_cast<float> (glContext_.getRenderingScale());
    int width  = static_cast<int> (getWidth() * scale);
    int height = static_cast<int> (getHeight() * scale);

    juce::gl::glViewport (0, 0, width, height);
    juce::gl::glClearColor (0.05f, 0.05f, 0.08f, 1.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    if (runtimeGraph_)
    {
        // Update visual nodes with latest analysis data
        for (auto* node : runtimeGraph_->getVisualProcessOrder())
        {
            if (auto* wfn = dynamic_cast<WaveformRendererNode*> (node))
            {
                if (snapshot_.hasData)
                    wfn->updateWaveformSnapshot (snapshot_.latestFrame.waveform.data(),
                                                  snapshot_.latestFrame.waveformSize);
            }
            else if (auto* srn = dynamic_cast<SpectrumRendererNode*> (node))
            {
                if (snapshot_.hasData)
                    srn->updateMagnitudes (snapshot_.latestFrame.magnitudes.data(),
                                            snapshot_.latestFrame.numBins);
            }
            else if (auto* svn = dynamic_cast<ShaderVisualNode*> (node))
            {
                if (snapshot_.hasData)
                    svn->updateMagnitudes (snapshot_.latestFrame.magnitudes.data(),
                                            snapshot_.latestFrame.numBins);
            }
        }

        // Process visual nodes
        runtimeGraph_->processVisualFrame (glContext_);

        // Find OutputCanvas and blit its input texture
        for (auto* node : runtimeGraph_->getVisualProcessOrder())
        {
            if (auto* canvas = dynamic_cast<OutputCanvasNode*> (node))
            {
                auto tex = canvas->getInputTexture();
                if (tex != 0)
                {
                    juce::gl::glViewport (0, 0, width, height);
                    blitTextureToScreen (tex);
                }
                break;
            }
        }
    }

    auto endTick = juce::Time::getHighResolutionTicks();
    frameTime_ = static_cast<float> (juce::Time::highResolutionTicksToSeconds (endTick - startTick));
    frameCount_++;
}

void VisualCanvas::openGLContextClosing()
{
    if (blitProgram_ != 0)
    {
        glContext_.extensions.glDeleteProgram (blitProgram_);
        blitProgram_ = 0;
    }
    if (blitVBO_ != 0)
    {
        glContext_.extensions.glDeleteBuffers (1, &blitVBO_);
        blitVBO_ = 0;
    }
}

void VisualCanvas::timerCallback()
{
    if (analysisFifo_)
    {
        AnalysisFrame frame;
        if (analysisFifo_->popLatestFrame (frame))
        {
            snapshot_.latestFrame = frame;
            snapshot_.hasData = true;
        }
    }
}

void VisualCanvas::paint (juce::Graphics& g)
{
    // This is called for the JUCE paint layer — show FPS overlay
    g.setColour (juce::Colour (0x88000000));
    g.fillRect (4, 4, 80, 20);
    g.setColour (juce::Colours::white);
    g.setFont (11.f);
    g.drawText ("FPS: " + juce::String (frameTime_ > 0.f ? static_cast<int> (1.f / frameTime_) : 0),
                8, 4, 72, 20, juce::Justification::centredLeft);
}

void VisualCanvas::compileBlitShader()
{
    const char* vertSrc =
        "attribute vec2 a_position;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_position * 0.5 + 0.5;\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n";

    const char* fragSrc =
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_texture;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(u_texture, v_uv);\n"
        "}\n";

    auto vs = glContext_.extensions.glCreateShader (juce::gl::GL_VERTEX_SHADER);
    glContext_.extensions.glShaderSource (vs, 1, &vertSrc, nullptr);
    glContext_.extensions.glCompileShader (vs);

    auto fs = glContext_.extensions.glCreateShader (juce::gl::GL_FRAGMENT_SHADER);
    glContext_.extensions.glShaderSource (fs, 1, &fragSrc, nullptr);
    glContext_.extensions.glCompileShader (fs);

    blitProgram_ = glContext_.extensions.glCreateProgram();
    glContext_.extensions.glAttachShader (blitProgram_, vs);
    glContext_.extensions.glAttachShader (blitProgram_, fs);
    glContext_.extensions.glLinkProgram (blitProgram_);

    glContext_.extensions.glDeleteShader (vs);
    glContext_.extensions.glDeleteShader (fs);

    float quadVerts[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    glContext_.extensions.glGenBuffers (1, &blitVBO_);
    glContext_.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, blitVBO_);
    glContext_.extensions.glBufferData (juce::gl::GL_ARRAY_BUFFER, sizeof (quadVerts), quadVerts, juce::gl::GL_STATIC_DRAW);
    glContext_.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

void VisualCanvas::blitTextureToScreen (juce::uint32 texture)
{
    if (blitProgram_ == 0) return;

    glContext_.extensions.glUseProgram (blitProgram_);

    juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, texture);
    auto texLoc = glContext_.extensions.glGetUniformLocation (blitProgram_, "u_texture");
    glContext_.extensions.glUniform1i (texLoc, 0);

    glContext_.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, blitVBO_);
    auto posLoc = glContext_.extensions.glGetAttribLocation (blitProgram_, "a_position");
    glContext_.extensions.glEnableVertexAttribArray (posLoc);
    glContext_.extensions.glVertexAttribPointer (posLoc, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);

    juce::gl::glDrawArrays (juce::gl::GL_TRIANGLE_STRIP, 0, 4);

    glContext_.extensions.glDisableVertexAttribArray (posLoc);
    glContext_.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
    glContext_.extensions.glUseProgram (0);
}

} // namespace pf
