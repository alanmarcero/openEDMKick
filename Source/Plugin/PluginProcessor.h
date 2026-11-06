#pragma once

#include "KickEngine.h"
#include <array>
#include <atomic>
#include <mutex>
#include <juce_audio_processors/juce_audio_processors.h>

class OpenEdmKickProcessor : public juce::AudioProcessor {
public:
    OpenEdmKickProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    [[nodiscard]] openedmkick::core::KickSynthSettings currentSynthSettings() const noexcept;
    [[nodiscard]] openedmkick::core::AutoSlotSettings currentAutoSlotSettings() const noexcept;
    [[nodiscard]] openedmkick::core::AutoSlotSettings autoSlotSettings(int slot) const noexcept;
    [[nodiscard]] int currentAutoSlot() const noexcept;
    [[nodiscard]] bool isKickLockedIn() const noexcept { return displayLockedIn.load(); }
    [[nodiscard]] std::vector<float> previewKickWaveform() const;
    [[nodiscard]] std::vector<openedmkick::core::PianoRollNote> currentPianoRollNotes() const;
    void togglePianoRollNote(int step64, int length64, int semitoneOffset);
    void triggerAudition() noexcept { auditionPending_.store(true); }

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> displayPeak{ 0.0f };
    std::atomic<float> displayAutoPhase{ 0.0f };
    std::atomic<float> displayBeatPhase{ 0.0f };
    std::atomic<float> displayBpm{ 128.0f };
    std::atomic<float> displayBeatsPerBar{ 4.0f };
    std::atomic<bool> displayLockedIn{ false };
    std::atomic<bool> displayReversing{ false };

private:
    void syncSequencerPresets(const std::array<openedmkick::core::AutoSlotSettings, openedmkick::core::kNumAutoSlots>& slots);

    struct AutoSlotParams {
        std::atomic<float>* bars;
        std::atomic<float>* fillPreset;
        std::atomic<float>* grid;
        std::atomic<float>* tailShorten;
        std::atomic<float>* pitchRise;
        std::atomic<float>* crushSweep;
    };

    struct ParameterValues {
        std::atomic<float>* note;
        std::atomic<float>* octave;
        std::atomic<float>* fineTune;
        std::atomic<float>* startPitch;
        std::atomic<float>* pitchDecay;
        std::atomic<float>* pitchCurve;
        std::atomic<float>* tailLength;
        std::atomic<float>* tailCurve;
        std::atomic<float>* subDrive;
        std::atomic<float>* clickLevel;
        std::atomic<float>* clickFreq;
        std::atomic<float>* punch;
        std::atomic<float>* lockIn;
        std::atomic<float>* clockSync;
        std::atomic<float>* tempoBpm;
        std::atomic<float>* timeSignature;
        std::atomic<float>* autoEnabled;
        std::atomic<float>* autoBypass;
        std::atomic<float>* autoSlot;
        std::array<AutoSlotParams, openedmkick::core::kNumAutoSlots> autoSlots;
        std::atomic<float>* reverseMode;
        std::atomic<float>* reverseMix;
        std::atomic<float>* drive;
        std::atomic<float>* crushRate;
        std::atomic<float>* crushBits;
        std::atomic<float>* crushMix;
        std::atomic<float>* stutterRate;
        std::atomic<float>* stutterMix;
        std::atomic<float>* gateRate;
        std::atomic<float>* gateMix;
        std::atomic<float>* pumpRate;
        std::atomic<float>* pumpMix;
        std::atomic<float>* output;
        std::atomic<float>* limiter;
        std::atomic<float>* ceiling;
        std::atomic<float>* masterBypass;
    };

    ParameterValues values_;
    openedmkick::core::KickEngine engine_;
    mutable std::mutex stateMutex_;
    std::array<int, openedmkick::core::kNumAutoSlots> lastSlotPreset_{ -1, -1, -1, -1 };
    std::array<int, openedmkick::core::kNumAutoSlots> lastSlotBars_{ 0, 0, 0, 0 };
    std::atomic<bool> auditionPending_{ false };
    std::vector<openedmkick::core::MidiTrigger> midiScratch_;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmKickProcessor)
};
