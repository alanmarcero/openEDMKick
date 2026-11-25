#pragma once

#include "Controls.h"
#include "PluginProcessor.h"

#include <array>
#include <memory>

namespace openedmkick::ui {

class KickWaveformView final : public juce::Component, private juce::Timer {
public:
    explicit KickWaveformView(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    OpenEdmKickProcessor& processor_;
    std::vector<std::atomic<float>*> sources_;
    std::vector<float> renderedInputs_;
    juce::AudioBuffer<float> hit_;
};

class PeakMeter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit PeakMeter(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    OpenEdmKickProcessor& processor_;
    float levelDb_ = -100.0f;
    float heldDb_ = -100.0f;
    double heldAtMs_ = 0.0;
    bool clipped_ = false;
};

class KickSynthPanel final : public juce::Component, private juce::Value::Listener, private juce::Timer {
public:
    explicit KickSynthPanel(OpenEdmKickProcessor& processor);
    ~KickSynthPanel() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }
    void refreshActivity();

private:
    void valueChanged(juce::Value&) override { setAdvancedMode(advancedMode_); }
    void timerCallback() override { refreshActivity(); }
    [[nodiscard]] std::array<juce::Rectangle<int>, 7> sections() const;
    bool heldNotes_ = false;
    bool advancedMode_ = false;
    Knob sweepOctaves_;
    Knob sweepMs_;
    Knob sweepShape_;
    Knob keyTracking_;
    Knob snapOctaves_;
    Knob snapMs_;
    Knob bodyTone_;
    Knob knockHz_;
    Knob knockDb_;
    Knob toneHz_;
    Knob subLevel_;
    Knob subOctave_;
    Knob startPhase_;
    Choice clickType_;
    Knob clickLevel_;
    Knob clickHz_;
    Knob clickMs_;
    Knob clickPunch_;
    Knob attack_;
    Knob hold_;
    Knob decay_;
    Knob decayShape_;
    Knob lowCut_;
    Toggle ampEnabled_;
    Knob ampAttack_, ampDecay_, ampSustain_, ampRelease_;
    DualEqKnob eqLow_;
    DualEqKnob eqMid_;
    DualEqKnob eqHigh_;
    Knob stereoWidth_;
    Knob output_;
    Knob ceiling_;
    Toggle limiter_;
    PeakMeter meter_;
    std::vector<std::unique_ptr<SectionLock>> sectionLocks_;
    KickWaveformView waveform_;
    OpenEdmKickProcessor& processor_;
};

class EffectsPanel final : public juce::Component, private juce::Timer {
public:
    explicit EffectsPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }
    void refreshActivity();

private:
    void timerCallback() override { refreshActivity(); }
    [[nodiscard]] std::array<juce::Rectangle<int>, 4> sections() const;
    OpenEdmKickProcessor& processor_;
    bool advancedMode_ = false;
    Toggle reverseEnabled_, highPassMidi_, lowPassMidi_;
    Knob midiVolume_, midiExpression_, midiBrightness_;
    std::vector<std::unique_ptr<SectionLock>> sectionLocks_;
    Choice reverseMode_;
    Knob reverseMix_;
    Toggle clockSync_;
    Knob tempo_;
    Choice timeSig_;
    Choice distType_;
    Knob distDrive_, distTone_, distMix_;
    Knob compThresh_, compRatio_, compAttack_, compRelease_, compMakeup_, compMix_;
    Knob reverbSize_, reverbDecay_, reverbDamp_, reverbGate_, reverbMix_;
    Knob highPass_;
    Knob lowPass_;
    Toggle highPassSync_;
    Choice highPassSyncRate_;
    Toggle lowPassSync_;
    Choice lowPassSyncRate_;
};

} // namespace openedmkick::ui
