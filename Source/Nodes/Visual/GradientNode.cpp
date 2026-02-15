#include "Nodes/Visual/GradientNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void GradientNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512, height = 512;

    ShaderUtils::ensureFBO (gl, fbo_, fboTexture_, fboWidth_, fboHeight_, width, height);
    ShaderUtils::ensureQuadVBO (gl, quadVBO_);

    if (! shaderCompiled_ && ! shaderError_)
    {
        const auto vertSrc = ShaderUtils::getStandardVertexShader();
        const auto fragSrc = ShaderUtils::getFragmentPreamble() +
            "varying vec2 v_uv;\n"
            "uniform int   u_type;\n"
            "uniform float u_rotation;\n"
            "uniform float u_offset;\n"
            "uniform float u_spread;\n"
            "uniform vec3  u_colorA;\n"
            "uniform vec3  u_colorB;\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv - 0.5;\n"
            "    float c = cos(u_rotation), s = sin(u_rotation);\n"
            "    vec2 ruv = vec2(c * uv.x - s * uv.y, s * uv.x + c * uv.y);\n"
            "\n"
            "    float t = 0.0;\n"
            "    if (u_type == 0) { // Linear\n"
            "        t = (ruv.x + 0.5 + u_offset) * u_spread;\n"
            "    } else if (u_type == 1) { // Radial\n"
            "        t = length(uv + vec2(u_offset)) * 2.0 * u_spread;\n"
            "    } else if (u_type == 2) { // Angular\n"
            "        t = (atan(uv.y + u_offset * 0.5, uv.x) / 6.283 + 0.5) * u_spread;\n"
            "    } else { // Diamond\n"
            "        t = (abs(ruv.x) + abs(ruv.y) + u_offset) * u_spread;\n"
            "    }\n"
            "    t = clamp(t, 0.0, 1.0);\n"
            "\n"
            "    gl_FragColor = vec4(mix(u_colorA, u_colorB, t), 1.0);\n"
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

        float rotation = juce::degreesToRadians (getParamAsFloat ("rotation", 0.0f));
        float offset = getParamAsFloat ("offset", 0.0f);
        float spread = getParamAsFloat ("spread", 1.0f);

        if (isInputConnected (0)) rotation += getConnectedVisualValue (0) * 6.283f;
        if (isInputConnected (1)) offset += getConnectedVisualValue (1) - 0.5f;
        if (isInputConnected (2)) spread *= juce::jlimit (0.1f, 4.0f, getConnectedVisualValue (2) * 2.0f);

        if (auto l = loc ("u_type");     l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("gradientType", 0));
        if (auto l = loc ("u_rotation"); l >= 0) gl.extensions.glUniform1f (l, rotation);
        if (auto l = loc ("u_offset");   l >= 0) gl.extensions.glUniform1f (l, offset);
        if (auto l = loc ("u_spread");   l >= 0) gl.extensions.glUniform1f (l, spread);
        if (auto l = loc ("u_colorA");   l >= 0) gl.extensions.glUniform3f (l,
            getParamAsFloat ("colorA_r", 0.0f), getParamAsFloat ("colorA_g", 0.0f), getParamAsFloat ("colorA_b", 0.0f));
        if (auto l = loc ("u_colorB");   l >= 0) gl.extensions.glUniform3f (l,
            getParamAsFloat ("colorB_r", 1.0f), getParamAsFloat ("colorB_g", 1.0f), getParamAsFloat ("colorB_b", 1.0f));

        ShaderUtils::drawFullscreenQuad (gl, shaderProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
