#include "Nodes/Visual/MirrorNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void MirrorNode::renderFrame (juce::OpenGLContext& gl)
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
            "uniform int   u_mode;\n"
            "uniform float u_offset;\n"
            "uniform int   u_segments;\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv;\n"
            "\n"
            "    if (u_mode == 0) { // Horizontal\n"
            "        if (uv.x > u_offset) uv.x = 2.0 * u_offset - uv.x;\n"
            "    } else if (u_mode == 1) { // Vertical\n"
            "        if (uv.y > u_offset) uv.y = 2.0 * u_offset - uv.y;\n"
            "    } else if (u_mode == 2) { // Quad\n"
            "        if (uv.x > u_offset) uv.x = 2.0 * u_offset - uv.x;\n"
            "        if (uv.y > u_offset) uv.y = 2.0 * u_offset - uv.y;\n"
            "    } else { // Radial\n"
            "        vec2 p = uv - 0.5;\n"
            "        float angle = atan(p.y, p.x);\n"
            "        float r = length(p);\n"
            "        float segAngle = 6.283185 / float(u_segments);\n"
            "        angle = mod(angle, segAngle);\n"
            "        if (angle > segAngle * 0.5) angle = segAngle - angle;\n"
            "        uv = vec2(cos(angle), sin(angle)) * r + 0.5;\n"
            "    }\n"
            "\n"
            "    gl_FragColor = texture2D(u_texture, clamp(uv, 0.0, 1.0));\n"
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

        float offset = getParamAsFloat ("offset", 0.5f);
        if (isInputConnected (1)) offset += getConnectedVisualValue (1) - 0.5f;

        if (auto l = loc ("u_mode");     l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("mode", 0));
        if (auto l = loc ("u_offset");   l >= 0) gl.extensions.glUniform1f (l, offset);
        if (auto l = loc ("u_segments"); l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("segments", 4));

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
