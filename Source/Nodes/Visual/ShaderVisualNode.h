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
        addParam  ("fragmentShader", getDefaultFragmentShader());
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
            "uniform sampler2D u_spectrum;\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = gl_FragCoord.xy / u_resolution;\n"
            "    float spec = texture2D(u_spectrum, vec2(uv.x, 0.0)).r;\n"
            "    float wave = sin(uv.x * 10.0 + u_time * 2.0 + u_param1 * 6.28) * 0.3;\n"
            "    float d = abs(uv.y - 0.5 - wave * spec);\n"
            "    float glow = 0.01 / (d + 0.01);\n"
            "    vec3 col = vec3(u_param2 + 0.3, u_param3 + 0.5, 1.0) * glow * 0.15;\n"
            "    col += vec3(spec * 0.2, spec * 0.1, spec * 0.3) * (1.0 - uv.y);\n"
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
};

} // namespace pf
