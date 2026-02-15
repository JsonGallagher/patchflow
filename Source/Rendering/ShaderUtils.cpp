#include "Rendering/ShaderUtils.h"

namespace pf
{
namespace ShaderUtils
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

juce::uint32 linkProgram (juce::OpenGLContext& gl,
                          juce::uint32 vs,
                          juce::uint32 fs,
                          juce::String& outErrorLog)
{
    auto program = gl.extensions.glCreateProgram();
    gl.extensions.glAttachShader (program, vs);
    gl.extensions.glAttachShader (program, fs);
    gl.extensions.glLinkProgram (program);

    GLint linked = 0;
    gl.extensions.glGetProgramiv (program, juce::gl::GL_LINK_STATUS, &linked);

    gl.extensions.glDeleteShader (vs);
    gl.extensions.glDeleteShader (fs);

    if (! linked)
    {
        outErrorLog = getProgramInfoLog (gl, program);
        gl.extensions.glDeleteProgram (program);
        return 0;
    }

    outErrorLog = {};
    return program;
}

void ensureFBO (juce::OpenGLContext& gl,
                juce::uint32& fbo, juce::uint32& fboTexture,
                int& fboWidth, int& fboHeight,
                int width, int height)
{
    if (fbo != 0 && fboWidth == width && fboHeight == height)
        return;

    if (fbo != 0)
    {
        gl.extensions.glDeleteFramebuffers (1, &fbo);
        juce::gl::glDeleteTextures (1, &fboTexture);
    }

    juce::gl::glGenTextures (1, &fboTexture);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, fboTexture);
    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                            width, height, 0,
                            juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE, nullptr);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);

    gl.extensions.glGenFramebuffers (1, &fbo);
    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo);
    gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER,
                                          juce::gl::GL_COLOR_ATTACHMENT0,
                                          juce::gl::GL_TEXTURE_2D,
                                          fboTexture, 0);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    fboWidth = width;
    fboHeight = height;
}

void ensurePingPongFBOs (juce::OpenGLContext& gl,
                         juce::uint32 fbos[2], juce::uint32 textures[2],
                         int& fboWidth, int& fboHeight,
                         int width, int height)
{
    if (fbos[0] != 0 && fboWidth == width && fboHeight == height)
        return;

    if (fbos[0] != 0)
    {
        gl.extensions.glDeleteFramebuffers (2, fbos);
        juce::gl::glDeleteTextures (2, textures);
        fbos[0] = fbos[1] = 0;
        textures[0] = textures[1] = 0;
    }

    juce::gl::glGenTextures (2, textures);
    gl.extensions.glGenFramebuffers (2, fbos);

    for (int i = 0; i < 2; ++i)
    {
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, textures[i]);
        juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                                width, height, 0,
                                juce::gl::GL_RGBA, juce::gl::GL_UNSIGNED_BYTE, nullptr);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);

        gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbos[i]);
        gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER,
                                              juce::gl::GL_COLOR_ATTACHMENT0,
                                              juce::gl::GL_TEXTURE_2D,
                                              textures[i], 0);

        juce::gl::glViewport (0, 0, width, height);
        juce::gl::glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    fboWidth = width;
    fboHeight = height;
}

void ensureQuadVBO (juce::OpenGLContext& gl, juce::uint32& quadVBO)
{
    if (quadVBO != 0)
        return;

    float quadVerts[] = {
        -1.f, -1.f,
         1.f, -1.f,
        -1.f,  1.f,
         1.f,  1.f
    };
    gl.extensions.glGenBuffers (1, &quadVBO);
    gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, quadVBO);
    gl.extensions.glBufferData (juce::gl::GL_ARRAY_BUFFER, sizeof (quadVerts), quadVerts, juce::gl::GL_STATIC_DRAW);
    gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

void ensureFallbackTexture (juce::uint32& fallbackTexture)
{
    if (fallbackTexture != 0)
        return;

    juce::gl::glGenTextures (1, &fallbackTexture);
    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, fallbackTexture);

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

void drawFullscreenQuad (juce::OpenGLContext& gl,
                         juce::uint32 shaderProgram,
                         juce::uint32 quadVBO)
{
    gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, quadVBO);
    auto posAttrib = gl.extensions.glGetAttribLocation (shaderProgram, "a_position");
    if (posAttrib >= 0)
    {
        gl.extensions.glEnableVertexAttribArray (static_cast<juce::uint32> (posAttrib));
        gl.extensions.glVertexAttribPointer (static_cast<juce::uint32> (posAttrib), 2,
                                             juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);
        juce::gl::glDrawArrays (juce::gl::GL_TRIANGLE_STRIP, 0, 4);
        gl.extensions.glDisableVertexAttribArray (static_cast<juce::uint32> (posAttrib));
    }
    gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

juce::String getStandardVertexShader()
{
    return
        "#if __VERSION__ >= 130\n"
        "#define attribute in\n"
        "#endif\n"
        "attribute vec2 a_position;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_position * 0.5 + 0.5;\n"
        "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}\n";
}

juce::String getFragmentPreamble()
{
    return
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "#if __VERSION__ >= 130\n"
        "out vec4 pf_fragColor;\n"
        "#define gl_FragColor pf_fragColor\n"
        "#define texture2D texture\n"
        "#endif\n";
}

} // namespace ShaderUtils
} // namespace pf
