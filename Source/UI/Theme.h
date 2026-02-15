#pragma once
#include <juce_graphics/juce_graphics.h>

namespace pf
{
namespace Theme
{

//==============================================================================
// Background hierarchy
inline constexpr juce::uint32 kBgPrimary   = 0xff1a1a2a;
inline constexpr juce::uint32 kBgSecondary = 0xff1e1e2e;
inline constexpr juce::uint32 kBgSurface   = 0xff2a2a3a;
inline constexpr juce::uint32 kBgHover     = 0xff3a3a5a;
inline constexpr juce::uint32 kBgInput     = 0xff151524;

//==============================================================================
// Borders
inline constexpr juce::uint32 kBorderSubtle = 0xff333344;
inline constexpr juce::uint32 kBorderNormal = 0xff555566;
inline constexpr juce::uint32 kBorderFocus  = 0xff64b5f6;
inline constexpr juce::uint32 kBorderInput  = 0xff34344a;

//==============================================================================
// Text
inline constexpr juce::uint32 kTextPrimary   = 0xffffffff;
inline constexpr juce::uint32 kTextSecondary = 0xffcccccc;
inline constexpr juce::uint32 kTextMuted     = 0xffaaaacc;
inline constexpr juce::uint32 kTextDim       = 0xff888899;
inline constexpr juce::uint32 kTextPlaceholder = 0xff666677;

//==============================================================================
// Node header colours by category
inline constexpr juce::uint32 kHeaderAudio   = 0xff2196f3;
inline constexpr juce::uint32 kHeaderMath    = 0xff4caf50;
inline constexpr juce::uint32 kHeaderVisual  = 0xff9c27b0;
inline constexpr juce::uint32 kHeaderDefault = 0xff607d8b;

//==============================================================================
// Grid
inline constexpr juce::uint32 kGridMinor = 0xff252535;
inline constexpr juce::uint32 kGridMajor = 0xff303045;

//==============================================================================
// Accent
inline constexpr juce::uint32 kAccentBlue   = 0xff4fc3f7;
inline constexpr juce::uint32 kAccentPurple = 0xff9c27b0;

//==============================================================================
// Inspector-specific
inline constexpr juce::uint32 kSliderBackground = 0xff2a2a3a;
inline constexpr juce::uint32 kSliderTrack      = 0xff4fc3f7;
inline constexpr juce::uint32 kSliderThumb      = 0xffffffff;
inline constexpr juce::uint32 kSliderTextBg     = 0xff1a1a2a;

inline constexpr juce::uint32 kGroupSeparator   = 0xff3a3a4a;
inline constexpr juce::uint32 kGroupLabel        = 0xff7777aa;

//==============================================================================
// Preset row
inline constexpr juce::uint32 kPresetRowBg     = 0xff222233;
inline constexpr juce::uint32 kPresetStatusText = 0xffd0d0e0;

//==============================================================================
// Typography sizes
inline constexpr float kFontTitle       = 15.f;
inline constexpr float kFontNodeHeader  = 13.f;
inline constexpr float kFontLabel       = 12.f;
inline constexpr float kFontSmall       = 10.f;
inline constexpr float kFontGroupHeader = 11.f;

//==============================================================================
// Spacing
inline constexpr int kInspectorPadding  = 8;
inline constexpr int kGroupHeaderHeight = 22;
inline constexpr int kParamRowHeight    = 50;
inline constexpr int kTextParamHeight   = 190;
inline constexpr int kTitleHeight       = 28;

//==============================================================================
// Lasso selection
inline constexpr juce::uint32 kLassoFill    = 0x1864b5f6;
inline constexpr juce::uint32 kLassoBorder  = 0x6664b5f6;

//==============================================================================
// Port highlighting
inline constexpr juce::uint32 kPortGlow         = 0x44ffffff;
inline constexpr juce::uint32 kPortCompatible   = 0xff66ff88;
inline constexpr juce::uint32 kPortIncompatible  = 0x44ff4444;

//==============================================================================
// Node dimensions
inline constexpr int kNodeCornerRadius = 6;
inline constexpr float kGridSnapSize = 20.f;

//==============================================================================
// Helpers
inline juce::Colour bg (juce::uint32 c) { return juce::Colour (c); }

inline juce::Colour headerColourForCategory (const juce::String& category)
{
    if (category == "Audio")  return juce::Colour (kHeaderAudio);
    if (category == "Math")   return juce::Colour (kHeaderMath);
    if (category == "Visual") return juce::Colour (kHeaderVisual);
    return juce::Colour (kHeaderDefault);
}

} // namespace Theme
} // namespace pf
