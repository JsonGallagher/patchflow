#include "Nodes/Visual/FeedbackNode.h"
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

void FeedbackNode::ensureResources (juce::OpenGLContext& gl, int width, int height)
{
    if (fbos_[0] != 0 && fboWidth_ == width && fboHeight_ == height)
        return;

    if (fbos_[0] != 0)
    {
        gl.extensions.glDeleteFramebuffers (2, fbos_);
        juce::gl::glDeleteTextures (2, textures_);
        fbos_[0] = fbos_[1] = 0;
        textures_[0] = textures_[1] = 0;
    }

    juce::gl::glGenTextures (2, textures_);
    gl.extensions.glGenFramebuffers (2, fbos_);

    for (int i = 0; i < 2; ++i)
    {
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, textures_[i]);
        juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                                width, height, 0,
                                juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE, nullptr);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);

        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos_[i]);
        gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER,
                                              juce::gl::GL_COLOR_ATTACHMENT0,
                                              juce::gl::GL_TEXTURE_2D,
                                              textures_[i],
                                              0);

        // Start with a black history buffer.
        juce::gl::glViewport (0, 0, width, height);
        juce::gl::glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);

    fboWidth_ = width;
    fboHeight_ = height;
    latestIndex_ = 0;
}

void FeedbackNode::compileShader (juce::OpenGLContext& gl)
{
    if (shaderProgram_ != 0)
        return;

    const juce::String vertexShaderSource =
        "#if __VERSION__ >= 130\n"
        "#define attribute in\n"
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
        "out vec4 pf_fragColor;\n"
        "#define gl_FragColor pf_fragColor\n"
        "#define texture2D texture\n"
        "#endif\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_inputTex;\n"
        "uniform sampler2D u_prevTex;\n"
        "uniform float u_feedback;\n"
        "uniform float u_mix;\n"
        "uniform vec2 u_offset;\n"
        "uniform float u_zoom;\n"
        "uniform float u_rotation;\n"
        "uniform int u_wrap;\n"
        "\n"
        "vec4 samplePrev(vec2 uv) {\n"
        "    if (u_wrap == 1)\n"
        "        return texture2D(u_prevTex, fract(uv));\n"
        "\n"
        "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)\n"
        "        return vec4(0.0);\n"
        "\n"
        "    return texture2D(u_prevTex, uv);\n"
        "}\n"
        "\n"
        "void main() {\n"
        "    vec4 inputCol = texture2D(u_inputTex, v_uv);\n"
        "\n"
        "    vec2 p = v_uv - vec2(0.5);\n"
        "    float c = cos(u_rotation);\n"
        "    float s = sin(u_rotation);\n"
        "    mat2 rot = mat2(c, -s, s, c);\n"
        "\n"
        "    p = rot * (p / max(0.001, u_zoom));\n"
        "    p -= u_offset;\n"
        "    vec2 prevUv = p + vec2(0.5);\n"
        "\n"
        "    vec4 prevCol = samplePrev(prevUv);\n"
        "\n"
        "    vec3 trailed = inputCol.rgb + prevCol.rgb * u_feedback;\n"
        "    vec3 outRgb = mix(inputCol.rgb, trailed, u_mix);\n"
        "    float outA = max(inputCol.a, prevCol.a * u_feedback);\n"
        "\n"
        "    gl_FragColor = vec4(clamp(outRgb, 0.0, 1.0), outA);\n"
        "}\n";

    juce::String errorLog;
    auto vs = compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertexShaderSource, errorLog);
    if (vs == 0)
    {
        shaderError_ = true;
        DBG ("FeedbackNode vertex shader compile error:\n" + errorLog);
        return;
    }

    auto fs = compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragmentShaderSource, errorLog);
    if (fs == 0)
    {
        gl.extensions.glDeleteShader (vs);
        shaderError_ = true;
        DBG ("FeedbackNode fragment shader compile error:\n" + errorLog);
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
        DBG ("FeedbackNode shader link error:\n" + linkLog);
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

void FeedbackNode::ensureFallbackTexture()
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

void FeedbackNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512;
    constexpr int height = 512;

    ensureResources (gl, width, height);
    compileShader (gl);
    ensureFallbackTexture();

    const int prevIndex = latestIndex_;
    const int writeIndex = 1 - latestIndex_;

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos_[writeIndex]);
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

        float feedbackAmount = getParamAsFloat ("feedback", 0.82f);
        float offsetX = getParamAsFloat ("offsetX", 0.002f);
        float offsetY = getParamAsFloat ("offsetY", 0.001f);

        if (isInputConnected (1))
            feedbackAmount = juce::jlimit (0.0f, 0.99f, feedbackAmount + (getConnectedVisualValue (1) - 0.5f));
        else
            feedbackAmount = juce::jlimit (0.0f, 0.99f, feedbackAmount);

        if (isInputConnected (2))
            offsetX += getConnectedVisualValue (2) * 0.02f;
        if (isInputConnected (3))
            offsetY += getConnectedVisualValue (3) * 0.02f;

        const auto mixAmount = juce::jlimit (0.0f, 1.0f, getParamAsFloat ("mix", 1.0f));
        const auto zoom = juce::jlimit (0.5f, 2.0f, getParamAsFloat ("zoom", 1.0f));
        const auto rotationRad = juce::degreesToRadians (getParamAsFloat ("rotationDeg", 0.0f));

        if (auto feedbackLoc = loc ("u_feedback"); feedbackLoc >= 0)
            gl.extensions.glUniform1f (feedbackLoc, feedbackAmount);

        if (auto mixLoc = loc ("u_mix"); mixLoc >= 0)
            gl.extensions.glUniform1f (mixLoc, mixAmount);

        if (auto offsetLoc = loc ("u_offset"); offsetLoc >= 0)
            gl.extensions.glUniform2f (offsetLoc, offsetX, offsetY);

        if (auto zoomLoc = loc ("u_zoom"); zoomLoc >= 0)
            gl.extensions.glUniform1f (zoomLoc, zoom);

        if (auto rotationLoc = loc ("u_rotation"); rotationLoc >= 0)
            gl.extensions.glUniform1f (rotationLoc, rotationRad);

        if (auto wrapLoc = loc ("u_wrap"); wrapLoc >= 0)
            gl.extensions.glUniform1i (wrapLoc, getParamAsInt ("wrap", 1));

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D,
                                 isInputConnected (0) ? getConnectedTexture (0) : fallbackTexture_);
        if (auto inputLoc = loc ("u_inputTex"); inputLoc >= 0)
            gl.extensions.glUniform1i (inputLoc, 0);

        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE1);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, textures_[prevIndex]);
        if (auto prevLoc = loc ("u_prevTex"); prevLoc >= 0)
            gl.extensions.glUniform1i (prevLoc, 1);

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

    latestIndex_ = writeIndex;
    setTextureOutput (0, textures_[latestIndex_]);
}

} // namespace pf
