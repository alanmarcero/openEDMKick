#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace openedmkick::theme {

inline const juce::Colour kInk{ 0xff0D1317 };
inline const juce::Colour kSteel{ 0xff152026 };
inline const juce::Colour kEdge{ 0xff283842 };
inline const juce::Colour kTrack{ 0xff283842 };
inline const juce::Colour kText{ 0xffE6F0F5 };
inline const juce::Colour kTextDim{ 0xff869CA8 };
inline const juce::Colour kViolet{ 0xff48F0C8 };
inline const juce::Colour kCoral{ 0xffFFBE3B };
inline const juce::Colour kBrass{ 0xffFFBE3B };

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
