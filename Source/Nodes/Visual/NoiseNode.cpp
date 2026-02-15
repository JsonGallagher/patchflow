#include "Nodes/Visual/NoiseNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void NoiseNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512, height = 512;

    ShaderUtils::ensureFBO (gl, fbo_, fboTexture_, fboWidth_, fboHeight_, width, height);
    ShaderUtils::ensureQuadVBO (gl, quadVBO_);

    if (! shaderCompiled_ && ! shaderError_)
    {
        const auto vertSrc = ShaderUtils::getStandardVertexShader();
        const auto fragSrc = ShaderUtils::getFragmentPreamble() +
            "varying vec2 v_uv;\n"
            "uniform float u_time;\n"
            "uniform float u_scale;\n"
            "uniform float u_speed;\n"
            "uniform vec2  u_offset;\n"
            "uniform int   u_noiseType;\n"
            "uniform int   u_octaves;\n"
            "uniform float u_lacunarity;\n"
            "uniform float u_persistence;\n"
            "uniform float u_domainWarp;\n"
            "uniform int   u_colorize;\n"
            "\n"
            "// --- Hash functions ---\n"
            "vec2 hash2(vec2 p) {\n"
            "    p = vec2(dot(p, vec2(127.1, 311.7)),\n"
            "             dot(p, vec2(269.5, 183.3)));\n"
            "    return -1.0 + 2.0 * fract(sin(p) * 43758.5453);\n"
            "}\n"
            "\n"
            "float hash1(vec2 p) {\n"
            "    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
            "}\n"
            "\n"
            "// --- Value Noise ---\n"
            "float valueNoise(vec2 p) {\n"
            "    vec2 i = floor(p);\n"
            "    vec2 f = fract(p);\n"
            "    f = f * f * (3.0 - 2.0 * f);\n"
            "    float a = hash1(i);\n"
            "    float b = hash1(i + vec2(1.0, 0.0));\n"
            "    float c = hash1(i + vec2(0.0, 1.0));\n"
            "    float d = hash1(i + vec2(1.0, 1.0));\n"
            "    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
            "}\n"
            "\n"
            "// --- Perlin Gradient Noise ---\n"
            "float perlinNoise(vec2 p) {\n"
            "    vec2 i = floor(p);\n"
            "    vec2 f = fract(p);\n"
            "    vec2 u = f * f * (3.0 - 2.0 * f);\n"
            "    return mix(mix(dot(hash2(i), f),\n"
            "                   dot(hash2(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0)), u.x),\n"
            "               mix(dot(hash2(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0)),\n"
            "                   dot(hash2(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0)), u.x), u.y) * 0.5 + 0.5;\n"
            "}\n"
            "\n"
            "// --- Simplex-like Noise ---\n"
            "float simplexNoise(vec2 p) {\n"
            "    const float K1 = 0.366025404; // (sqrt(3)-1)/2\n"
            "    const float K2 = 0.211324865; // (3-sqrt(3))/6\n"
            "    vec2 i = floor(p + (p.x + p.y) * K1);\n"
            "    vec2 a = p - i + (i.x + i.y) * K2;\n"
            "    float m = step(a.y, a.x);\n"
            "    vec2 o = vec2(m, 1.0 - m);\n"
            "    vec2 b = a - o + K2;\n"
            "    vec2 c = a - 1.0 + 2.0 * K2;\n"
            "    vec3 h = max(0.5 - vec3(dot(a, a), dot(b, b), dot(c, c)), 0.0);\n"
            "    vec3 n = h * h * h * h * vec3(dot(a, hash2(i)),\n"
            "                                   dot(b, hash2(i + o)),\n"
            "                                   dot(c, hash2(i + 1.0)));\n"
            "    return dot(n, vec3(70.0)) * 0.5 + 0.5;\n"
            "}\n"
            "\n"
            "// --- Worley (Cellular) Noise ---\n"
            "float worleyNoise(vec2 p) {\n"
            "    vec2 i = floor(p);\n"
            "    vec2 f = fract(p);\n"
            "    float minDist = 1.0;\n"
            "    for (int y = -1; y <= 1; ++y) {\n"
            "        for (int x = -1; x <= 1; ++x) {\n"
            "            vec2 neighbor = vec2(float(x), float(y));\n"
            "            vec2 point = hash2(i + neighbor) * 0.5 + 0.5;\n"
            "            float d = length(neighbor + point - f);\n"
            "            minDist = min(minDist, d);\n"
            "        }\n"
            "    }\n"
            "    return minDist;\n"
            "}\n"
            "\n"
            "float sampleNoise(vec2 p, int type) {\n"
            "    if (type == 1) return simplexNoise(p);\n"
            "    if (type == 2) return worleyNoise(p);\n"
            "    if (type == 3) return valueNoise(p);\n"
            "    return perlinNoise(p);\n"
            "}\n"
            "\n"
            "float fbm(vec2 p, int type, int octaves, float lac, float pers) {\n"
            "    float v = 0.0;\n"
            "    float a = 0.5;\n"
            "    for (int i = 0; i < 8; ++i) {\n"
            "        if (i >= octaves) break;\n"
            "        v += sampleNoise(p, type) * a;\n"
            "        p *= lac;\n"
            "        a *= pers;\n"
            "    }\n"
            "    return v;\n"
            "}\n"
            "\n"
            "vec3 hsv2rgb(vec3 c) {\n"
            "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
            "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
            "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv;\n"
            "    float t = u_time * u_speed;\n"
            "    vec2 p = uv * u_scale + u_offset;\n"
            "\n"
            "    // Domain warp\n"
            "    if (u_domainWarp > 0.0) {\n"
            "        vec2 warp = vec2(\n"
            "            fbm(p + vec2(t * 0.3, t * 0.1), u_noiseType, u_octaves, u_lacunarity, u_persistence),\n"
            "            fbm(p + vec2(-t * 0.2, t * 0.4), u_noiseType, u_octaves, u_lacunarity, u_persistence)\n"
            "        );\n"
            "        p += (warp - 0.5) * u_domainWarp * 2.0;\n"
            "    }\n"
            "\n"
            "    float n = fbm(p + vec2(t * 0.1, -t * 0.15), u_noiseType, u_octaves, u_lacunarity, u_persistence);\n"
            "\n"
            "    vec3 col;\n"
            "    if (u_colorize == 1) {\n"
            "        col = hsv2rgb(vec3(n, 0.8, 0.9));\n"
            "    } else {\n"
            "        col = vec3(n);\n"
            "    }\n"
            "\n"
            "    gl_FragColor = vec4(col, 1.0);\n"
            "}\n";

        juce::String errorLog;
        auto vs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertSrc, errorLog);
        if (vs == 0) { shaderError_ = true; DBG ("NoiseNode VS error: " + errorLog); return; }

        auto fs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragSrc, errorLog);
        if (fs == 0) { gl.extensions.glDeleteShader (vs); shaderError_ = true; DBG ("NoiseNode FS error: " + errorLog); return; }

        shaderProgram_ = ShaderUtils::linkProgram (gl, vs, fs, errorLog);
        if (shaderProgram_ == 0) { shaderError_ = true; DBG ("NoiseNode link error: " + errorLog); return; }

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
        time_ += 1.0f / 60.0f;

        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (shaderProgram_, name);
        };

        float scale = getParamAsFloat ("scale", 4.0f);
        float speed = getParamAsFloat ("speed", 0.3f);
        float offsetX = 0.0f, offsetY = 0.0f;

        if (isInputConnected (0)) scale *= juce::jlimit (0.1f, 8.0f, getConnectedVisualValue (0) * 4.0f);
        if (isInputConnected (1)) speed *= juce::jlimit (0.0f, 4.0f, getConnectedVisualValue (1) * 2.0f);
        if (isInputConnected (2)) offsetX = getConnectedVisualValue (2) * 10.0f;
        if (isInputConnected (3)) offsetY = getConnectedVisualValue (3) * 10.0f;

        if (auto l = loc ("u_time");        l >= 0) gl.extensions.glUniform1f (l, time_);
        if (auto l = loc ("u_scale");       l >= 0) gl.extensions.glUniform1f (l, scale);
        if (auto l = loc ("u_speed");       l >= 0) gl.extensions.glUniform1f (l, speed);
        if (auto l = loc ("u_offset");      l >= 0) gl.extensions.glUniform2f (l, offsetX, offsetY);
        if (auto l = loc ("u_noiseType");   l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("noiseType", 0));
        if (auto l = loc ("u_octaves");     l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("octaves", 4));
        if (auto l = loc ("u_lacunarity");  l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("lacunarity", 2.0f));
        if (auto l = loc ("u_persistence"); l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("persistence", 0.5f));
        if (auto l = loc ("u_domainWarp");  l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("domainWarp", 0.0f));
        if (auto l = loc ("u_colorize");    l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("colorize", 0));

        ShaderUtils::drawFullscreenQuad (gl, shaderProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
