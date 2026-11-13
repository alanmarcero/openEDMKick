#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace openedmkick::theme {

inline const juce::Colour kInk{ 0xff171521 };
inline const juce::Colour kSteel{ 0xff292736 };
inline const juce::Colour kEdge{ 0xff454253 };
inline const juce::Colour kTrack{ 0xff514e60 };
inline const juce::Colour kText{ 0xffe9e6ef };
inline const juce::Colour kTextDim{ 0xffaaa6b7 };
inline const juce::Colour kViolet{ 0xffa98bff };
inline const juce::Colour kCoral{ 0xffff705e };
inline const juce::Colour kBrass{ 0xffe9b94d };

// Semantic aliases used by shared JUCE drawing helpers.
inline const juce::Colour kBackground = kInk;
inline const juce::Colour kPanel = kSteel;
inline const juce::Colour kPanelEdge = kEdge;
inline const juce::Colour kKnobBody = kSteel.brighter(0.12f);
inline const juce::Colour kAccentMint = kViolet;
inline const juce::Colour kAccentGold = kBrass;
inline const juce::Colour kAccentFx = kViolet;
inline const juce::Colour kAccentReverse = kCoral;
inline const juce::Colour kAccentOut = kBrass;

} // namespace openedmkick::theme
