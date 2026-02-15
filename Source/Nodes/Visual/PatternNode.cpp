#include "Nodes/Visual/PatternNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void PatternNode::renderFrame (juce::OpenGLContext& gl)
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
            "uniform float u_freq;\n"
            "uniform float u_rotation;\n"
            "uniform float u_thickness;\n"
            "uniform float u_time;\n"
            "uniform float u_softness;\n"
            "\n"
            "vec2 rotate2d(vec2 p, float a) {\n"
            "    float c = cos(a), s = sin(a);\n"
            "    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv - 0.5;\n"
            "    uv = rotate2d(uv, u_rotation);\n"
            "    uv += 0.5;\n"
            "    float v = 0.0;\n"
            "\n"
            "    if (u_type == 0) { // Stripes\n"
            "        v = smoothstep(u_thickness - u_softness, u_thickness + u_softness,\n"
            "                       fract(uv.x * u_freq + u_time));\n"
            "    } else if (u_type == 1) { // Checkerboard\n"
            "        vec2 c = floor(uv * u_freq + u_time);\n"
            "        v = mod(c.x + c.y, 2.0);\n"
            "    } else if (u_type == 2) { // Concentric circles\n"
            "        float r = length(uv - 0.5) * u_freq * 2.0 + u_time;\n"
            "        v = smoothstep(u_thickness - u_softness, u_thickness + u_softness,\n"
            "                       fract(r));\n"
            "    } else if (u_type == 3) { // Spiral\n"
            "        vec2 p = uv - 0.5;\n"
            "        float angle = atan(p.y, p.x);\n"
            "        float r = length(p);\n"
            "        v = smoothstep(u_thickness - u_softness, u_thickness + u_softness,\n"
            "                       fract(r * u_freq + angle / 6.283 + u_time));\n"
            "    } else if (u_type == 4) { // Grid dots\n"
            "        vec2 cell = fract(uv * u_freq + u_time) - 0.5;\n"
            "        float d = length(cell);\n"
            "        v = 1.0 - smoothstep(u_thickness * 0.4 - u_softness,\n"
            "                              u_thickness * 0.4 + u_softness, d);\n"
            "    } else { // Hexagonal\n"
            "        vec2 p = uv * u_freq + u_time;\n"
            "        vec2 r = vec2(1.0, 1.732);\n"
            "        vec2 h = r * 0.5;\n"
            "        vec2 a = mod(p, r) - h;\n"
            "        vec2 b = mod(p - h, r) - h;\n"
            "        vec2 gv = dot(a, a) < dot(b, b) ? a : b;\n"
            "        float d = length(gv);\n"
            "        v = 1.0 - smoothstep(u_thickness * 0.3 - u_softness,\n"
            "                              u_thickness * 0.3 + u_softness, d);\n"
            "    }\n"
            "\n"
            "    gl_FragColor = vec4(vec3(v), 1.0);\n"
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
        time_ += (1.0f / 60.0f) * getParamAsFloat ("speed", 0.0f);

        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (shaderProgram_, name);
        };

        float freq = getParamAsFloat ("frequency", 8.0f);
        float rotation = juce::degreesToRadians (getParamAsFloat ("rotation", 0.0f));
        float thickness = getParamAsFloat ("thickness", 0.5f);

        if (isInputConnected (0)) freq *= juce::jlimit (0.1f, 8.0f, getConnectedVisualValue (0) * 4.0f);
        if (isInputConnected (1)) rotation += getConnectedVisualValue (1) * 6.283f;
        if (isInputConnected (2)) thickness *= juce::jlimit (0.1f, 2.0f, getConnectedVisualValue (2));

        if (auto l = loc ("u_type");      l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("patternType", 0));
        if (auto l = loc ("u_freq");      l >= 0) gl.extensions.glUniform1f (l, freq);
        if (auto l = loc ("u_rotation");  l >= 0) gl.extensions.glUniform1f (l, rotation);
        if (auto l = loc ("u_thickness"); l >= 0) gl.extensions.glUniform1f (l, thickness);
        if (auto l = loc ("u_time");      l >= 0) gl.extensions.glUniform1f (l, time_);
        if (auto l = loc ("u_softness");  l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("softness", 0.02f));

        ShaderUtils::drawFullscreenQuad (gl, shaderProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
