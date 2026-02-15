#include "Nodes/Visual/ColorGradeNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void ColorGradeNode::renderFrame (juce::OpenGLContext& gl)
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
            "uniform float u_hueShift;\n"
            "uniform float u_saturation;\n"
            "uniform float u_brightness;\n"
            "uniform float u_contrast;\n"
            "uniform float u_gamma;\n"
            "uniform int   u_invert;\n"
            "\n"
            "vec3 rgb2hsv(vec3 c) {\n"
            "    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);\n"
            "    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n"
            "    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n"
            "    float d = q.x - min(q.w, q.y);\n"
            "    float e = 1.0e-10;\n"
            "    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n"
            "}\n"
            "\n"
            "vec3 hsv2rgb(vec3 c) {\n"
            "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
            "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
            "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec4 col = texture2D(u_texture, v_uv);\n"
            "    vec3 rgb = col.rgb;\n"
            "\n"
            "    // Invert\n"
            "    if (u_invert == 1) rgb = 1.0 - rgb;\n"
            "\n"
            "    // HSV adjustments\n"
            "    vec3 hsv = rgb2hsv(rgb);\n"
            "    hsv.x = fract(hsv.x + u_hueShift);\n"
            "    hsv.y *= u_saturation;\n"
            "    hsv.z *= u_brightness;\n"
            "    rgb = hsv2rgb(hsv);\n"
            "\n"
            "    // Contrast (centered at 0.5)\n"
            "    rgb = (rgb - 0.5) * u_contrast + 0.5;\n"
            "\n"
            "    // Gamma\n"
            "    rgb = pow(max(rgb, 0.0), vec3(1.0 / u_gamma));\n"
            "\n"
            "    gl_FragColor = vec4(clamp(rgb, 0.0, 1.0), col.a);\n"
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

        float hueShift = getParamAsFloat ("hueShift", 0.0f);
        float saturation = getParamAsFloat ("saturation", 1.0f);
        float brightness = getParamAsFloat ("brightness", 1.0f);

        if (isInputConnected (1)) hueShift += getConnectedVisualValue (1) - 0.5f;
        if (isInputConnected (2)) saturation *= juce::jlimit (0.0f, 3.0f, getConnectedVisualValue (2) * 2.0f);
        if (isInputConnected (3)) brightness *= juce::jlimit (0.0f, 3.0f, getConnectedVisualValue (3) * 2.0f);

        if (auto l = loc ("u_hueShift");   l >= 0) gl.extensions.glUniform1f (l, hueShift);
        if (auto l = loc ("u_saturation"); l >= 0) gl.extensions.glUniform1f (l, saturation);
        if (auto l = loc ("u_brightness"); l >= 0) gl.extensions.glUniform1f (l, brightness);
        if (auto l = loc ("u_contrast");   l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("contrast", 1.0f));
        if (auto l = loc ("u_gamma");      l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("gamma", 1.0f));
        if (auto l = loc ("u_invert");     l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("invert", 0));

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
