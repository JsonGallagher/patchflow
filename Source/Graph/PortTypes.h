#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace pf
{

enum class PortType
{
    Audio,    // float* buffer (blockSize samples)
    Signal,   // float scalar (control-rate, per audio block)
    Buffer,   // span<float> (FFT bins, band arrays)
    Visual,   // float / colour / vec (frame-rate)
    Texture   // GLuint handle (frame-rate)
};

enum class PortDirection
{
    Input,
    Output
};

inline juce::String portTypeToString (PortType t)
{
    switch (t)
    {
        case PortType::Audio:   return "Audio";
        case PortType::Signal:  return "Signal";
        case PortType::Buffer:  return "Buffer";
        case PortType::Visual:  return "Visual";
        case PortType::Texture: return "Texture";
    }
    return "Unknown";
}

inline PortType portTypeFromString (const juce::String& s)
{
    if (s == "Audio")   return PortType::Audio;
    if (s == "Signal")  return PortType::Signal;
    if (s == "Buffer")  return PortType::Buffer;
    if (s == "Visual")  return PortType::Visual;
    if (s == "Texture") return PortType::Texture;
    return PortType::Signal;
}

/** Returns true if a connection from sourceType to destType is allowed. */
inline bool canConnect (PortType source, PortType dest)
{
    if (source == dest) return true;
    // Signal can auto-connect to Visual (implicit rate hold)
    if (source == PortType::Signal && dest == PortType::Visual) return true;
    return false;
}

inline juce::Colour getPortColour (PortType t)
{
    switch (t)
    {
        case PortType::Audio:   return juce::Colour (0xff4fc3f7); // blue
        case PortType::Signal:  return juce::Colour (0xff81c784); // green
        case PortType::Buffer:  return juce::Colour (0xffffb74d); // orange
        case PortType::Visual:  return juce::Colour (0xffce93d8); // purple
        case PortType::Texture: return juce::Colour (0xffef5350); // red
    }
    return juce::Colours::grey;
}

} // namespace pf
