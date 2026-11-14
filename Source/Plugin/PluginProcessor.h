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

    [[nodiscard]] openedmkick::core::KickSettings currentSynthSettings() const noexcept;
    [[nodiscard]] std::array<openedmkick::core::DrumLayerSettings, openedmkick::core::kNumSoundLayers>
        currentLayerSettings() const noexcept;
    [[nodiscard]] openedmkick::core::AutoSlotSettings currentAutoSlotSettings() const noexcept;
    [[nodiscard]] openedmkick::core::AutoSlotSettings autoSlotSettings(int slot) const noexcept;
    [[nodiscard]] int currentAutoSlot() const noexcept;
    [[nodiscard]] bool hasRestoredCustomState() const noexcept { return restoredCustomState_.load(); }
    [[nodiscard]] int currentMidiNote() const noexcept { return displayMidiNote.load(); }
    [[nodiscard]] std::vector<float> previewKickWaveform() const;
    [[nodiscard]] std::vector<openedmkick::core::PianoRollNote> currentPianoRollNotes() const;
    void togglePianoRollNote(int step64, int length64);
    void triggerAudition() noexcept { auditionPending_.store(true); }

    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float> displayPeak{ 0.0f };
    std::atomic<float> displayAutoPhase{ 0.0f };
    std::atomic<float> displayBeatPhase{ 0.0f };
    std::atomic<float> displayBpm{ 128.0f };
    std::atomic<float> displayBeatsPerBar{ 4.0f };
    std::atomic<float> displayMidiHz{ 65.4064f };
    std::atomic<int> displayMidiNote{ openedmkick::core::kDefaultMidiNote };
    std::atomic<bool> displayReversing{ false };
    std::atomic<uint32_t> displayTriggerCount{ 0 };

private:
    void syncSequencerPresets(
        const std::array<openedmkick::core::AutoSlotSettings, openedmkick::core::kNumAutoSlots>& slots);

    struct AutoSlotParams {
        std::atomic<float>* bars;
        std::atomic<float>* fillPreset;
        std::atomic<float>* grid;
        std::atomic<float>* tailShorten;
        std::atomic<float>* crushSweep;
    };

    struct LayerParams {
        std::atomic<float>* enabled;
        std::atomic<float>* sound;
        std::atomic<float>* gain;
        std::atomic<float>* transpose;
        std::atomic<float>* decay;
        std::atomic<float>* highPass;
        std::atomic<float>* lowPass;
    };

    struct ParameterValues {
        std::atomic<float>* sweepOctaves;
        std::atomic<float>* sweepMs;
        std::atomic<float>* sweepShape;
        std::atomic<float>* snapOctaves;
        std::atomic<float>* snapMs;
        std::atomic<float>* bodyTone;
        std::atomic<float>* bodyDrive;
        std::atomic<float>* saturation;
        std::atomic<float>* knockHz;
        std::atomic<float>* knockDb;
        std::atomic<float>* grit;
        std::atomic<float>* toneHz;
        std::atomic<float>* subLevel;
        std::atomic<float>* startPhase;
        std::atomic<float>* clickType;
        std::atomic<float>* clickLevel;
        std::atomic<float>* clickHz;
        std::atomic<float>* clickMs;
        std::atomic<float>* attackMs;
        std::atomic<float>* holdMs;
        std::atomic<float>* decayMs;
        std::atomic<float>* decayShape;
        std::atomic<float>* lowCutHz;
        std::atomic<float>* clip;
        std::atomic<float>* clockSync;
        std::atomic<float>* tempoBpm;
        std::atomic<float>* timeSignature;
        std::atomic<float>* autoEnabled;
        std::atomic<float>* autoBypass;
        std::atomic<float>* autoSlot;
        std::array<AutoSlotParams, openedmkick::core::kNumAutoSlots> autoSlots;
        std::array<LayerParams, openedmkick::core::kNumSoundLayers> layers;
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
        std::atomic<float>* highPass;
        std::atomic<float>* lowPass;
        std::atomic<float>* highPassSyncEnabled;
        std::atomic<float>* highPassSyncRate;
        std::atomic<float>* lowPassSyncEnabled;
        std::atomic<float>* lowPassSyncRate;
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
    std::atomic<bool> restoredCustomState_{ false };
    std::vector<openedmkick::core::MidiTrigger> midiScratch_;
    std::vector<openedmkick::core::MidiCcEvent> midiCcScratch_;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmKickProcessor)
};
