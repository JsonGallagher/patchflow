#include "Nodes/Visual/ChromaticAberrationNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void ChromaticAberrationNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512, height = 512;

    ShaderUtils::ensureFBO (gl, fbo_, fboTexture_, fboWidth_, fboHeight_, width, height);
    ShaderUtils::ensureQuadVBO (gl, quadVBO_);
    ShaderUtils::ensureFallbackTexture (fallbackTexture_);

    if (! shaderCompiled_ && ! shaderError_)
    {
        const auto vertSrc = ShaderUtils::getStandardVertexShader();
        const auto fragSrc = ShaderUtils::getFragmentPreamble() +
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_texture;\n"
            "uniform float u_amount;\n"
            "uniform float u_angle;\n"
            "uniform int   u_radial;\n"
            "\n"
            "void main() {\n"
            "    vec2 dir;\n"
            "    if (u_radial == 1) {\n"
            "        dir = (v_uv - 0.5) * u_amount;\n"
            "    } else {\n"
            "        dir = vec2(cos(u_angle), sin(u_angle)) * u_amount;\n"
            "    }\n"
            "\n"
            "    float r = texture2D(u_texture, v_uv + dir).r;\n"
            "    float g = texture2D(u_texture, v_uv).g;\n"
            "    float b = texture2D(u_texture, v_uv - dir).b;\n"
            "    float a = texture2D(u_texture, v_uv).a;\n"
            "\n"
            "    gl_FragColor = vec4(r, g, b, a);\n"
            "}\n";

        juce::String errorLog;
        auto vs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertSrc, errorLog);
        if (vs == 0) { shaderError_ = true; return; }

        auto fs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragSrc, errorLog);
        if (fs == 0) { gl.extensions.glDeleteShader (vs); shaderError_ = true; return; }

        shaderProgram_ = ShaderUtils::linkProgram (gl, vs, fs, errorLog);
        if (shaderProgram_ == 0) { shaderError_ = true; return; }

        shaderCompiled_ = true;
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    juce::gl::glViewport (0, 0, width, height);

    if (shaderError_ || shaderProgram_ == 0)
    {
        juce::gl::glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);
    }
    else
    {
        gl.extensions.glUseProgram (shaderProgram_);

        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (shaderProgram_, name);
        };

        float amount = getParamAsFloat ("amount", 0.005f);
        if (isInputConnected (1))
            amount *= juce::jlimit (0.0f, 10.0f, getConnectedVisualValue (1) * 5.0f);

        float angle = juce::degreesToRadians (getParamAsFloat ("angle", 0.0f));

        if (auto l = loc ("u_amount"); l >= 0) gl.extensions.glUniform1f (l, amount);
        if (auto l = loc ("u_angle");  l >= 0) gl.extensions.glUniform1f (l, angle);
        if (auto l = loc ("u_radial"); l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("radial", 0));

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D,
                                 isInputConnected (0) ? getConnectedTexture (0) : fallbackTexture_);
        if (auto l = loc ("u_texture"); l >= 0) gl.extensions.glUniform1i (l, 0);

        ShaderUtils::drawFullscreenQuad (gl, shaderProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
