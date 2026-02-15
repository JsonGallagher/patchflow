#include "Nodes/Visual/ReactionDiffusionNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

namespace
{
juce::uint32 compileRDShader (juce::OpenGLContext& gl, const juce::String& fragBody)
{
    auto vertSrc = ShaderUtils::getStandardVertexShader();
    auto fragSrc = ShaderUtils::getFragmentPreamble() + fragBody;

    juce::String errorLog;
    auto vs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertSrc, errorLog);
    if (vs == 0) return 0;
    auto fs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragSrc, errorLog);
    if (fs == 0) { gl.extensions.glDeleteShader (vs); return 0; }
    return ShaderUtils::linkProgram (gl, vs, fs, errorLog);
}

void ensureRGFBO (juce::OpenGLContext& gl, juce::uint32 fbos[2], juce::uint32 textures[2],
                  int& fboW, int& fboH, int w, int h)
{
    if (fbos[0] != 0 && fboW == w && fboH == h) return;

    if (fbos[0] != 0)
    {
        gl.extensions.glDeleteFramebuffers (2, fbos);
        juce::gl::glDeleteTextures (2, textures);
    }

    juce::gl::glGenTextures (2, textures);
    gl.extensions.glGenFramebuffers (2, fbos);

    for (int i = 0; i < 2; ++i)
    {
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, textures[i]);
        juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RG32F,
                                w, h, 0,
                                juce::gl::GL_RG, juce::gl::GL_FLOAT, nullptr);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_REPEAT);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_REPEAT);

        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos[i]);
        gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER,
                                              juce::gl::GL_COLOR_ATTACHMENT0,
                                              juce::gl::GL_TEXTURE_2D, textures[i], 0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    fboW = w;
    fboH = h;
}
} // namespace

void ReactionDiffusionNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int simW = 256, simH = 256; // Lower res for simulation speed
    constexpr int outW = 512, outH = 512;

    ensureRGFBO (gl, simFBOs_, simTextures_, fboWidth_, fboHeight_, simW, simH);
    ShaderUtils::ensureFBO (gl, renderFBO_, renderTexture_, renderWidth_, renderHeight_, outW, outH);
    ShaderUtils::ensureQuadVBO (gl, quadVBO_);

    if (! shadersCompiled_ && ! shaderError_)
    {
        // Simulation shader (Gray-Scott)
        simProgram_ = compileRDShader (gl,
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_state;\n"
            "uniform vec2  u_texel;\n"
            "uniform float u_feed;\n"
            "uniform float u_kill;\n"
            "uniform float u_dA;\n"
            "uniform float u_dB;\n"
            "uniform float u_dt;\n"
            "\n"
            "void main() {\n"
            "    vec2 s = texture2D(u_state, v_uv).rg;\n"
            "    float a = s.r;\n"
            "    float b = s.g;\n"
            "\n"
            "    // 5-point Laplacian\n"
            "    float lapA = texture2D(u_state, v_uv + vec2(u_texel.x, 0.0)).r\n"
            "               + texture2D(u_state, v_uv - vec2(u_texel.x, 0.0)).r\n"
            "               + texture2D(u_state, v_uv + vec2(0.0, u_texel.y)).r\n"
            "               + texture2D(u_state, v_uv - vec2(0.0, u_texel.y)).r\n"
            "               - 4.0 * a;\n"
            "    float lapB = texture2D(u_state, v_uv + vec2(u_texel.x, 0.0)).g\n"
            "               + texture2D(u_state, v_uv - vec2(u_texel.x, 0.0)).g\n"
            "               + texture2D(u_state, v_uv + vec2(0.0, u_texel.y)).g\n"
            "               + texture2D(u_state, v_uv - vec2(0.0, u_texel.y)).g\n"
            "               - 4.0 * b;\n"
            "\n"
            "    float abb = a * b * b;\n"
            "    float newA = a + (u_dA * lapA - abb + u_feed * (1.0 - a)) * u_dt;\n"
            "    float newB = b + (u_dB * lapB + abb - (u_kill + u_feed) * b) * u_dt;\n"
            "\n"
            "    gl_FragColor = vec4(clamp(newA, 0.0, 1.0), clamp(newB, 0.0, 1.0), 0.0, 1.0);\n"
            "}\n");

        // Render shader (colorize)
        renderProgram_ = compileRDShader (gl,
            "varying vec2 v_uv;\n"
            "uniform sampler2D u_state;\n"
            "\n"
            "void main() {\n"
            "    vec2 s = texture2D(u_state, v_uv).rg;\n"
            "    float val = s.r - s.g;\n"
            "    vec3 col = mix(vec3(0.0, 0.05, 0.15), vec3(0.9, 0.95, 1.0), clamp(val, 0.0, 1.0));\n"
            "    col = mix(col, vec3(0.1, 0.6, 0.9), clamp(s.g * 3.0, 0.0, 1.0));\n"
            "    gl_FragColor = vec4(col, 1.0);\n"
            "}\n");

        if (simProgram_ == 0 || renderProgram_ == 0)
            shaderError_ = true;

        shadersCompiled_ = true;
    }

    // Reset trigger
    if (isInputConnected (2))
    {
        float resetVal = getConnectedVisualValue (2);
        if (resetVal > 0.5f && lastReset_ <= 0.5f)
            needsSeed_ = true;
        lastReset_ = resetVal;
    }

    // Seed simulation
    if (needsSeed_)
    {
        int numPixels = simW * simH;
        std::vector<float> seedData (numPixels * 2, 0.0f);

        // Fill with A=1, B=0 everywhere
        for (int i = 0; i < numPixels; ++i)
        {
            seedData[i * 2]     = 1.0f; // A
            seedData[i * 2 + 1] = 0.0f; // B
        }

        // Add random seed spots of B
        for (int s = 0; s < 20; ++s)
        {
            int cx = (std::rand() % (simW - 20)) + 10;
            int cy = (std::rand() % (simH - 20)) + 10;
            for (int dy = -5; dy <= 5; ++dy)
            {
                for (int dx = -5; dx <= 5; ++dx)
                {
                    int idx = ((cy + dy) * simW + (cx + dx));
                    if (idx >= 0 && idx < numPixels)
                    {
                        seedData[idx * 2]     = 0.5f; // Lower A
                        seedData[idx * 2 + 1] = 0.25f; // Inject B
                    }
                }
            }
        }

        for (int i = 0; i < 2; ++i)
        {
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, simTextures_[i]);
            juce::gl::glTexSubImage2D (juce::gl::GL_TEXTURE_2D, 0, 0, 0, simW, simH,
                                       juce::gl::GL_RG, juce::gl::GL_FLOAT, seedData.data());
        }

        latestIdx_ = 0;
        needsSeed_ = false;
    }

    if (shaderError_) return;

    // Apply presets
    float feed = getParamAsFloat ("feed", 0.055f);
    float kill = getParamAsFloat ("kill", 0.062f);
    int preset = getParamAsInt ("preset", 0);

    if (preset != 4) // Not custom
    {
        constexpr float presets[][2] = {
            { 0.0367f, 0.0649f }, // Mitosis
            { 0.0620f, 0.0609f }, // Coral
            { 0.0290f, 0.0570f }, // Maze
            { 0.0350f, 0.0650f }, // Spots
        };
        feed = presets[preset][0];
        kill = presets[preset][1];
    }

    if (isInputConnected (0)) feed += (getConnectedVisualValue (0) - 0.5f) * 0.02f;
    if (isInputConnected (1)) kill += (getConnectedVisualValue (1) - 0.5f) * 0.02f;

    feed = juce::jlimit (0.01f, 0.1f, feed);
    kill = juce::jlimit (0.04f, 0.08f, kill);

    float dA = getParamAsFloat ("diffuseA", 1.0f);
    float dB = getParamAsFloat ("diffuseB", 0.5f);
    int steps = getParamAsInt ("speed", 4);

    // Simulation steps
    gl.extensions.glUseProgram (simProgram_);

    auto simLoc = [&] (const char* name) {
        return gl.extensions.glGetUniformLocation (simProgram_, name);
    };

    if (auto l = simLoc ("u_texel"); l >= 0) gl.extensions.glUniform2f (l, 1.0f / simW, 1.0f / simH);
    if (auto l = simLoc ("u_feed");  l >= 0) gl.extensions.glUniform1f (l, feed);
    if (auto l = simLoc ("u_kill");  l >= 0) gl.extensions.glUniform1f (l, kill);
    if (auto l = simLoc ("u_dA");    l >= 0) gl.extensions.glUniform1f (l, dA);
    if (auto l = simLoc ("u_dB");    l >= 0) gl.extensions.glUniform1f (l, dB);
    if (auto l = simLoc ("u_dt");    l >= 0) gl.extensions.glUniform1f (l, 1.0f);

    for (int i = 0; i < steps; ++i)
    {
        int readIdx = latestIdx_;
        int writeIdx = 1 - latestIdx_;

        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, simFBOs_[writeIdx]);
        juce::gl::glViewport (0, 0, simW, simH);

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, simTextures_[readIdx]);
        if (auto l = simLoc ("u_state"); l >= 0) gl.extensions.glUniform1i (l, 0);

        ShaderUtils::drawFullscreenQuad (gl, simProgram_, quadVBO_);

        latestIdx_ = writeIdx;
    }

    gl.extensions.glUseProgram (0);

    // Render colorized output
    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, renderFBO_);
    juce::gl::glViewport (0, 0, outW, outH);

    gl.extensions.glUseProgram (renderProgram_);
    juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, simTextures_[latestIdx_]);
    auto renderLoc = gl.extensions.glGetUniformLocation (renderProgram_, "u_state");
    if (renderLoc >= 0) gl.extensions.glUniform1i (renderLoc, 0);

    ShaderUtils::drawFullscreenQuad (gl, renderProgram_, quadVBO_);
    gl.extensions.glUseProgram (0);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, renderTexture_);
}

} // namespace pf
