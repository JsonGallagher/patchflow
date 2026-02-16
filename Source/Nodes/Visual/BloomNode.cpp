#include "Nodes/Visual/BloomNode.h"
#include <juce_opengl/juce_opengl.h>

namespace pf
{

namespace
{
juce::String getShaderInfoLog (juce::OpenGLContext& gl, juce::uint32 shader)
{
    GLint logLength = 0;
    gl.extensions.glGetShaderiv (shader, juce::gl::GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1)
        return {};

    juce::HeapBlock<GLchar> buffer (static_cast<size_t> (logLength));
    GLsizei outLength = 0;
    gl.extensions.glGetShaderInfoLog (shader, logLength, &outLength, buffer.getData());
    return juce::String (buffer.getData(), outLength);
}

juce::String getProgramInfoLog (juce::OpenGLContext& gl, juce::uint32 program)
{
    GLint logLength = 0;
    gl.extensions.glGetProgramiv (program, juce::gl::GL_INFO_LOG_LENGTH, &logLength);

    if (logLength <= 1)
        return {};

    juce::HeapBlock<GLchar> buffer (static_cast<size_t> (logLength));
    GLsizei outLength = 0;
    gl.extensions.glGetProgramInfoLog (program, logLength, &outLength, buffer.getData());
    return juce::String (buffer.getData(), outLength);
}

juce::uint32 compileShaderStage (juce::OpenGLContext& gl,
                                 GLenum stage,
                                 const juce::String& source,
                                 juce::String& outErrorLog)
{
    auto shader = gl.extensions.glCreateShader (stage);
    auto src = source.toRawUTF8();
    gl.extensions.glShaderSource (shader, 1, &src, nullptr);
    gl.extensions.glCompileShader (shader);

    GLint compiled = 0;
    gl.extensions.glGetShaderiv (shader, juce::gl::GL_COMPILE_STATUS, &compiled);
    if (! compiled)
    {
        outErrorLog = getShaderInfoLog (gl, shader);
        gl.extensions.glDeleteShader (shader);
        return 0;
    }

    outErrorLog = {};
    return shader;
}
} // namespace

void BloomNode::ensureFBO (juce::OpenGLContext& gl, int width, int height)
{
    if (fbo_ != 0 && fboWidth_ == width && fboHeight_ == height)
        return;

    if (fbo_ != 0)
    {
        gl.extensions.glDeleteFramebuffers (1, &fbo_);
        juce::gl::glDeleteTextures (1, &fboTexture_);
    }

    juce::gl::glGenTextures (1, &fboTexture_);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, fboTexture_);
    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                            width, height, 0,
                            juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE, nullptr);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);

    gl.extensions.glGenFramebuffers (1, &fbo_);
    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER,
                                          juce::gl::GL_COLOR_ATTACHMENT0,
                                          juce::gl::GL_TEXTURE_2D,
                                          fboTexture_,
                                          0);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    fboWidth_ = width;
    fboHeight_ = height;
}

void BloomNode::compileShader (juce::OpenGLContext& gl)
{
    if (shaderProgram_ != 0)
        return;

    const juce::String vertexShaderSource =
        "#if __VERSION__ >= 130\n"
        "#define attribute in\n"
        "#define varying out\n"
        "#endif\n"
        "attribute vec2 a_position;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_position * 0.5 + 0.5;\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n";

    const juce::String fragmentShaderSource =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "#if __VERSION__ >= 130\n"
        "#define varying in\n"
        "out vec4 pf_fragColor;\n"
        "#define gl_FragColor pf_fragColor\n"
        "#define texture2D texture\n"
        "#endif\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_texture;\n"
        "uniform vec2 u_texel;\n"
        "uniform float u_threshold;\n"
        "uniform float u_intensity;\n"
        "uniform float u_radius;\n"
        "\n"
        "float brightMask(vec3 c, float threshold) {\n"
        "    float luma = dot(c, vec3(0.299, 0.587, 0.114));\n"
        "    return max(0.0, luma - threshold) / max(0.0001, 1.0 - threshold);\n"
        "}\n"
        "\n"
        "void main() {\n"
        "    vec4 base = texture2D(u_texture, v_uv);\n"
        "\n"
        "    vec2 x = vec2(u_texel.x * u_radius, 0.0);\n"
        "    vec2 y = vec2(0.0, u_texel.y * u_radius);\n"
        "\n"
        "    vec3 accum = vec3(0.0);\n"
        "    float wSum = 0.0;\n"
        "\n"
        "    vec2 offsets[9];\n"
        "    offsets[0] = vec2(0.0, 0.0);\n"
        "    offsets[1] = x;\n"
        "    offsets[2] = -x;\n"
        "    offsets[3] = y;\n"
        "    offsets[4] = -y;\n"
        "    offsets[5] = x + y;\n"
        "    offsets[6] = x - y;\n"
        "    offsets[7] = -x + y;\n"
        "    offsets[8] = -x - y;\n"
        "\n"
        "    float weights[9];\n"
        "    weights[0] = 0.22;\n"
        "    weights[1] = 0.12;\n"
        "    weights[2] = 0.12;\n"
        "    weights[3] = 0.12;\n"
        "    weights[4] = 0.12;\n"
        "    weights[5] = 0.075;\n"
        "    weights[6] = 0.075;\n"
        "    weights[7] = 0.075;\n"
        "    weights[8] = 0.075;\n"
        "\n"
        "    for (int i = 0; i < 9; ++i) {\n"
        "        vec3 c = texture2D(u_texture, v_uv + offsets[i]).rgb;\n"
        "        float m = brightMask(c, u_threshold);\n"
        "        float w = weights[i] * m;\n"
        "        accum += c * w;\n"
        "        wSum += w;\n"
        "    }\n"
        "\n"
        "    vec3 bloom = (wSum > 0.0) ? (accum / wSum) : vec3(0.0);\n"
        "    vec3 outRgb = base.rgb + bloom * u_intensity;\n"
        "\n"
        "    gl_FragColor = vec4(clamp(outRgb, 0.0, 1.0), base.a);\n"
        "}\n";

    juce::String errorLog;
    auto vs = compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertexShaderSource, errorLog);
    if (vs == 0)
    {
        shaderError_ = true;
        DBG ("BloomNode vertex shader compile error:\n" + errorLog);
        return;
    }

    auto fs = compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragmentShaderSource, errorLog);
    if (fs == 0)
    {
        gl.extensions.glDeleteShader (vs);
        shaderError_ = true;
        DBG ("BloomNode fragment shader compile error:\n" + errorLog);
        return;
    }

    shaderProgram_ = gl.extensions.glCreateProgram();
    gl.extensions.glAttachShader (shaderProgram_, vs);
    gl.extensions.glAttachShader (shaderProgram_, fs);
    gl.extensions.glLinkProgram (shaderProgram_);

    GLint linked = 0;
    gl.extensions.glGetProgramiv (shaderProgram_, juce::gl::GL_LINK_STATUS, &linked);

    gl.extensions.glDeleteShader (vs);
    gl.extensions.glDeleteShader (fs);

    if (! linked)
    {
        auto linkLog = getProgramInfoLog (gl, shaderProgram_);
        gl.extensions.glDeleteProgram (shaderProgram_);
        shaderProgram_ = 0;
        shaderError_ = true;
        DBG ("BloomNode shader link error:\n" + linkLog);
        return;
    }

    shaderError_ = false;

    if (quadVBO_ == 0)
    {
        float quadVerts[] = {
            -1.f, -1.f,
             1.f, -1.f,
            -1.f,  1.f,
             1.f,  1.f
        };
        gl.extensions.glGenBuffers (1, &quadVBO_);
        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, quadVBO_);
        gl.extensions.glBufferData (juce::gl::GL_ARRAY_BUFFER, sizeof (quadVerts), quadVerts, juce::gl::GL_STATIC_DRAW);
        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
    }
}

void BloomNode::ensureFallbackTexture()
{
    if (fallbackTexture_ != 0)
        return;

    juce::gl::glGenTextures (1, &fallbackTexture_);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, fallbackTexture_);

    const juce::uint8 blackPixel[4] = { 0, 0, 0, 255 };
    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                            1, 1, 0,
                            juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE,
                            blackPixel);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);
}

void BloomNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512;
    constexpr int height = 512;

    ensureFBO (gl, width, height);
    compileShader (gl);
    ensureFallbackTexture();

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

        float threshold = getParamAsFloat ("threshold", 0.6f);
        float intensity = getParamAsFloat ("intensity", 0.9f);
        float radius = getParamAsFloat ("radius", 1.6f);

        if (isInputConnected (1))
            threshold += getConnectedVisualValue (1) - 0.5f;

        if (isInputConnected (2))
            intensity *= juce::jlimit (0.0f, 2.0f, getConnectedVisualValue (2) * 2.0f);

        if (isInputConnected (3))
            radius *= juce::jlimit (0.1f, 4.0f, getConnectedVisualValue (3) * 2.0f);

        threshold = juce::jlimit (0.0f, 1.0f, threshold);
        intensity = juce::jlimit (0.0f, 4.0f, intensity);
        radius = juce::jlimit (0.05f, 12.0f, radius);

        if (auto thresholdLoc = loc ("u_threshold"); thresholdLoc >= 0)
            gl.extensions.glUniform1f (thresholdLoc, threshold);

        if (auto intensityLoc = loc ("u_intensity"); intensityLoc >= 0)
            gl.extensions.glUniform1f (intensityLoc, intensity);

        if (auto radiusLoc = loc ("u_radius"); radiusLoc >= 0)
            gl.extensions.glUniform1f (radiusLoc, radius);

        if (auto texelLoc = loc ("u_texel"); texelLoc >= 0)
            gl.extensions.glUniform2f (texelLoc, 1.0f / static_cast<float> (width), 1.0f / static_cast<float> (height));

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D,
                                 isInputConnected (0) ? getConnectedTexture (0) : fallbackTexture_);
        if (auto texLoc = loc ("u_texture"); texLoc >= 0)
            gl.extensions.glUniform1i (texLoc, 0);

        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, quadVBO_);
        auto posAttrib = gl.extensions.glGetAttribLocation (shaderProgram_, "a_position");
        if (posAttrib >= 0)
        {
            gl.extensions.glEnableVertexAttribArray (static_cast<juce::uint32> (posAttrib));
            gl.extensions.glVertexAttribPointer (static_cast<juce::uint32> (posAttrib), 2,
                                                 juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);
            juce::gl::glDrawArrays (juce::gl::GL_TRIANGLE_STRIP, 0, 4);
            gl.extensions.glDisableVertexAttribArray (static_cast<juce::uint32> (posAttrib));
        }

        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
