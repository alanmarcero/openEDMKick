#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace openedmkick::theme {

inline const juce::Colour kInk{ 0xff111824 };
inline const juce::Colour kSteel{ 0xff202b38 };
inline const juce::Colour kEdge{ 0xff344455 };
inline const juce::Colour kTrack{ 0xff405366 };
inline const juce::Colour kText{ 0xffe9e4d8 };
inline const juce::Colour kTextDim{ 0xffa5b2bf };
inline const juce::Colour kCyan{ 0xff4ec5e8 };
inline const juce::Colour kCoral{ 0xffff715b };
inline const juce::Colour kAmber{ 0xffe9b971 };

// Semantic aliases used by shared JUCE drawing helpers.
inline const juce::Colour kBackground = kInk;
inline const juce::Colour kPanel = kSteel;
inline const juce::Colour kPanelEdge = kEdge;
inline const juce::Colour kKnobBody = kSteel.brighter(0.12f);
inline const juce::Colour kAccentMint = kCyan;
inline const juce::Colour kAccentGold = kAmber;
inline const juce::Colour kAccentFx = kCyan;
inline const juce::Colour kAccentReverse = kCoral;
inline const juce::Colour kAccentOut = kAmber;

} // namespace openedmkick::theme
