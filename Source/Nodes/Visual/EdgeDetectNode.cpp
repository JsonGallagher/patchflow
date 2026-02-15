#include "Nodes/Visual/EdgeDetectNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void EdgeDetectNode::renderFrame (juce::OpenGLContext& gl)
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
            "uniform vec2  u_texel;\n"
            "uniform int   u_mode;\n"
            "uniform float u_strength;\n"
            "uniform int   u_invert;\n"
            "uniform int   u_overlay;\n"
            "\n"
            "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
            "\n"
            "void main() {\n"
            "    // Sample 3x3 neighborhood\n"
            "    float tl = luma(texture2D(u_texture, v_uv + vec2(-u_texel.x, u_texel.y)).rgb);\n"
            "    float tc = luma(texture2D(u_texture, v_uv + vec2(0.0, u_texel.y)).rgb);\n"
            "    float tr = luma(texture2D(u_texture, v_uv + vec2(u_texel.x, u_texel.y)).rgb);\n"
            "    float ml = luma(texture2D(u_texture, v_uv + vec2(-u_texel.x, 0.0)).rgb);\n"
            "    float mc = luma(texture2D(u_texture, v_uv).rgb);\n"
            "    float mr = luma(texture2D(u_texture, v_uv + vec2(u_texel.x, 0.0)).rgb);\n"
            "    float bl = luma(texture2D(u_texture, v_uv + vec2(-u_texel.x, -u_texel.y)).rgb);\n"
            "    float bc = luma(texture2D(u_texture, v_uv + vec2(0.0, -u_texel.y)).rgb);\n"
            "    float br = luma(texture2D(u_texture, v_uv + vec2(u_texel.x, -u_texel.y)).rgb);\n"
            "\n"
            "    float edge = 0.0;\n"
            "    if (u_mode == 0) { // Sobel\n"
            "        float gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;\n"
            "        float gy = -tl - 2.0*tc - tr + bl + 2.0*bc + br;\n"
            "        edge = sqrt(gx*gx + gy*gy);\n"
            "    } else { // Laplacian\n"
            "        edge = abs(-4.0*mc + tc + ml + mr + bc);\n"
            "    }\n"
            "\n"
            "    edge = clamp(edge * u_strength, 0.0, 1.0);\n"
            "    if (u_invert == 1) edge = 1.0 - edge;\n"
            "\n"
            "    vec3 col;\n"
            "    if (u_overlay == 1) {\n"
            "        col = texture2D(u_texture, v_uv).rgb + vec3(edge);\n"
            "    } else {\n"
            "        col = vec3(edge);\n"
            "    }\n"
            "\n"
            "    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
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

        float strength = getParamAsFloat ("strength", 1.0f);
        if (isInputConnected (1)) strength *= juce::jlimit (0.0f, 5.0f, getConnectedVisualValue (1) * 3.0f);

        if (auto l = loc ("u_texel");    l >= 0) gl.extensions.glUniform2f (l, 1.0f / width, 1.0f / height);
        if (auto l = loc ("u_mode");     l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("mode", 0));
        if (auto l = loc ("u_strength"); l >= 0) gl.extensions.glUniform1f (l, strength);
        if (auto l = loc ("u_invert");   l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("invert", 0));
        if (auto l = loc ("u_overlay");  l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("overlay", 0));

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
