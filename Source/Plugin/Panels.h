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
    Toggle masterBypass_;
    PeakMeter meter_;
    std::vector<std::unique_ptr<SectionLock>> sectionLocks_;
    KickWaveformView waveform_;
    OpenEdmKickProcessor& processor_;
    Toggle clickSampleOn_;
    Choice clickSampleSource_;
    Knob clickSampleLevel_;
    Knob clickSampleStart_;
    Knob clickSamplePitch_;
    Knob clickSampleHp_;
    juce::TextButton loadClick_{ "Load" };
};

class LayerPage final : public juce::Component {
public:
    LayerPage(OpenEdmKickProcessor& processor, int layer);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void showEnabled(bool enabled, bool isKick = true, bool mutedBySolo = false);

private:
    std::unique_ptr<SectionLock> sectionLock_;
    int layer_ = 0;
    Toggle enabled_;
    Choice sound_;
    Knob level_;
    Knob pitch_;
    Knob decay_;
    Knob highPass_;
    Knob lowPass_;
    std::array<std::unique_ptr<Knob>, 9> layerKnobs_;
    std::array<std::unique_ptr<Toggle>, 2> layerToggles_;
};

class LayerPanel final : public juce::Component, private juce::Timer {
public:
    explicit LayerPanel(OpenEdmKickProcessor& processor);
    void resized() override;
    void selectLayer(int layer);
    void refreshActivity();

private:
    void timerCallback() override { refreshActivity(); }
    OpenEdmKickProcessor& processor_;
    std::array<std::unique_ptr<LayerPage>, openedmkick::core::kNumSoundLayers> pages_;
    std::array<juce::TextButton, openedmkick::core::kNumSoundLayers> tabs_;
    int selectedLayer_ = 0;
};

class PianoRollView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit PianoRollView(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    [[nodiscard]] juce::Rectangle<float> gridArea() const;
    OpenEdmKickProcessor& processor_;
    float lastPhase_ = -1.0f;
    openedmkick::core::AutoSlotSettings lastSettings_{};
    std::vector<openedmkick::core::PianoRollNote> lastNotes_;
    std::array<std::pair<bool, int>, openedmkick::core::kNumSoundLayers> lastLayers_{};
};

class AutoPianoRollPanel final : public juce::Component, private juce::Timer {
public:
    explicit AutoPianoRollPanel(OpenEdmKickProcessor& processor);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void selectSlot(int slot);
    void setAdvancedMode(bool advanced);
    [[nodiscard]] bool isAdvancedMode() const noexcept { return advancedMode_; }
    void refreshActivity();

private:
    void timerCallback() override;
    void bindSlotControls();
    void syncSelectedSlot(int slot);
    OpenEdmKickProcessor& processor_;
    bool advancedMode_ = false;
    Choice scene_;
    std::unique_ptr<SectionLock> clockLock_, sceneLock_;
    Toggle bypass_;
    Toggle enabled_;
    Toggle clockSync_;
    Choice timeSig_;
    Choice bars_;
    Choice grid_;
    Choice fillPreset_;
    Knob tempo_;
    Knob tailShorten_;
    Knob crushSweep_;
    PianoRollView pianoRoll_;
    int selectedSlot_ = -1;
    juce::ParameterAttachment slotWatcher_;
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
    Toggle reverseEnabled_, crushEnabled_, stutterEnabled_, gateEnabled_, highPassMidi_, lowPassMidi_;
    Knob midiVolume_, midiExpression_, midiBrightness_;
    std::vector<std::unique_ptr<SectionLock>> sectionLocks_;
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
    Knob highPass_;
    Knob lowPass_;
    Toggle highPassSync_;
    Choice highPassSyncRate_;
    Toggle lowPassSync_;
    Choice lowPassSyncRate_;
};

} // namespace openedmkick::ui
