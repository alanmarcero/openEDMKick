#pragma once

#include "Controls.h"
#include "PluginProcessor.h"

#include <array>

namespace openedmkick::ui {

class KickWaveformView final : public juce::Component, private juce::Timer {
public:
    explicit KickWaveformView(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    OpenEdmKickProcessor& processor_;
    std::vector<float> samples_;
};

class KickSynthPanel final : public juce::Component {
public:
    explicit KickSynthPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }

private:
    void setVisibleControls(std::initializer_list<juce::Component*> controls, bool visible);
    bool advancedMode_ = false;
    Knob sweepOctaves_;
    Knob sweepMs_;
    Knob sweepShape_;
    Knob snapOctaves_;
    Knob snapMs_;
    Knob bodyTone_;
    Knob bodyDrive_;
    Choice saturation_;
    Knob knockHz_;
    Knob knockDb_;
    Knob grit_;
    Knob toneHz_;
    Knob subLevel_;
    Knob startPhase_;
    Choice clickType_;
    Knob clickLevel_;
    Knob clickHz_;
    Knob clickMs_;
    Knob attack_;
    Knob hold_;
    Knob decay_;
    Knob decayShape_;
    Knob lowCut_;
    Knob clip_;
    KickWaveformView waveform_;
};

class PianoRollView final : public juce::Component, private juce::Timer {
public:
    explicit PianoRollView(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override { repaint(); }
    [[nodiscard]] juce::Rectangle<float> gridArea() const;
    OpenEdmKickProcessor& processor_;
};

class AutoPianoRollPanel final : public juce::Component {
public:
    explicit AutoPianoRollPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void selectSlot(int slot);
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }

private:
    void bindSlotControls();
    void syncSelectedSlot(int slot);
    OpenEdmKickProcessor& processor_;
    bool advancedMode_ = false;
    Toggle bypass_;
    Toggle enabled_;
    Toggle clockSync_;
    Choice timeSig_;
    Choice bars_;
    Choice grid_;
    Choice fillPreset_;
    std::array<juce::TextButton, openedmkick::core::kNumAutoSlots> slotButtons_;
    Knob tempo_;
    Knob tailShorten_;
    Knob crushSweep_;
    PianoRollView pianoRoll_;
    int selectedSlot_ = -1;
    juce::ParameterAttachment slotWatcher_;
};

class PeakMeter final : public juce::Component, private juce::Timer {
public:
    explicit PeakMeter(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }
    OpenEdmKickProcessor& processor_;
};

class EffectsPanel final : public juce::Component {
public:
    explicit EffectsPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }

private:
    bool advancedMode_ = false;
    Choice reverseMode_;
    Knob reverseMix_;
    Knob drive_;
    Knob crushRate_;
    Knob crushBits_;
    Knob crushMix_;
    Choice stutterRate_;
    Knob stutterMix_;
    Choice gateRate_;
    Knob gateMix_;
    Choice pumpRate_;
    Knob pumpMix_;
    Knob highPass_;
    Knob lowPass_;
    Knob output_;
    Knob ceiling_;
    Toggle limiter_;
    Toggle masterBypass_;
    PeakMeter meter_;
};

} // namespace openedmkick::ui
