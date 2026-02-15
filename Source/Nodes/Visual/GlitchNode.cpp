#include "Nodes/Visual/GlitchNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void GlitchNode::renderFrame (juce::OpenGLContext& gl)
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
            "uniform float u_time;\n"
            "uniform float u_intensity;\n"
            "uniform float u_blockSize;\n"
            "uniform float u_rgbSplit;\n"
            "uniform float u_scanlines;\n"
            "uniform float u_noise;\n"
            "\n"
            "float hash(vec2 p) {\n"
            "    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv;\n"
            "    float t = floor(u_time * 8.0);\n"
            "\n"
            "    // Block displacement\n"
            "    vec2 block = floor(uv / u_blockSize);\n"
            "    float blockRand = hash(block + vec2(t));\n"
            "    if (blockRand > 1.0 - u_intensity * 0.3) {\n"
            "        float shift = (hash(block + vec2(t * 1.3, t)) - 0.5) * u_intensity * 0.2;\n"
            "        uv.x += shift;\n"
            "    }\n"
            "\n"
            "    // RGB split\n"
            "    float splitAmount = u_rgbSplit * u_intensity;\n"
            "    float r = texture2D(u_texture, vec2(uv.x + splitAmount, uv.y)).r;\n"
            "    float g = texture2D(u_texture, uv).g;\n"
            "    float b = texture2D(u_texture, vec2(uv.x - splitAmount, uv.y)).b;\n"
            "    float a = texture2D(u_texture, uv).a;\n"
            "    vec3 col = vec3(r, g, b);\n"
            "\n"
            "    // Scanlines\n"
            "    float scanline = sin(v_uv.y * 800.0) * 0.5 + 0.5;\n"
            "    col *= 1.0 - u_scanlines * scanline * 0.15 * u_intensity;\n"
            "\n"
            "    // Noise\n"
            "    float n = hash(v_uv * 500.0 + vec2(t));\n"
            "    col = mix(col, vec3(n), u_noise * u_intensity * 0.15);\n"
            "\n"
            "    gl_FragColor = vec4(col, a);\n"
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
        time_ += (1.0f / 60.0f) * getParamAsFloat ("speed", 1.0f);

        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (shaderProgram_, name);
        };

        float intensity = getParamAsFloat ("intensity", 0.3f);
        if (isInputConnected (1)) intensity *= juce::jlimit (0.0f, 4.0f, getConnectedVisualValue (1) * 2.0f);
        if (isInputConnected (2)) intensity = juce::jmax (intensity, getConnectedVisualValue (2));

        if (auto l = loc ("u_time");      l >= 0) gl.extensions.glUniform1f (l, time_);
        if (auto l = loc ("u_intensity"); l >= 0) gl.extensions.glUniform1f (l, intensity);
        if (auto l = loc ("u_blockSize"); l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("blockSize", 0.05f));
        if (auto l = loc ("u_rgbSplit");  l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("rgbSplit", 0.01f));
        if (auto l = loc ("u_scanlines"); l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("scanlines", 0.5f));
        if (auto l = loc ("u_noise");     l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("noiseAmount", 0.1f));

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
