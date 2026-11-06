#pragma once

#include "Controls.h"
#include "PluginProcessor.h"
#include <array>

namespace openedmkick::ui {

class KickWaveformView : public juce::Component, private juce::Timer {
public:
    explicit KickWaveformView(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    void refreshCache();

    OpenEdmKickProcessor& processor_;
    openedmkick::core::KickSynthSettings cachedSettings_{};
    bool cachedLocked_ = false;
    std::vector<float> cachedSamples_;
};

class KickSynthPanel : public juce::Component {
public:
    explicit KickSynthPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void selectNote(int noteIndex);
    void updateNoteButtons(int activeNote);

    OpenEdmKickProcessor& processor_;
    std::array<juce::TextButton, openedmkick::core::kNumNotes> noteButtons_;
    juce::TextButton auditionButton_;
    Toggle lockIn_;
    KickWaveformView waveformView_;
    Choice octave_;
    Knob fineTune_;
    Knob startPitch_;
    Knob pitchDecay_;
    Knob pitchCurve_;
    Knob punch_;
    Knob tailLength_;
    Knob tailCurve_;
    Knob subDrive_;
    Knob clickLevel_;
    Knob clickFreq_;
    juce::Rectangle<int> noteBarArea_;
    juce::Rectangle<int> pitchRowArea_;
    juce::Rectangle<int> bodyRowArea_;
    juce::ParameterAttachment noteWatcher_;
};

class MuleStagePanel : public juce::Component, private juce::Timer {
public:
    explicit MuleStagePanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    OpenEdmKickProcessor& processor_;
    juce::Image mascot_;
};

// Interactive Piano Roll editor that displays and edits kick triggers down to 1/64-note resolution.
class PianoRollView : public juce::Component, private juce::Timer {
public:
    explicit PianoRollView(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override { repaint(); }
    [[nodiscard]] juce::Rectangle<float> gridArea() const;

    OpenEdmKickProcessor& processor_;
};

class AutoPianoRollPanel : public juce::Component {
public:
    explicit AutoPianoRollPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void selectSlot(int slot);

private:
    OpenEdmKickProcessor& processor_;
    Toggle bypass_;
    Toggle enabled_;
    Toggle clockSync_;
    Choice timeSig_;
    Choice bars_;
    Choice grid_;
    Choice fillPreset_;
    juce::ComboBox quickPresetBox_;
    std::array<juce::TextButton, openedmkick::core::kNumAutoSlots> slotButtons_;
    Knob tempo_;
    Knob tailShorten_;
    Knob pitchRise_;
    Knob crushSweep_;
    PianoRollView pianoRoll_;
    juce::Rectangle<int> slotRowArea_;
    juce::Rectangle<int> leftControlsArea_;
    juce::Rectangle<int> pianoRollArea_;
    juce::ParameterAttachment bypassWatcher_;
    juce::ParameterAttachment slotWatcher_;
};

class PeakMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit PeakMeter(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    OpenEdmKickProcessor& processor_;
};

class EffectsPanel : public juce::Component {
public:
    explicit EffectsPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Section {
        juce::String title;
        juce::Colour accent;
        int units;
        juce::Rectangle<int> area;
    };

    Choice reverseMode_;
    Knob reverseMix_;
    Knob crushRate_;
    Knob crushBits_;
    Knob crushMix_;
    Choice stutterRate_;
    Knob stutterMix_;
    Choice gateRate_;
    Knob gateMix_;
    Choice pumpRate_;
    Knob pumpMix_;
    Knob drive_;
    Knob output_;
    Knob ceiling_;
    Toggle limiter_;
    PeakMeter meter_;
    std::array<Section, 5> sections_;
};

} // namespace openedmkick::ui
