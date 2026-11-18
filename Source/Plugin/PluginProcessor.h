#pragma once

#include "KickEngine.h"
#include "ParameterRouting.h"
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
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return displayTailSeconds_.load(); }
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
    static constexpr double previewSampleRate = 24000.0;
    [[nodiscard]] std::vector<float> previewKickWaveform() const;
    [[nodiscard]] std::vector<openedmkick::core::PianoRollNote> currentPianoRollNotes() const;
    void togglePianoRollNote(int step64, int length64, int layer = -1, int ratchets = 1);
    void cyclePianoRollRatchets(int step64, int layer);
    void triggerAudition() noexcept { auditionPending_.store(true); }

    juce::AudioProcessorValueTreeState parameters;
    ParameterRouting& routing() noexcept { return routing_; }

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
    openedmkick::core::EngineSettings currentEngineSettings() const noexcept;
    void syncSequencerPresets(
        const std::array<openedmkick::core::AutoSlotSettings, openedmkick::core::kNumAutoSlots>& slots);

    void syncEditorSequencer() const;
    openedmkick::core::AutoSlotSettings hostAutoSlotSettings(int) const;
    void adoptDisplayedPattern();
    void publishPatterns();
    struct PatternSnapshot {
        openedmkick::core::AutoSequencer sequencer;
        std::array<int, openedmkick::core::kNumAutoSlots> presets{};
        std::array<int, openedmkick::core::kNumAutoSlots> bars{};
    };

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
        std::atomic<float>* velocityCurve;
        std::atomic<float>* velocityTimbre;
        std::atomic<float>* keyLow;
        std::atomic<float>* keyHigh;
        std::atomic<float>* velocityLow;
        std::atomic<float>* velocityHigh;
        std::atomic<float>* toneScale;
        std::atomic<float>* sweepScale;
        std::atomic<float>* solo;
        std::atomic<float>* pan;
        std::atomic<float>* invertPolarity;
    };

    struct ParameterValues {
        std::atomic<float>* sweepOctaves;
        std::atomic<float>* sweepMs;
        std::atomic<float>* sweepShape;
        std::atomic<float>* snapOctaves;
        std::atomic<float>* snapMs;
        std::atomic<float>* bodyTone;
        std::atomic<float>* knockHz;
        std::atomic<float>* knockDb;
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
        std::atomic<float>* ampEnabled;
        std::atomic<float>* ampAttackMs;
        std::atomic<float>* ampDecayMs;
        std::atomic<float>* ampSustain;
        std::atomic<float>* ampReleaseMs;
        std::atomic<float>* reverseEnabled;
        std::atomic<float>* crushEnabled;
        std::atomic<float>* stutterEnabled;
        std::atomic<float>* gateEnabled;
        std::atomic<float>* midiVolume;
        std::atomic<float>* midiExpression;
        std::atomic<float>* midiBrightness;
        std::atomic<float>* highPassMidi;
        std::atomic<float>* lowPassMidi;
        std::atomic<float>* subOctave;
        std::atomic<float>* clickPunch;
        std::atomic<float>* stereoWidth;
        std::atomic<float>* eqLowFreq;
        std::atomic<float>* eqLowDb;
        std::atomic<float>* eqMidFreq;
        std::atomic<float>* eqMidDb;
        std::atomic<float>* eqHighFreq;
        std::atomic<float>* eqHighDb;
    };

    static constexpr size_t kMaxMidiEvents = 65536;
    static_assert(std::atomic<int>::is_always_lock_free);
    std::atomic<double> displayTailSeconds_{0};
    ParameterRouting routing_;
    ParameterValues values_;
    openedmkick::core::KickEngine engine_;
    mutable std::mutex stateMutex_;
    openedmkick::core::AutoSequencer audioBaseline_;
    std::array<int, 4> baselinePresets_{0, 1, 2, 3};
    std::array<int, 4> baselineBars_{4, 4, 4, 4};
    std::array<bool, 4> usesBaseline_{};
    std::array<std::atomic<float>*, 4> hostFillParams_{};
    std::array<std::atomic<float>*, 4> hostBarParams_{};
    mutable openedmkick::core::AutoSequencer editorSequencer_;
    mutable std::array<int, openedmkick::core::kNumAutoSlots> editorPresets_{ -1, -1, -1, -1 };
    mutable std::array<int, openedmkick::core::kNumAutoSlots> editorBars_{};
    std::array<PatternSnapshot, 3> patternSnapshots_;
    std::atomic<int> middlePattern_{1};
    int writePattern_ = 0;
    int readPattern_ = 2;
    std::array<int, openedmkick::core::kNumAutoSlots> lastSlotPreset_{ -1, -1, -1, -1 };
    std::array<int, openedmkick::core::kNumAutoSlots> lastSlotBars_{ 0, 0, 0, 0 };
    std::atomic<bool> auditionPending_{ false };
    std::atomic<bool> restoredCustomState_{ false };
    std::vector<openedmkick::core::MidiInputEvent> midiEventScratch_;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmKickProcessor)
};
