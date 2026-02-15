#include "Nodes/Visual/SDFShapeNode.h"
#include "Rendering/ShaderUtils.h"

namespace pf
{

void SDFShapeNode::renderFrame (juce::OpenGLContext& gl)
{
    constexpr int width = 512, height = 512;

    ShaderUtils::ensureFBO (gl, fbo_, fboTexture_, fboWidth_, fboHeight_, width, height);
    ShaderUtils::ensureQuadVBO (gl, quadVBO_);

    if (! shaderCompiled_ && ! shaderError_)
    {
        const auto vertSrc = ShaderUtils::getStandardVertexShader();
        const auto fragSrc = ShaderUtils::getFragmentPreamble() +
            "varying vec2 v_uv;\n"
            "uniform int   u_shape;\n"
            "uniform float u_radius;\n"
            "uniform float u_edgeSoftness;\n"
            "uniform float u_rotation;\n"
            "uniform float u_repeatX;\n"
            "uniform float u_repeatY;\n"
            "uniform float u_ringThickness;\n"
            "uniform int   u_starPoints;\n"
            "uniform int   u_fillColor;\n"
            "\n"
            "vec2 rotate2d(vec2 p, float a) {\n"
            "    float c = cos(a), s = sin(a);\n"
            "    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);\n"
            "}\n"
            "\n"
            "float sdCircle(vec2 p, float r) { return length(p) - r; }\n"
            "\n"
            "float sdRing(vec2 p, float r, float w) {\n"
            "    return abs(length(p) - r) - w;\n"
            "}\n"
            "\n"
            "float sdTriangle(vec2 p, float r) {\n"
            "    const float k = sqrt(3.0);\n"
            "    p.x = abs(p.x) - r;\n"
            "    p.y = p.y + r / k;\n"
            "    if (p.x + k * p.y > 0.0) p = vec2(p.x - k * p.y, -k * p.x - p.y) / 2.0;\n"
            "    p.x -= clamp(p.x, -2.0 * r, 0.0);\n"
            "    return -length(p) * sign(p.y);\n"
            "}\n"
            "\n"
            "float sdBox(vec2 p, float r) {\n"
            "    vec2 d = abs(p) - vec2(r);\n"
            "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
            "}\n"
            "\n"
            "float sdPentagon(vec2 p, float r) {\n"
            "    const vec3 k = vec3(0.809016994, 0.587785252, 0.726542528);\n"
            "    p.x = abs(p.x);\n"
            "    p -= 2.0 * min(dot(vec2(-k.x, k.y), p), 0.0) * vec2(-k.x, k.y);\n"
            "    p -= 2.0 * min(dot(vec2(k.x, k.y), p), 0.0) * vec2(k.x, k.y);\n"
            "    p -= vec2(clamp(p.x, -r * k.z, r * k.z), r);\n"
            "    return length(p) * sign(p.y);\n"
            "}\n"
            "\n"
            "float sdHexagon(vec2 p, float r) {\n"
            "    const vec3 k = vec3(-0.866025404, 0.5, 0.577350269);\n"
            "    p = abs(p);\n"
            "    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;\n"
            "    p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);\n"
            "    return length(p) * sign(p.y);\n"
            "}\n"
            "\n"
            "float sdStar(vec2 p, float r, int n, float rf) {\n"
            "    float an = 3.141593 / float(n);\n"
            "    float en = 3.141593 / (rf > 0.0 ? rf : 3.0);\n"
            "    vec2 acs = vec2(cos(an), sin(an));\n"
            "    vec2 ecs = vec2(cos(en), sin(en));\n"
            "    float bn = mod(atan(p.x, p.y), 2.0 * an) - an;\n"
            "    p = length(p) * vec2(cos(bn), abs(sin(bn)));\n"
            "    p -= r * acs;\n"
            "    p += ecs * clamp(-dot(p, ecs), 0.0, r * acs.y / ecs.y);\n"
            "    return length(p) * sign(p.x);\n"
            "}\n"
            "\n"
            "vec3 hsv2rgb(vec3 c) {\n"
            "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
            "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
            "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = v_uv * 2.0 - 1.0;\n"
            "\n"
            "    // Grid repetition\n"
            "    if (u_repeatX > 1.0 || u_repeatY > 1.0) {\n"
            "        uv = fract(v_uv * vec2(u_repeatX, u_repeatY)) * 2.0 - 1.0;\n"
            "    }\n"
            "\n"
            "    uv = rotate2d(uv, u_rotation);\n"
            "\n"
            "    float d = 0.0;\n"
            "    if (u_shape == 0) d = sdCircle(uv, u_radius);\n"
            "    else if (u_shape == 1) d = sdRing(uv, u_radius, u_ringThickness);\n"
            "    else if (u_shape == 2) d = sdTriangle(uv, u_radius);\n"
            "    else if (u_shape == 3) d = sdBox(uv, u_radius);\n"
            "    else if (u_shape == 4) d = sdPentagon(uv, u_radius);\n"
            "    else if (u_shape == 5) d = sdHexagon(uv, u_radius);\n"
            "    else d = sdStar(uv, u_radius, u_starPoints, 3.0);\n"
            "\n"
            "    float mask = 1.0 - smoothstep(0.0, u_edgeSoftness, d);\n"
            "\n"
            "    vec3 col;\n"
            "    if (u_fillColor == 0) col = vec3(1.0);\n"
            "    else if (u_fillColor == 1) col = mix(vec3(0.2, 0.5, 1.0), vec3(1.0, 0.3, 0.5), v_uv.y);\n"
            "    else if (u_fillColor == 2) col = hsv2rgb(vec3(atan(uv.y, uv.x) / 6.283 + 0.5, 0.8, 1.0));\n"
            "    else col = vec3(1.0 - clamp(abs(d) * 4.0, 0.0, 1.0));\n"
            "\n"
            "    gl_FragColor = vec4(col * mask, mask);\n"
            "}\n";

        juce::String errorLog;
        auto vs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_VERTEX_SHADER, vertSrc, errorLog);
        if (vs == 0) { shaderError_ = true; DBG ("SDFShapeNode VS error: " + errorLog); return; }

        auto fs = ShaderUtils::compileShaderStage (gl, juce::gl::GL_FRAGMENT_SHADER, fragSrc, errorLog);
        if (fs == 0) { gl.extensions.glDeleteShader (vs); shaderError_ = true; DBG ("SDFShapeNode FS error: " + errorLog); return; }

        shaderProgram_ = ShaderUtils::linkProgram (gl, vs, fs, errorLog);
        if (shaderProgram_ == 0) { shaderError_ = true; DBG ("SDFShapeNode link error: " + errorLog); return; }

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

        float radius = getParamAsFloat ("radius", 0.35f);
        float rotation = juce::degreesToRadians (getParamAsFloat ("rotation", 0.0f));
        float edgeSoftness = getParamAsFloat ("edgeSoftness", 0.01f);
        float repeatX = static_cast<float> (getParamAsInt ("repeatX", 1));
        float repeatY = static_cast<float> (getParamAsInt ("repeatY", 1));

        if (isInputConnected (0)) radius *= juce::jlimit (0.1f, 4.0f, getConnectedVisualValue (0) * 2.0f);
        if (isInputConnected (1)) rotation += getConnectedVisualValue (1) * 6.283f;
        if (isInputConnected (2)) edgeSoftness *= juce::jlimit (0.1f, 4.0f, getConnectedVisualValue (2) * 2.0f);
        if (isInputConnected (3)) { float rep = juce::jlimit (1.0f, 8.0f, getConnectedVisualValue (3) * 8.0f); repeatX = rep; repeatY = rep; }

        if (auto l = loc ("u_shape");          l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("shape", 0));
        if (auto l = loc ("u_radius");         l >= 0) gl.extensions.glUniform1f (l, radius);
        if (auto l = loc ("u_edgeSoftness");   l >= 0) gl.extensions.glUniform1f (l, edgeSoftness);
        if (auto l = loc ("u_rotation");       l >= 0) gl.extensions.glUniform1f (l, rotation);
        if (auto l = loc ("u_repeatX");        l >= 0) gl.extensions.glUniform1f (l, repeatX);
        if (auto l = loc ("u_repeatY");        l >= 0) gl.extensions.glUniform1f (l, repeatY);
        if (auto l = loc ("u_ringThickness");  l >= 0) gl.extensions.glUniform1f (l, getParamAsFloat ("ringThickness", 0.05f));
        if (auto l = loc ("u_starPoints");     l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("starPoints", 5));
        if (auto l = loc ("u_fillColor");      l >= 0) gl.extensions.glUniform1i (l, getParamAsInt ("fillColor", 0));

        ShaderUtils::drawFullscreenQuad (gl, shaderProgram_, quadVBO_);
        gl.extensions.glUseProgram (0);
    }

    gl.extensions.glBindFramebuffer (juce::gl::GL_FRAMEBUFFER, 0);
    setTextureOutput (0, fboTexture_);
}

} // namespace pf
