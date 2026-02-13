#include "Nodes/Visual/SpectrumRendererNode.h"
#include <juce_opengl/juce_opengl.h>
#include <cmath>

namespace pf
{

namespace
{
float magnitudeToDisplay (float magnitude, float dbRangeParam)
{
    const auto dB = magnitude > 1.0e-7f ? 20.0f * std::log10 (magnitude) : -96.0f;
    const auto dbRange = juce::jlimit (20.0f, 96.0f, std::abs (dbRangeParam));
    auto normalized = juce::jlimit (0.0f, 1.0f, (dB + dbRange) / dbRange);
    normalized = std::pow (normalized, 1.25f);
    return normalized;
}

int mapBarToBin (int barIndex, int numBars, int numBins, int scaleMode)
{
    auto t = (static_cast<float> (barIndex) + 0.5f) / static_cast<float> (numBars);
    if (scaleMode == 1)
        t = std::pow (t, 2.35f); // "Log-like" density toward bass frequencies.

    return juce::jlimit (0, numBins - 1, static_cast<int> (t * static_cast<float> (numBins - 1)));
}

float averageMagnitudeWindow (const std::vector<float>& bins, int centreBin, int radius)
{
    const auto numBins = static_cast<int> (bins.size());
    const auto start = juce::jlimit (0, numBins - 1, centreBin - radius);
    const auto end = juce::jlimit (start + 1, numBins, centreBin + radius + 1);

    float sum = 0.0f;
    for (int i = start; i < end; ++i)
        sum += bins[static_cast<size_t> (i)];

    return sum / static_cast<float> (end - start);
}
} // namespace

void SpectrumRendererNode::ensureFBO (juce::OpenGLContext& gl, int width, int height)
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

void SpectrumRendererNode::renderFrame (juce::OpenGLContext& gl)
{
    int width = 512, height = 256;
    ensureFBO (gl, width, height);

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, fbo_);
    juce::gl::glViewport (0, 0, width, height);

    juce::gl::glClearColor (0.02f, 0.03f, 0.07f, 1.0f);
    juce::gl::glClear (juce::gl::GL_COLOR_BUFFER_BIT);

    // Vertical gradient backdrop for more depth.
    juce::gl::glBegin (juce::gl::GL_QUADS);
    juce::gl::glColor4f (0.015f, 0.018f, 0.045f, 1.0f);
    juce::gl::glVertex2f (-1.0f, -1.0f);
    juce::gl::glVertex2f ( 1.0f, -1.0f);
    juce::gl::glColor4f (0.03f, 0.045f, 0.09f, 1.0f);
    juce::gl::glVertex2f ( 1.0f,  1.0f);
    juce::gl::glVertex2f (-1.0f,  1.0f);
    juce::gl::glEnd();

    const auto baseR = juce::jlimit (0.0f, 1.2f, isInputConnected (1) ? getConnectedVisualValue (1) : 0.2f);
    const auto baseG = juce::jlimit (0.0f, 1.2f, isInputConnected (2) ? getConnectedVisualValue (2) : 0.9f);
    const auto baseB = juce::jlimit (0.0f, 1.2f, isInputConnected (3) ? getConnectedVisualValue (3) : 0.4f);

    if (! magnitudeSnapshot_.empty() && magnitudeSnapshot_.size() > 4)
    {
        int numBins = static_cast<int> (magnitudeSnapshot_.size());
        const int numBars = juce::jlimit (8, 160, juce::jmin (numBins, 96));
        const int scaleMode = getParamAsInt ("scale", 0);
        const int style = getParamAsInt ("barStyle", 0);
        const float dbRange = getParamAsFloat ("dbRange", -60.0f);

        if (static_cast<int> (smoothedBars_.size()) != numBars)
        {
            smoothedBars_.assign (static_cast<size_t> (numBars), 0.0f);
            peakBars_.assign (static_cast<size_t> (numBars), 0.0f);
        }

        std::vector<float> displayBars (static_cast<size_t> (numBars), 0.0f);
        std::vector<float> peakCaps (static_cast<size_t> (numBars), 0.0f);

        const auto neighbourhood = juce::jmax (1, numBins / (numBars * 3));
        for (int i = 0; i < numBars; ++i)
        {
            const auto bin = mapBarToBin (i, numBars, numBins, scaleMode);
            const auto mag = averageMagnitudeWindow (magnitudeSnapshot_, bin, neighbourhood);
            const auto target = magnitudeToDisplay (mag, dbRange);

            const auto previous = smoothedBars_[static_cast<size_t> (i)];
            const auto smoothed = target > previous
                                ? previous + (target - previous) * 0.48f
                                : previous + (target - previous) * 0.16f;

            smoothedBars_[static_cast<size_t> (i)] = smoothed;
            auto& peak = peakBars_[static_cast<size_t> (i)];
            peak = juce::jmax (smoothed, peak * 0.94f);

            displayBars[static_cast<size_t> (i)] = juce::jmax (smoothed, peak * 0.55f);
            peakCaps[static_cast<size_t> (i)] = peak;
        }

        juce::gl::glEnable (juce::gl::GL_BLEND);
        juce::gl::glBlendFunc (juce::gl::GL_SRC_ALPHA, juce::gl::GL_ONE_MINUS_SRC_ALPHA);

        if (style == 2)
        {
            juce::gl::glBegin (juce::gl::GL_TRIANGLE_STRIP);
            for (int i = 0; i < numBars; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (numBars - 1);
                const auto x = t * 2.0f - 1.0f;
                const auto v = displayBars[static_cast<size_t> (i)];
                const auto y = -1.0f + v * 2.0f;
                const auto tint = 0.35f + 0.65f * v;

                const auto rr = juce::jlimit (0.0f, 1.0f, baseR * tint + (1.0f - t) * 0.12f);
                const auto gg = juce::jlimit (0.0f, 1.0f, baseG * tint + t * 0.08f);
                const auto bb = juce::jlimit (0.0f, 1.0f, baseB * tint + 0.12f);

                juce::gl::glColor4f (rr * 0.22f, gg * 0.24f, bb * 0.3f, 0.08f);
                juce::gl::glVertex2f (x, -1.0f);
                juce::gl::glColor4f (rr, gg, bb, 0.78f);
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();
        }
        else
        {
            juce::gl::glBegin (juce::gl::GL_QUADS);
            for (int i = 0; i < numBars; ++i)
            {
                const auto t0 = static_cast<float> (i) / static_cast<float> (numBars);
                const auto t1 = static_cast<float> (i + 1) / static_cast<float> (numBars);
                auto x0 = t0 * 2.0f - 1.0f;
                auto x1 = t1 * 2.0f - 1.0f;
                const auto widthNorm = x1 - x0;
                const auto gap = widthNorm * 0.18f;
                x0 += gap;
                x1 -= gap;

                const auto v = displayBars[static_cast<size_t> (i)];
                const auto y0 = -1.0f;
                const auto y1 = -1.0f + v * 2.0f;
                const auto tint = 0.28f + 0.72f * v;

                const auto rr = juce::jlimit (0.0f, 1.0f, baseR * tint + (1.0f - t0) * 0.14f);
                const auto gg = juce::jlimit (0.0f, 1.0f, baseG * tint + t0 * 0.07f);
                const auto bb = juce::jlimit (0.0f, 1.0f, baseB * tint + 0.1f);

                juce::gl::glColor4f (rr * 0.18f, gg * 0.2f, bb * 0.24f, 0.35f);
                juce::gl::glVertex2f (x0, y0);
                juce::gl::glVertex2f (x1, y0);
                juce::gl::glColor4f (rr, gg, bb, 0.95f);
                juce::gl::glVertex2f (x1, y1);
                juce::gl::glVertex2f (x0, y1);
            }
            juce::gl::glEnd();
        }

        if (style == 1 || style == 2)
        {
            juce::gl::glLineWidth (4.0f);
            juce::gl::glColor4f (baseR, baseG, baseB, 0.25f);
            juce::gl::glBegin (juce::gl::GL_LINE_STRIP);
            for (int i = 0; i < numBars; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (numBars - 1);
                const auto x = t * 2.0f - 1.0f;
                const auto y = -1.0f + displayBars[static_cast<size_t> (i)] * 2.0f;
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();

            juce::gl::glLineWidth (2.0f);
            juce::gl::glColor4f (juce::jlimit (0.0f, 1.0f, baseR + 0.08f),
                                 juce::jlimit (0.0f, 1.0f, baseG + 0.08f),
                                 juce::jlimit (0.0f, 1.0f, baseB + 0.08f),
                                 0.95f);
            juce::gl::glBegin (juce::gl::GL_LINE_STRIP);
            for (int i = 0; i < numBars; ++i)
            {
                const auto t = static_cast<float> (i) / static_cast<float> (numBars - 1);
                const auto x = t * 2.0f - 1.0f;
                const auto y = -1.0f + displayBars[static_cast<size_t> (i)] * 2.0f;
                juce::gl::glVertex2f (x, y);
            }
            juce::gl::glEnd();
        }

        juce::gl::glLineWidth (1.2f);
        juce::gl::glColor4f (0.95f, 0.95f, 1.0f, 0.55f);
        juce::gl::glBegin (juce::gl::GL_LINES);
        for (int i = 0; i < numBars; ++i)
        {
            const auto t = static_cast<float> (i) / static_cast<float> (numBars - 1);
            const auto x = t * 2.0f - 1.0f;
            const auto y = -1.0f + peakCaps[static_cast<size_t> (i)] * 2.0f;
            juce::gl::glVertex2f (x - 0.006f, y);
            juce::gl::glVertex2f (x + 0.006f, y);
        }
        juce::gl::glEnd();

        juce::gl::glDisable (juce::gl::GL_BLEND);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
