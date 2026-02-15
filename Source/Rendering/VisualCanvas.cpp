#include "Rendering/VisualCanvas.h"
#include "UI/Theme.h"
#include "Nodes/Visual/WaveformRendererNode.h"
#include "Nodes/Visual/SpectrumRendererNode.h"
#include "Nodes/Visual/ShaderVisualNode.h"

namespace pf
{

VisualCanvas::VisualCanvas()
{
    glContext_.setRenderer (this);
    glContext_.setContinuousRepainting (true);
    glContext_.setComponentPaintingEnabled (true);
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
    compileNoSignalShader();
}

void VisualCanvas::renderOpenGL()
{
    auto startTick = juce::Time::getHighResolutionTicks();
    auto scale = static_cast<float> (glContext_.getRenderingScale());
    int width  = static_cast<int> (getWidth() * scale);
    int height = static_cast<int> (getHeight() * scale);

    if (width <= 0 || height <= 0)
        return;

    juce::gl::glViewport (0, 0, width, height);
    juce::gl::glClearColor (0.05f, 0.05f, 0.08f, 1.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    bool hasOutput = false;
    AnalysisSnapshot localSnapshot;
    {
        const juce::SpinLock::ScopedLockType lock (snapshotLock_);
        localSnapshot = snapshot_;
    }

    auto* graph = runtimeGraph_.load (std::memory_order_acquire);
    if (graph)
    {
        const auto& visualOrder = graph->getVisualProcessOrder();
        visualNodeCount_.store (static_cast<int> (visualOrder.size()), std::memory_order_release);

        // Update visual nodes with latest analysis data
        for (auto* node : visualOrder)
        {
            if (auto* wfn = dynamic_cast<WaveformRendererNode*> (node))
            {
                if (localSnapshot.hasData)
                    wfn->updateWaveformSnapshot (localSnapshot.latestFrame.waveform.data(),
                                                  localSnapshot.latestFrame.waveformSize);
            }
            else if (auto* srn = dynamic_cast<SpectrumRendererNode*> (node))
            {
                if (localSnapshot.hasData)
                    srn->updateMagnitudes (localSnapshot.latestFrame.magnitudes.data(),
                                            localSnapshot.latestFrame.numBins);
            }
            else if (auto* svn = dynamic_cast<ShaderVisualNode*> (node))
            {
                if (localSnapshot.hasData)
                    svn->updateMagnitudes (localSnapshot.latestFrame.magnitudes.data(),
                                            localSnapshot.latestFrame.numBins);
            }
        }

        // Process visual nodes
        graph->processVisualFrame (glContext_);

        // Find OutputCanvas and blit its input texture
        for (auto* node : visualOrder)
        {
            if (auto* canvas = dynamic_cast<OutputCanvasNode*> (node))
            {
                auto tex = canvas->getInputTexture();
                if (tex != 0)
                {
                    juce::gl::glViewport (0, 0, width, height);
                    blitTextureToScreen (tex);
                    hasOutput = true;
                }
                break;
            }
        }
    }
    else
    {
        visualNodeCount_.store (0, std::memory_order_release);
    }

    // If no graph or no output texture, show animated no-signal pattern
    if (! hasOutput)
        renderNoSignalPattern (width, height);

    auto endTick = juce::Time::getHighResolutionTicks();
    frameTime_.store (static_cast<float> (juce::Time::highResolutionTicksToSeconds (endTick - startTick)),
                      std::memory_order_release);
    frameCount_.fetch_add (1, std::memory_order_relaxed);
}

void VisualCanvas::openGLContextClosing()
{
    if (blitProgram_ != 0)
    {
        glContext_.extensions.glDeleteProgram (blitProgram_);
        blitProgram_ = 0;
    }
    if (noSignalProgram_ != 0)
    {
        glContext_.extensions.glDeleteProgram (noSignalProgram_);
        noSignalProgram_ = 0;
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
            const juce::SpinLock::ScopedLockType lock (snapshotLock_);
            snapshot_.latestFrame = frame;
            snapshot_.hasData = true;
        }
    }
    repaint();
}

void VisualCanvas::paint (juce::Graphics& g)
{
    // FPS pill overlay at bottom-left
    const auto frameTime = frameTime_.load (std::memory_order_acquire);
    const auto visualNodeCount = visualNodeCount_.load (std::memory_order_acquire);
    int fps = frameTime > 0.f ? static_cast<int> (1.f / frameTime) : 0;
    juce::String info = "FPS: " + juce::String (fps);
    if (visualNodeCount > 0)
        info += "  |  Nodes: " + juce::String (visualNodeCount);

    float textWidth = juce::Font (Theme::kFontGroupHeader).getStringWidthFloat (info) + 16.f;
    float pillWidth = juce::jmax (textWidth, 80.f);
    float pillHeight = 22.f;
    float pillX = 8.f;
    float pillY = static_cast<float> (getHeight()) - pillHeight - 8.f;

    g.setColour (juce::Colour (Theme::kBgPrimary).withAlpha (0.75f));
    g.fillRoundedRectangle (pillX, pillY, pillWidth, pillHeight, 11.f);
    g.setColour (juce::Colour (Theme::kBorderSubtle));
    g.drawRoundedRectangle (pillX, pillY, pillWidth, pillHeight, 11.f, 0.5f);

    g.setColour (juce::Colour (Theme::kTextSecondary));
    g.setFont (Theme::kFontGroupHeader);
    g.drawText (info, static_cast<int> (pillX), static_cast<int> (pillY),
                static_cast<int> (pillWidth), static_cast<int> (pillHeight),
                juce::Justification::centred);
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

void VisualCanvas::compileNoSignalShader()
{
    const char* vertSrc =
        "attribute vec2 a_position;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n";

    // Animated dot grid pattern
    const char* fragSrc =
        "uniform float u_time;\n"
        "uniform vec2 u_resolution;\n"
        "void main() {\n"
        "    vec2 uv = gl_FragCoord.xy / u_resolution;\n"
        "    vec2 p = uv * 2.0 - 1.0;\n"
        "    p.x *= u_resolution.x / u_resolution.y;\n"
        "    \n"
        "    // Animated dot grid\n"
        "    float gridSize = 0.15;\n"
        "    vec2 grid = mod(p + 0.5 * gridSize, gridSize) - 0.5 * gridSize;\n"
        "    float d = length(grid);\n"
        "    float pulse = sin(u_time * 1.5 + length(p) * 3.0) * 0.5 + 0.5;\n"
        "    float dot = smoothstep(0.025, 0.018, d) * (0.15 + pulse * 0.12);\n"
        "    \n"
        "    // Gentle radial vignette\n"
        "    float vig = 1.0 - length(p) * 0.4;\n"
        "    \n"
        "    vec3 col = vec3(0.25, 0.3, 0.5) * dot * vig;\n"
        "    col += vec3(0.03, 0.03, 0.06);\n"
        "    gl_FragColor = vec4(col, 1.0);\n"
        "}\n";

    auto vs = glContext_.extensions.glCreateShader (juce::gl::GL_VERTEX_SHADER);
    glContext_.extensions.glShaderSource (vs, 1, &vertSrc, nullptr);
    glContext_.extensions.glCompileShader (vs);

    auto fs = glContext_.extensions.glCreateShader (juce::gl::GL_FRAGMENT_SHADER);
    glContext_.extensions.glShaderSource (fs, 1, &fragSrc, nullptr);
    glContext_.extensions.glCompileShader (fs);

    noSignalProgram_ = glContext_.extensions.glCreateProgram();
    glContext_.extensions.glAttachShader (noSignalProgram_, vs);
    glContext_.extensions.glAttachShader (noSignalProgram_, fs);
    glContext_.extensions.glLinkProgram (noSignalProgram_);

    glContext_.extensions.glDeleteShader (vs);
    glContext_.extensions.glDeleteShader (fs);
}

void VisualCanvas::renderNoSignalPattern (int width, int height)
{
    if (noSignalProgram_ == 0 || blitVBO_ == 0) return;

    noSignalTime_ += 1.f / 60.f;

    glContext_.extensions.glUseProgram (noSignalProgram_);
    glContext_.extensions.glUniform1f (
        glContext_.extensions.glGetUniformLocation (noSignalProgram_, "u_time"), noSignalTime_);
    glContext_.extensions.glUniform2f (
        glContext_.extensions.glGetUniformLocation (noSignalProgram_, "u_resolution"),
        static_cast<float> (width), static_cast<float> (height));

    glContext_.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, blitVBO_);
    auto posLoc = glContext_.extensions.glGetAttribLocation (noSignalProgram_, "a_position");
    glContext_.extensions.glEnableVertexAttribArray (posLoc);
    glContext_.extensions.glVertexAttribPointer (posLoc, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);

    juce::gl::glDrawArrays (juce::gl::GL_TRIANGLE_STRIP, 0, 4);

    glContext_.extensions.glDisableVertexAttribArray (posLoc);
    glContext_.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
    glContext_.extensions.glUseProgram (0);
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
