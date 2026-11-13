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
    openedmkick::core::KickSynthSettings cachedSettings_{};
    bool hasCache_ = false;
    bool cachedLocked_ = false;
    float cachedHighPass_ = -1.0f;
    float cachedLowPass_ = -1.0f;
};

class KickSynthPanel final : public juce::Component {
public:
    explicit KickSynthPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }

private:
    void selectNote(int index);
    void updateNoteButtons(float note);
    OpenEdmKickProcessor& processor_;
    bool advancedMode_ = false;
    std::array<juce::TextButton, openedmkick::core::kNumNotes> noteButtons_;
    juce::TextButton auditionButton_{ "AUDITION" };
    Choice triggerMode_;
    Choice octave_;
    Knob fineTune_;
    Knob startPitch_;
    Knob pitchDecay_;
    Knob pitchCurve_;
    Knob bodyTone_;
    Knob attack_;
    Knob decay_;
    Knob sustain_;
    Knob release_;
    Knob tailLength_;
    Knob tailCurve_;
    Knob punch_;
    Knob subDrive_;
    Toggle lockIn_;
    Knob clickLevel_;
    Knob clickFreq_;
    Knob snareLevel_;
    Knob snareTone_;
    Knob snareDecay_;
    Knob clapLevel_;
    Knob clapSpread_;
    Knob clapTone_;
    Knob clapDecay_;
    Knob hatLevel_;
    Knob hatTone_;
    Knob hatDecay_;
    KickWaveformView waveform_;
    SectionLock tuningLock_;
    SectionLock pitchLock_;
    SectionLock bodyLock_;
    SectionLock transientLock_;
    SectionLock layersLock_;
    juce::ParameterAttachment noteWatcher_;
    juce::Rectangle<int> tuningArea_, waveArea_, pitchArea_, bodyArea_, transientArea_, layersArea_;
};

class MuleStagePanel final : public juce::Component, private juce::Timer {
public:
    explicit MuleStagePanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    OpenEdmKickProcessor& processor_;
    juce::Image mascot_;
    uint32_t seenTriggers_ = 0;
    double kickStartedMs_ = -1.0e12;
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
    OpenEdmKickProcessor& processor_;
    bool advancedMode_ = false;
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
    SectionLock autoLock_;
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
    SectionLock reverseLock_;
    SectionLock textureLock_;
    SectionLock rhythmLock_;
    SectionLock toneLock_;
};

} // namespace openedmkick::ui
