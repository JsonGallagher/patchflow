#pragma once
#include <juce_opengl/juce_opengl.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Graph/RuntimeGraph.h"
#include "Audio/AnalysisSnapshot.h"
#include "Nodes/Visual/OutputCanvasNode.h"

namespace pf
{

/**
 * OpenGL-rendered visual output canvas.
 * Processes visual nodes each frame, composites to screen.
 */
class VisualCanvas : public juce::Component,
                      public juce::OpenGLRenderer,
                      public juce::Timer
{
public:
    VisualCanvas();
    ~VisualCanvas() override;

    //==============================================================================
    // OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    //==============================================================================
    // Timer — consumes analysis FIFO on GUI thread
    void timerCallback() override;

    void paint (juce::Graphics& g) override;

    //==============================================================================
    void setRuntimeGraph (RuntimeGraph* graph) { runtimeGraph_ = graph; }
    void setAnalysisFIFO (AnalysisFIFO* fifo) { analysisFifo_ = fifo; }

private:
    juce::OpenGLContext glContext_;
    RuntimeGraph* runtimeGraph_ = nullptr;
    AnalysisFIFO* analysisFifo_ = nullptr;
    AnalysisSnapshot snapshot_;

    // Blit shader for final output
    juce::uint32 blitProgram_ = 0;
    juce::uint32 blitVBO_ = 0;

    // No-signal animated pattern shader
    juce::uint32 noSignalProgram_ = 0;
    float noSignalTime_ = 0.f;

    void compileBlitShader();
    void compileNoSignalShader();
    void renderNoSignalPattern (int width, int height);
    void blitTextureToScreen (juce::uint32 texture);

    float frameTime_ = 0.f;
    int frameCount_ = 0;
    int visualNodeCount_ = 0;
};

} // namespace pf
