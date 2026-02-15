#pragma once
#include <juce_opengl/juce_opengl.h>

namespace pf
{

namespace ShaderUtils
{
    juce::String getShaderInfoLog (juce::OpenGLContext& gl, juce::uint32 shader);
    juce::String getProgramInfoLog (juce::OpenGLContext& gl, juce::uint32 program);

    juce::uint32 compileShaderStage (juce::OpenGLContext& gl,
                                     GLenum stage,
                                     const juce::String& source,
                                     juce::String& outErrorLog);

    juce::uint32 linkProgram (juce::OpenGLContext& gl,
                              juce::uint32 vs,
                              juce::uint32 fs,
                              juce::String& outErrorLog);

    void ensureFBO (juce::OpenGLContext& gl,
                    juce::uint32& fbo, juce::uint32& fboTexture,
                    int& fboWidth, int& fboHeight,
                    int width, int height);

    void ensurePingPongFBOs (juce::OpenGLContext& gl,
                             juce::uint32 fbos[2], juce::uint32 textures[2],
                             int& fboWidth, int& fboHeight,
                             int width, int height);

    void ensureQuadVBO (juce::OpenGLContext& gl, juce::uint32& quadVBO);

    void ensureFallbackTexture (juce::uint32& fallbackTexture);

    void drawFullscreenQuad (juce::OpenGLContext& gl,
                             juce::uint32 shaderProgram,
                             juce::uint32 quadVBO);

    juce::String getStandardVertexShader();
    juce::String getFragmentPreamble();

} // namespace ShaderUtils

} // namespace pf
