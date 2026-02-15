#include "Nodes/Visual/TileNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void TileNode::renderFrame (juce::OpenGLContext& gl)
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
            "uniform float u_countX;\n"
            "uniform float u_countY;\n"
            "uniform float u_offsetX;\n"
            "uniform float u_offsetY;\n"
            "uniform float u_rotation;\n"
            "uniform int   u_mirror;\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv;\n"
            "\n"
            "    // Apply rotation around center\n"
            "    vec2 p = uv - 0.5;\n"
            "    float c = cos(u_rotation), s = sin(u_rotation);\n"
            "    p = vec2(c * p.x - s * p.y, s * p.x + c * p.y);\n"
            "    uv = p + 0.5;\n"
            "\n"
            "    // Tile\n"
            "    vec2 scaled = uv * vec2(u_countX, u_countY) + vec2(u_offsetX, u_offsetY);\n"
            "    vec2 cell = floor(scaled);\n"
            "    vec2 tiled = fract(scaled);\n"
            "\n"
            "    // Mirror alternating cells\n"
            "    if (u_mirror == 1) {\n"
            "        if (mod(cell.x, 2.0) >= 1.0) tiled.x = 1.0 - tiled.x;\n"
            "        if (mod(cell.y, 2.0) >= 1.0) tiled.y = 1.0 - tiled.y;\n"
            "    }\n"
            "\n"
            "    gl_FragColor = texture2D(u_texture, tiled);\n"
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

        float countX = static_cast<float> (getParamAsInt ("countX", 2));
        float countY = static_cast<float> (getParamAsInt ("countY", 2));
        if (isInputConnected (1)) { float c = juce::jlimit (1.0f, 16.0f, getConnectedVisualValue (1) * 16.0f); countX = c; countY = c; }

        if (auto l = loc ("u_countX");    l >= 0) gl.extensions.glUniform1f (l, countX);
        if (auto l = loc ("u_countY");    l >= 0) gl.extensions.glUniform1f (l, countY);
        if (auto l = loc ("u_offsetX");   l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("offsetX", 0.0f));
        if (auto l = loc ("u_offsetY");   l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("offsetY", 0.0f));
        if (auto l = loc ("u_rotation");  l >= 0) gl.extensions.glUniform1f (l, juce::degreesToRadians (getParamAsFloat ("rotation", 0.0f)));
        if (auto l = loc ("u_mirror");    l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("mirror", 0));

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
