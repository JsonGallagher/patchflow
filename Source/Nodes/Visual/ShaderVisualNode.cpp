#include "Nodes/Visual/ShaderVisualNode.h"
#include <cmath>

namespace pf
{

namespace
{
float remapMagnitudeForVisuals (float magnitude)
{
    // Raw FFT magnitudes are very small, so we boost and compress them to a useful visual range.
    const auto boosted = std::log1p (juce::jmax (0.0f, magnitude) * 420.0f) * 0.72f;
    return juce::jlimit (0.0f, 2.0f, boosted);
}

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
    auto requestedFragSource = getParam ("fragmentShader").toString();
    if (requestedFragSource.isEmpty())
        requestedFragSource = getDefaultFragmentShader();

    if (requestedFragSource == currentShaderSource_ && ! shaderNeedsRecompile_)
        return;

    currentShaderSource_ = requestedFragSource;
    shaderNeedsRecompile_ = false;

    // Clean up old shader
    if (shaderProgram_ != 0)
    {
        gl.extensions.glDeleteProgram (shaderProgram_);
        shaderProgram_ = 0;
    }

    const juce::String vertexShaderSource =
        "#if __VERSION__ >= 130\n"
        "#define attribute in\n"
        "#endif\n"
        + getDefaultVertexShader();

    const auto makeFragmentSource = [] (const juce::String& body)
    {
        return juce::String (
            "#ifdef GL_ES\n"
            "precision mediump float;\n"
            "#endif\n"
            "#if __VERSION__ >= 130\n"
            "out vec4 pf_fragColor;\n"
            "#define gl_FragColor pf_fragColor\n"
            "#define texture2D texture\n"
            "#endif\n")
            + body;
    };

    juce::String stageErrorLog;
    auto vs = compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertexShaderSource, stageErrorLog);
    if (vs == 0)
    {
        shaderError_ = true;
        DBG ("ShaderVisual vertex shader compile error:\n" + stageErrorLog);
        return;
    }

    juce::String fragmentSourceToUse = requestedFragSource;
    auto fs = compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, makeFragmentSource (fragmentSourceToUse), stageErrorLog);

    if (fs == 0 && requestedFragSource != getDefaultFragmentShader())
    {
        DBG ("ShaderVisual custom fragment shader compile error. Falling back to default.\n" + stageErrorLog);
        fragmentSourceToUse = getDefaultFragmentShader();
        fs = compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, makeFragmentSource (fragmentSourceToUse), stageErrorLog);
    }

    if (fs == 0)
    {
        gl.extensions.glDeleteShader (vs);
        shaderError_ = true;
        DBG ("ShaderVisual fragment shader compile error:\n" + stageErrorLog);
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
        auto linkLog = getProgramInfoLog (gl, shaderProgram_);
        gl.extensions.glDeleteProgram (shaderProgram_);
        shaderProgram_ = 0;
        shaderError_ = true;
        DBG ("ShaderVisual shader link error:\n" + linkLog);
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
        if (static_cast<int> (processedSpectrum_.size()) != numBins)
            processedSpectrum_.assign (static_cast<size_t> (numBins), 0.0f);

        const auto attack = 0.38f;
        const auto release = 0.11f;

        float bassSum = 0.0f, midSum = 0.0f, highSum = 0.0f;
        int bassCount = 0, midCount = 0, highCount = 0;

        const int bassEnd = juce::jmax (1, numBins / 18);
        const int midEnd = juce::jmax (bassEnd + 1, numBins / 4);

        for (int i = 0; i < numBins; ++i)
        {
            const auto target = remapMagnitudeForVisuals (magnitudeSnapshot_[i]);
            const auto previous = processedSpectrum_[i];

            float smoothed = previous;
            if (target > previous)
                smoothed = previous + (target - previous) * attack;
            else
                smoothed = previous + (target - previous) * release;

            processedSpectrum_[i] = smoothed;

            if (i < bassEnd)
            {
                bassSum += smoothed;
                ++bassCount;
            }
            else if (i < midEnd)
            {
                midSum += smoothed;
                ++midCount;
            }
            else
            {
                highSum += smoothed;
                ++highCount;
            }
        }

        const auto safeAverage = [] (float sum, int count)
        {
            return count > 0 ? sum / static_cast<float> (count) : 0.0f;
        };

        bassLevel_ = safeAverage (bassSum, bassCount);
        midLevel_ = safeAverage (midSum, midCount);
        highLevel_ = safeAverage (highSum, highCount);

        const auto instantEnergy = bassLevel_ * 1.35f + midLevel_ + highLevel_ * 0.75f;
        runningEnergy_ = runningEnergy_ * 0.985f + instantEnergy * 0.015f;

        const auto normalizedEnergy = instantEnergy / juce::jmax (0.06f, runningEnergy_);
        audioLevel_ = audioLevel_ * 0.82f + normalizedEnergy * 0.18f;

        const auto pulseTarget = juce::jlimit (0.0f, 3.0f, normalizedEnergy * 1.35f + bassLevel_ * 0.55f);
        if (pulseTarget > pulseLevel_)
            pulseLevel_ = pulseLevel_ * 0.55f + pulseTarget * 0.45f;
        else
            pulseLevel_ = pulseLevel_ * 0.92f + pulseTarget * 0.08f;

        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, spectrumTexture_);
        juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_R32F,
                                numBins, 1, 0,
                                juce::gl::GL_RED, juce::gl::GL_FLOAT,
                                processedSpectrum_.data());
    }
    else
    {
        const float zero = 0.0f;
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, spectrumTexture_);
        juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_R32F,
                                1, 1, 0,
                                juce::gl::GL_RED, juce::gl::GL_FLOAT,
                                &zero);

        bassLevel_ *= 0.96f;
        midLevel_ *= 0.96f;
        highLevel_ *= 0.96f;
        audioLevel_ *= 0.94f;
        pulseLevel_ *= 0.90f;
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

        const auto setUniform1f = [&] (const char* name, float value)
        {
            auto location = loc (name);
            if (location >= 0)
                gl.extensions.glUniform1f (location, value);
        };

        const auto setUniform2f = [&] (const char* name, float x, float y)
        {
            auto location = loc (name);
            if (location >= 0)
                gl.extensions.glUniform2f (location, x, y);
        };

        setUniform1f ("u_time", time_);
        setUniform2f ("u_resolution", static_cast<float> (width), static_cast<float> (height));
        setUniform1f ("u_param1", getConnectedVisualValue (0));
        setUniform1f ("u_param2", getConnectedVisualValue (1));
        setUniform1f ("u_param3", getConnectedVisualValue (2));
        setUniform1f ("u_param4", getConnectedVisualValue (3));
        setUniform1f ("u_audioLevel", audioLevel_);
        setUniform1f ("u_bassLevel", bassLevel_);
        setUniform1f ("u_midLevel", midLevel_);
        setUniform1f ("u_highLevel", highLevel_);
        setUniform1f ("u_pulse", pulseLevel_);

        // Bind spectrum texture
        juce::gl::glActiveTexture (juce::gl::GL_TEXTURE0);
        juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, spectrumTexture_);
        auto spectrumLoc = loc ("u_spectrum");
        if (spectrumLoc >= 0)
            gl.extensions.glUniform1i (spectrumLoc, 0);

        // Bind input texture if connected
        if (isInputConnected (5))
        {
            juce::gl::glActiveTexture (juce::gl::GL_TEXTURE1);
            juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, getConnectedTexture (5));
            auto inputTexLoc = loc ("u_texture");
            if (inputTexLoc >= 0)
                gl.extensions.glUniform1i (inputTexLoc, 1);
        }

        // Draw fullscreen quad
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
