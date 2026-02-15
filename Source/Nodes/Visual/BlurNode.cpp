#include "Nodes/Visual/BlurNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

namespace
{
juce::uint32 compileBlurShader (juce::OpenGLContext& gl, const juce::String& fragBody)
{
    auto vertSrc = ShaderUtils::getStandardVertexShader();
    auto fragSrc = ShaderUtils::getFragmentPreamble() + fragBody;

    juce::String errorLog;
    auto vs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertSrc, errorLog);
    if (vs == 0) { DBG ("Blur VS error: " + errorLog); return 0; }

    auto fs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragSrc, errorLog);
    if (fs == 0) { gl.extensions.glDeleteShader (vs); DBG ("Blur FS error: " + errorLog); return 0; }

    auto prog = ShaderUtils::linkProgram (gl, vs, fs, errorLog);
    if (prog == 0) { DBG ("Blur link error: " + errorLog); }
    return prog;
}
} // namespace

void BlurNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512, height = 512;

    ShaderUtils::ensurePingPongFBOs (gl, fbos_, textures_, fboWidth_, fboHeight_, width, height);
    ShaderUtils::ensureQuadVBO (gl, quadVBO_);
    ShaderUtils::ensureFallbackTexture (fallbackTexture_);

    if (! shadersCompiled_ && ! shaderError_)
    {
        // Gaussian (9-tap separable)
        gaussianProgram_ = compileBlurShader (gl,
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_texture;\n"
            "uniform vec2  u_dir;\n"
            "uniform vec2  u_texel;\n"
            "\n"
            "void main() {\n"
            "    vec4 sum = vec4(0.0);\n"
            "    vec2 step = u_dir * u_texel;\n"
            "    sum += texture2D(u_texture, v_uv - 4.0 * step) * 0.0162;\n"
            "    sum += texture2D(u_texture, v_uv - 3.0 * step) * 0.0540;\n"
            "    sum += texture2D(u_texture, v_uv - 2.0 * step) * 0.1216;\n"
            "    sum += texture2D(u_texture, v_uv - 1.0 * step) * 0.1945;\n"
            "    sum += texture2D(u_texture, v_uv)               * 0.2270;\n"
            "    sum += texture2D(u_texture, v_uv + 1.0 * step) * 0.1945;\n"
            "    sum += texture2D(u_texture, v_uv + 2.0 * step) * 0.1216;\n"
            "    sum += texture2D(u_texture, v_uv + 3.0 * step) * 0.0540;\n"
            "    sum += texture2D(u_texture, v_uv + 4.0 * step) * 0.0162;\n"
            "    gl_FragColor = sum;\n"
            "}\n");

        // Radial (zoom blur)
        radialProgram_ = compileBlurShader (gl,
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_texture;\n"
            "uniform float u_amount;\n"
            "uniform vec2  u_center;\n"
            "\n"
            "void main() {\n"
            "    vec2 dir = v_uv - u_center;\n"
            "    vec4 sum = vec4(0.0);\n"
            "    float samples = 16.0;\n"
            "    for (float i = 0.0; i < 16.0; i += 1.0) {\n"
            "        float t = i / samples;\n"
            "        vec2 offset = dir * t * u_amount * 0.1;\n"
            "        sum += texture2D(u_texture, v_uv - offset);\n"
            "    }\n"
            "    gl_FragColor = sum / samples;\n"
            "}\n");

        // Directional (motion blur)
        directionalProgram_ = compileBlurShader (gl,
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_texture;\n"
            "uniform vec2  u_dir;\n"
            "uniform float u_amount;\n"
            "\n"
            "void main() {\n"
            "    vec4 sum = vec4(0.0);\n"
            "    float samples = 16.0;\n"
            "    vec2 step = u_dir * u_amount * 0.01;\n"
            "    for (float i = -8.0; i < 8.0; i += 1.0) {\n"
            "        sum += texture2D(u_texture, v_uv + step * i / 8.0);\n"
            "    }\n"
            "    gl_FragColor = sum / samples;\n"
            "}\n");

        if (gaussianProgram_ == 0 && radialProgram_ == 0 && directionalProgram_ == 0)
            shaderError_ = true;

        shadersCompiled_ = true;
    }

    int mode = getParamAsInt ("mode", 0);
    float amount = getParamAsFloat ("amount", 1.0f);
    int passes = getParamAsInt ("passes", 2);

    if (isInputConnected (1)) amount *= juce::jlimit (0.0f, 4.0f, getConnectedVisualValue (1) * 2.0f);

    juce::uint32 inputTex = isInputConnected (0) ? getConnectedTexture (0) : fallbackTexture_;

    if (shaderError_)
    {
        setTextureOutput (0, inputTex);
        return;
    }

    if (mode == 0) // Gaussian separable
    {
        juce::uint32 currentInput = inputTex;
        int writeIdx = 0;

        for (int pass = 0; pass < passes; ++pass)
        {
            for (int axis = 0; axis < 2; ++axis)
            {
                gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos_[writeIdx]);
                juce::gl::glViewport (0, 0, width, height);

                gl.extensions.glUseProgram (gaussianProgram_);

                auto loc = [&] (const char* name) {
                    return gl.extensions.glGetUniformLocation (gaussianProgram_, name);
                };

                if (axis == 0)
                {
                    if (auto l = loc ("u_dir"); l >= 0) gl.extensions.glUniform2f (l, amount, 0.0f);
                }
                else
                {
                    if (auto l = loc ("u_dir"); l >= 0) gl.extensions.glUniform2f (l, 0.0f, amount);
                }

                if (auto l = loc ("u_texel"); l >= 0)
                    gl.extensions.glUniform2f (l, 1.0f / static_cast<float> (width), 1.0f / static_cast<float> (height));

                juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
                juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, currentInput);
                if (auto l = loc ("u_texture"); l >= 0) gl.extensions.glUniform1i (l, 0);

                ShaderUtils::drawFullscreenQuad (gl, gaussianProgram_, quadVBO_);
                gl.extensions.glUseProgram (0);

                currentInput = textures_[writeIdx];
                writeIdx = 1 - writeIdx;
            }
        }

        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
        setTextureOutput (0, currentInput);
    }
    else if (mode == 1) // Radial
    {
        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos_[0]);
        juce::gl::glViewport (0, 0, width, height);

        gl.extensions.glUseProgram (radialProgram_);
        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (radialProgram_, name);
        };

        if (auto l = loc ("u_amount"); l >= 0) gl.extensions.glUniform1f (l, amount);
        if (auto l = loc ("u_center"); l >= 0)
            gl.extensions.glUniform2f (l, getParamAsFloat ("center_x", 0.5f), getParamAsFloat ("center_y", 0.5f));

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, inputTex);
        if (auto l = loc ("u_texture"); l >= 0) gl.extensions.glUniform1i (l, 0);

        ShaderUtils::drawFullscreenQuad (gl, radialProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);

        setTextureOutput (0, textures_[0]);
    }
    else // Directional
    {
        float dirAngle = juce::degreesToRadians (getParamAsFloat ("directionDeg", 0.0f));
        if (isInputConnected (2)) dirAngle += getConnectedVisualValue (2) * 6.283f;

        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos_[0]);
        juce::gl::glViewport (0, 0, width, height);

        gl.extensions.glUseProgram (directionalProgram_);
        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (directionalProgram_, name);
        };

        if (auto l = loc ("u_amount"); l >= 0) gl.extensions.glUniform1f (l, amount);
        if (auto l = loc ("u_dir");    l >= 0) gl.extensions.glUniform2f (l, std::cos (dirAngle), std::sin (dirAngle));

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, inputTex);
        if (auto l = loc ("u_texture"); l >= 0) gl.extensions.glUniform1i (l, 0);

        ShaderUtils::drawFullscreenQuad (gl, directionalProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);

        setTextureOutput (0, textures_[0]);
    }
}

} // namespace pf
