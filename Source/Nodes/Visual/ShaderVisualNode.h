#pragma once
#include "Nodes/NodeBase.h"
#include <juce_opengl/juce_opengl.h>

namespace pf
{

class ShaderVisualNode : public NodeBase
{
public:
    ShaderVisualNode()
    {
        addInput  ("param1",     PortType::Visual);
        addInput  ("param2",     PortType::Visual);
        addInput  ("param3",     PortType::Visual);
        addInput  ("param4",     PortType::Visual);
        addInput  ("magnitudes", PortType::Buffer);
        addInput  ("texture_in", PortType::Texture);
        addOutput ("texture",    PortType::Texture);
        addParam  ("fragmentShader", getDefaultFragmentShader(),
                   {}, {}, "Fragment Shader", "GLSL fragment shader code");
    }

    juce::String getTypeId()      const override { return "ShaderVisual"; }
    juce::String getDisplayName() const override { return "Shader Visual"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override;

    void updateMagnitudes (const float* data, int numBins)
    {
        if (data && numBins > 0)
        {
            magnitudeSnapshot_.resize (static_cast<size_t> (numBins));
            std::memcpy (magnitudeSnapshot_.data(), data, sizeof (float) * numBins);
        }
    }

    static juce::String getDefaultFragmentShader()
    {
        return
            "uniform float u_time;\n"
            "uniform vec2  u_resolution;\n"
            "uniform float u_param1;\n"
            "uniform float u_param2;\n"
            "uniform float u_param3;\n"
            "uniform float u_param4;\n"
            "uniform float u_audioLevel;\n"
            "uniform float u_bassLevel;\n"
            "uniform float u_midLevel;\n"
            "uniform float u_highLevel;\n"
            "uniform float u_pulse;\n"
            "uniform sampler2D u_spectrum;\n"
            "\n"
            "float hash(vec2 p) {\n"
            "    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
            "}\n"
            "\n"
            "float noise(vec2 p) {\n"
            "    vec2 i = floor(p);\n"
            "    vec2 f = fract(p);\n"
            "    f = f * f * (3.0 - 2.0 * f);\n"
            "    float a = hash(i);\n"
            "    float b = hash(i + vec2(1.0, 0.0));\n"
            "    float c = hash(i + vec2(0.0, 1.0));\n"
            "    float d = hash(i + vec2(1.0, 1.0));\n"
            "    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
            "}\n"
            "\n"
            "float fbm(vec2 p) {\n"
            "    float v = 0.0;\n"
            "    float a = 0.5;\n"
            "    for (int i = 0; i < 5; ++i) {\n"
            "        v += noise(p) * a;\n"
            "        p *= 2.0;\n"
            "        a *= 0.5;\n"
            "    }\n"
            "    return v;\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = gl_FragCoord.xy / u_resolution;\n"
            "    vec2 p = uv * 2.0 - 1.0;\n"
            "    p.x *= u_resolution.x / u_resolution.y;\n"
            "\n"
            "    float userA = clamp(u_param1 * 2.0, 0.0, 2.0);\n"
            "    float userB = clamp(u_param2 * 2.0, 0.0, 2.0);\n"
            "    float userC = clamp(u_param3 * 2.0, 0.0, 2.0);\n"
            "    float userD = clamp(u_param4 * 2.0, 0.0, 2.0);\n"
            "\n"
            "    float bass = clamp(u_bassLevel + userA * 0.5, 0.0, 3.0);\n"
            "    float mid = clamp(u_midLevel + userB * 0.5, 0.0, 3.0);\n"
            "    float high = clamp(u_highLevel + userC * 0.5, 0.0, 3.0);\n"
            "    float pulse = clamp(u_pulse + u_audioLevel * 0.4 + userD * 0.5, 0.0, 3.0);\n"
            "    float t = u_time * (0.2 + bass * 0.05 + mid * 0.03);\n"
            "\n"
            "    vec2 warp = vec2(\n"
            "        fbm(p * 1.8 + vec2( t, -t * 0.6)),\n"
            "        fbm(p * 1.8 + vec2(-t * 0.4, t))\n"
            "    );\n"
            "    vec2 q = p + (warp - 0.5) * (0.35 + bass * 0.2 + pulse * 0.08);\n"
            "\n"
            "    float nebula = fbm(q * (2.0 + high * 0.3) + vec2(t * 1.3, -t));\n"
            "    float stream = exp(-abs(q.y + sin(q.x * 6.0 + t * 6.0) * (0.15 + mid * 0.08))\n"
            "                       * (6.0 + high * 3.0));\n"
            "    float spec = texture2D(u_spectrum, vec2(uv.x, 0.0)).r;\n"
            "    float spark = step(0.985 - high * 0.05, hash(floor((q + vec2(t * 4.0, -t * 2.0)) * 44.0)));\n"
            "    spark *= 0.3 + pulse;\n"
            "\n"
            "    vec3 bg = mix(vec3(0.03, 0.04, 0.08), vec3(0.02, 0.08, 0.14), uv.y);\n"
            "    vec3 nebColA = vec3(0.12, 0.34, 0.85);\n"
            "    vec3 nebColB = vec3(0.96, 0.30, 0.50);\n"
            "    vec3 nebCol = mix(nebColA, nebColB, nebula);\n"
            "\n"
            "    vec3 col = bg;\n"
            "    col += nebCol * nebula * (0.35 + pulse * 0.25 + userD * 0.1);\n"
            "    col += vec3(0.45, 0.8, 1.0) * stream * (0.12 + mid * 0.08);\n"
            "    col += vec3(1.0, 0.93, 0.7) * spark;\n"
            "    col += vec3(0.45, 0.7, 1.0) * spec * (0.08 + bass * 0.12) * (1.0 - uv.y);\n"
            "\n"
            "    float vignette = smoothstep(1.4, 0.25, length(p));\n"
            "    col *= vignette;\n"
            "    gl_FragColor = vec4(col, 1.0);\n"
            "}\n";
    }

    static juce::String getDefaultVertexShader()
    {
        return
            "attribute vec2 a_position;\n"
            "void main() {\n"
            "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
            "}\n";
    }

private:
    void ensureFBO (juce::OpenGLContext& gl, int width, int height);
    void compileShader (juce::OpenGLContext& gl);
    void updateSpectrumTexture (juce::OpenGLContext& gl);

    std::vector<float> magnitudeSnapshot_;

    juce::uint32 fbo_ = 0;
    juce::uint32 fboTexture_ = 0;
    int fboWidth_ = 0, fboHeight_ = 0;

    juce::uint32 shaderProgram_ = 0;
    juce::uint32 spectrumTexture_ = 0;
    juce::uint32 quadVBO_ = 0;

    juce::String currentShaderSource_;
    bool shaderNeedsRecompile_ = true;
    bool shaderError_ = false;
    float time_ = 0.f;
    std::vector<float> processedSpectrum_;
    float audioLevel_ = 0.f;
    float bassLevel_ = 0.f;
    float midLevel_ = 0.f;
    float highLevel_ = 0.f;
    float pulseLevel_ = 0.f;
    float runningEnergy_ = 0.08f;
};

} // namespace pf
