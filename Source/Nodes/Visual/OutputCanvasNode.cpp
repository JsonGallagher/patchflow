#include "Nodes/Visual/OutputCanvasNode.h"
#include <juce_opengl/juce_opengl.h>

namespace pf
{

void OutputCanvasNode::renderFrame (juce::OpenGLContext& /*gl*/)
{
    // OutputCanvas is the final stage — VisualCanvas reads getInputTexture()
    // and blits it to the viewport. No FBO work needed here.
}

} // namespace pf
