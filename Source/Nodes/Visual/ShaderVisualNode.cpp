#include "Nodes/Visual/ShaderVisualNode.h"

namespace pf
{

void ShaderVisualNode::ensureFBO (juce::OpenGLContext& gl, int width, int height)
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

    gl.extensions.glGenFramebuffers (1, &fbo_);
    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    gl.extensions.glFramebufferTexture2D (juce::gl::GL_FRAMEBUFFER, juce::gl::GL_COLOR_ATTACHMENT0,
                                          juce::gl::GL_TEXTURE_2D, fboTexture_, 0);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    fboWidth_ = width;
    fboHeight_ = height;
}

void ShaderVisualNode::compileShader (juce::OpenGLContext& gl)
{
    juce::String fragSource = getParam ("fragmentShader").toString();
    if (fragSource.isEmpty())
        fragSource = getDefaultFragmentShader();

    if (fragSource == currentShaderSource_ && ! shaderNeedsRecompile_)
        return;

    currentShaderSource_ = fragSource;
    shaderNeedsRecompile_ = false;

    // Clean up old shader
    if (shaderProgram_ != 0)
    {
        gl.extensions.glDeleteProgram (shaderProgram_);
        shaderProgram_ = 0;
    }

    // Compile vertex shader
    auto vs = gl.extensions.glCreateShader (juce::gl::GL_VERTEX_SHADER);
    auto vsSource = getDefaultVertexShader().toRawUTF8();
    gl.extensions.glShaderSource (vs, 1, &vsSource, nullptr);
    gl.extensions.glCompileShader (vs);

    GLint compiled = 0;
    gl.extensions.glGetShaderiv (vs, juce::gl::GL_COMPILE_STATUS, &compiled);
    if (! compiled)
    {
        gl.extensions.glDeleteShader (vs);
        shaderError_ = true;
        return;
    }

    // Compile fragment shader
    auto fs = gl.extensions.glCreateShader (juce::gl::GL_FRAGMENT_SHADER);
    auto fsSource = fragSource.toRawUTF8();
    gl.extensions.glShaderSource (fs, 1, &fsSource, nullptr);
    gl.extensions.glCompileShader (fs);

    gl.extensions.glGetShaderiv (fs, juce::gl::GL_COMPILE_STATUS, &compiled);
    if (! compiled)
    {
        gl.extensions.glDeleteShader (vs);
        gl.extensions.glDeleteShader (fs);
        shaderError_ = true;
        DBG ("ShaderVisual: Fragment shader compile error");
        return;
    }

    // Link program
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
        gl.extensions.glDeleteProgram (shaderProgram_);
        shaderProgram_ = 0;
        shaderError_ = true;
        return;
    }

    shaderError_ = false;

    // Create quad VBO if needed
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

void ShaderVisualNode::updateSpectrumTexture (juce::OpenGLContext& /*gl*/)
{
    if (spectrumTexture_ == 0)
    {
        juce::gl::glGenTextures (1, &spectrumTexture_);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, spectrumTexture_);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);
    }

    if (! magnitudeSnapshot_.empty())
    {
        int numBins = static_cast<int> (magnitudeSnapshot_.size());
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, spectrumTexture_);
        juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_R32F,
                                numBins, 1, 0,
                                juce::gl::GL_RED, juce::gl::GL_FLOAT,
                                magnitudeSnapshot_.data());
    }
}

void ShaderVisualNode::renderFrame (juce::OpenGLContext& gl)
{
    int width = 512, height = 512;
    ensureFBO (gl, width, height);
    compileShader (gl);
    updateSpectrumTexture (gl);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    juce::gl::glViewport (0, 0, width, height);

    if (shaderError_ || shaderProgram_ == 0)
    {
        // Error color — magenta
        juce::gl::glClearColor (0.5f, 0.0f, 0.5f, 1.0f);
        juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);
    }
    else
    {
        gl.extensions.glUseProgram (shaderProgram_);

        time_ += 1.0f / 60.0f;

        // Set uniforms
        auto loc = [&] (const char* name) {
            return gl.extensions.glGetUniformLocation (shaderProgram_, name);
        };

        gl.extensions.glUniform1f (loc ("u_time"), time_);
        gl.extensions.glUniform2f (loc ("u_resolution"),
                                   static_cast<float> (width),
                                   static_cast<float> (height));
        gl.extensions.glUniform1f (loc ("u_param1"), getConnectedVisualValue (0));
        gl.extensions.glUniform1f (loc ("u_param2"), getConnectedVisualValue (1));
        gl.extensions.glUniform1f (loc ("u_param3"), getConnectedVisualValue (2));
        gl.extensions.glUniform1f (loc ("u_param4"), getConnectedVisualValue (3));

        // Bind spectrum texture
        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, spectrumTexture_);
        gl.extensions.glUniform1i (loc ("u_spectrum"), 0);

        // Bind input texture if connected
        if (isInputConnected (5))
        {
            juce::gl::glActiveTexture (juce::gl::GL_TEXTURE1);
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, getConnectedTexture (5));
            gl.extensions.glUniform1i (loc ("u_texture"), 1);
        }

        // Draw fullscreen quad
        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, quadVBO_);
        auto posAttrib = gl.extensions.glGetAttribLocation (shaderProgram_, "a_position");
        gl.extensions.glEnableVertexAttribArray (posAttrib);
        gl.extensions.glVertexAttribPointer (posAttrib, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 0, nullptr);

        juce::gl::glDrawArrays (juce::gl::GL_TRIANGLE_STRIP, 0, 4);

        gl.extensions.glDisableVertexAttribArray (posAttrib);
        gl.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
