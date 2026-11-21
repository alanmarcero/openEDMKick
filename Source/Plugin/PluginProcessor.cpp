#include "PluginProcessor.h"

#include "ClickSample.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "suite/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ranges>
#include <juce_audio_formats/juce_audio_formats.h>

namespace core = openedmkick::core;
namespace params = openedmkick::params;
namespace suite = openedmkick::suite;

OpenEdmKickProcessor::OpenEdmKickProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openEDMKick", params::createParameterLayout()),
      routing_(parameters),
      values_([this] {
          const auto raw = [this](const juce::String& id) { return routing_.effectiveRaw(id); };
          const auto slotParams = [&](int slot) {
              const auto slotRaw = [&](const char* key) { return raw(params::autoSlotId(slot, key)); };
              return AutoSlotParams{ slotRaw(params::kAutoBars), slotRaw(params::kAutoFillPreset),
                                     slotRaw(params::kAutoGrid), slotRaw(params::kAutoTailShorten),
                                     slotRaw(params::kAutoCrushSweep) };
          };
          const auto layerParams = [&](int layer) {
              const auto layerRaw = [&](const char* key) { return raw(params::layerId(layer, key)); };
              return LayerParams{ layerRaw(params::kLayerEnabled), layerRaw(params::kLayerSound),
                                  layerRaw(params::kLayerGain), layerRaw(params::kLayerTranspose),
                                  layerRaw(params::kLayerDecay), layerRaw(params::kLayerHighPass),
                                  layerRaw(params::kLayerLowPass),
                                  layerRaw(params::kLayerVelocityCurve), layerRaw(params::kLayerVelocityTimbre),
                                  layerRaw(params::kLayerKeyLow), layerRaw(params::kLayerKeyHigh),
                                  layerRaw(params::kLayerVelocityLow), layerRaw(params::kLayerVelocityHigh),
                                  layerRaw(params::kLayerToneScale),
                                  layerRaw(params::kLayerSweepScale), layerRaw(params::kLayerSolo),
                                  layerRaw(params::kLayerPan), layerRaw(params::kLayerPolarity) };
          };
          return ParameterValues{
              raw(params::kSweepOctaves), raw(params::kSweepMs), raw(params::kSweepShape),
              raw(params::kSnapOctaves), raw(params::kSnapMs), raw(params::kBodyTone),
              raw(params::kKnockHz),
              raw(params::kKnockDb), raw(params::kToneHz),
              raw(params::kSubLevel), raw(params::kStartPhase), raw(params::kClickType),
              raw(params::kClickLevel), raw(params::kClickHz), raw(params::kClickMs),
              raw(params::kAttackMs), raw(params::kHoldMs), raw(params::kDecayMs),
              raw(params::kDecayShape), raw(params::kLowCutHz),
              raw(params::kClockSync), raw(params::kTempoBpm), raw(params::kTimeSignature),
              raw(params::kAutoEnabled), raw(params::kAutoBypass), raw(params::kAutoSlot),
              { slotParams(0), slotParams(1), slotParams(2), slotParams(3) },
              { layerParams(0), layerParams(1), layerParams(2), layerParams(3) },
              raw(params::kReverseMode), raw(params::kReverseMix),
              raw(params::kCrushRate), raw(params::kCrushBits), raw(params::kCrushMix),
              raw(params::kStutterRate), raw(params::kStutterMix), raw(params::kGateRate),
              raw(params::kGateMix), raw(params::kPumpRate), raw(params::kPumpMix),
              raw(params::kHighPass), raw(params::kLowPass), raw(params::kHighPassSyncEnabled),
              raw(params::kHighPassSyncRate), raw(params::kLowPassSyncEnabled), raw(params::kLowPassSyncRate),
              raw(params::kOutput),
              raw(params::kLimiter), raw(params::kCeiling), raw(params::kMasterBypass),
              raw(params::kAmpEnabled), raw(params::kAmpAttack), raw(params::kAmpDecay), raw(params::kAmpSustain), raw(params::kAmpRelease), raw(params::kReverseEnabled), raw(params::kCrushEnabled), raw(params::kStutterEnabled), raw(params::kGateEnabled), raw(params::kMidiVolume), raw(params::kMidiExpression), raw(params::kMidiBrightness), raw(params::kHighPassMidi), raw(params::kLowPassMidi),
              raw(params::kSubOctave), raw(params::kClickPunch), raw(params::kStereoWidth),
              raw(params::kEqLowFreq), raw(params::kEqLowDb), raw(params::kEqMidFreq),
              raw(params::kEqMidDb), raw(params::kEqHighFreq), raw(params::kEqHighDb),
              raw(params::kClickSampleOn), raw(params::kClickSampleSource), raw(params::kClickSampleLevel),
              raw(params::kClickSampleStart), raw(params::kClickSamplePitch), raw(params::kClickSampleHp)
          };
      }()) {
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
        hostFillParams_[static_cast<size_t>(slot)] = parameters.getRawParameterValue(params::autoSlotId(slot, params::kAutoFillPreset));
        hostBarParams_[static_cast<size_t>(slot)] = parameters.getRawParameterValue(params::autoSlotId(slot, params::kAutoBars));
    }
    setLatencySamples(core::KickEngine::kLatencySamples);
    engine_.prepare(48000.0);
    core::warmBuiltInClicks();
    openedmkick::presets::apply(openedmkick::presets::factoryPresets().front(), parameters);
    history_ = std::make_unique<suite::ParameterUndo>(*this);
    history_->captureContext = [this] {
        const std::scoped_lock lock(userPresetMutex_);
        return suite::EditContext{ currentProgram_, restoredCustomState_.load(), userPresetName_, presetEdited_.load() };
    };
    history_->restoreContext = [this](const suite::EditContext& context) {
        currentProgram_ = context.program;
        restoredCustomState_.store(context.custom);
        {
            const std::scoped_lock lock(userPresetMutex_);
            userPresetName_ = context.presetName;
        }
        presetEdited_.store(context.edited);
    };
    history_->onParameterChange = [this] { presetEdited_.store(true); };
    ab_.seed(captureSoundState());
}

void OpenEdmKickProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const std::scoped_lock lock(stateMutex_);
    preparedRate_ = sampleRate;
    core::warmBuiltInClicks();
    engine_.prepare(sampleRate);
    midiEventScratch_.reserve(kMaxMidiEvents);
    juce::ignoreUnused(samplesPerBlock);
}

bool OpenEdmKickProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

int OpenEdmKickProcessor::currentAutoSlot() const noexcept {
    return std::clamp(static_cast<int>(values_.autoSlot->load()), 0, core::kNumAutoSlots - 1);
}

core::KickSettings OpenEdmKickProcessor::currentSynthSettings() const noexcept {
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    return core::KickSettings{
        values_.sweepOctaves->load(), values_.sweepMs->load(), values_.sweepShape->load(),
        values_.snapOctaves->load(), values_.snapMs->load(), values_.bodyTone->load(),
        values_.knockHz->load(), values_.knockDb->load(),
        values_.toneHz->load(), values_.subLevel->load(), values_.startPhase->load(),
        static_cast<core::ClickType>(choice(values_.clickType)), values_.clickLevel->load(),
        values_.clickHz->load(), values_.clickMs->load(), values_.attackMs->load(),
        values_.holdMs->load(), values_.decayMs->load(), values_.decayShape->load(),
        values_.lowCutHz->load(),
        values_.ampEnabled->load() > 0.5f, values_.ampAttackMs->load(), values_.ampDecayMs->load(), values_.ampSustain->load(), values_.ampReleaseMs->load(),
        values_.subOctave->load(), values_.clickPunch->load()
    };
}

std::array<core::DrumLayerSettings, core::kNumSoundLayers> OpenEdmKickProcessor::currentLayerSettings() const noexcept {
    std::array<core::DrumLayerSettings, core::kNumSoundLayers> result{};
    std::ranges::for_each(std::views::iota(size_t{ 0 }, result.size()), [&](size_t index) {
        const auto& layer = values_.layers[index];
        result[index] = core::DrumLayerSettings{
            layer.enabled->load() > 0.5f,
            static_cast<core::DrumSoundType>(std::clamp(static_cast<int>(layer.sound->load()), 0, 3)),
            layer.gain->load(), layer.transpose->load(), layer.decay->load(),
            layer.highPass->load(), layer.lowPass->load(),
            layer.velocityCurve->load(), layer.velocityTimbre->load(),
            static_cast<int>(layer.keyLow->load()), static_cast<int>(layer.keyHigh->load()),
            static_cast<int>(layer.velocityLow->load()), static_cast<int>(layer.velocityHigh->load()),
            layer.toneScale->load(), layer.sweepScale->load(),
            layer.solo->load() > 0.5f, layer.pan->load(), layer.invertPolarity->load() > 0.5f
        };
    });
    return result;
}

core::AutoSlotSettings OpenEdmKickProcessor::autoSlotSettings(int slot) const noexcept {
    const int safeSlot = std::clamp(slot, 0, core::kNumAutoSlots - 1);
    const auto& slotValues = values_.autoSlots[static_cast<size_t>(safeSlot)];
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    const auto barIndex = static_cast<size_t>(
        std::clamp(choice(slotValues.bars), 0, static_cast<int>(core::kAutoBarCounts.size()) - 1));
    return core::AutoSlotSettings{
        core::kAutoBarCounts[barIndex],
        std::clamp(choice(slotValues.fillPreset), 0, core::kNumFillPresets - 1),
        std::clamp(choice(slotValues.grid), 0, static_cast<int>(core::kGridStepSizes64.size()) - 1),
        slotValues.tailShorten->load(),
        slotValues.crushSweep->load()
    };
}

core::AutoSlotSettings OpenEdmKickProcessor::currentAutoSlotSettings() const noexcept {
    return autoSlotSettings(currentAutoSlot());
}

std::vector<core::PianoRollNote> OpenEdmKickProcessor::currentPianoRollNotes() const {
    const std::scoped_lock lock(stateMutex_);
    syncEditorSequencer();
    const int slot = currentAutoSlot();
    const auto settings = autoSlotSettings(slot);
    const auto index = static_cast<size_t>(slot);
    if (settings.fillPreset != editorPresets_[index] || settings.bars != editorBars_[index])
        return core::buildPresetPattern(settings.fillPreset, settings.bars);
    return editorSequencer_.notesForSlot(slot);
}

core::AutoSlotSettings OpenEdmKickProcessor::hostAutoSlotSettings(int slot) const {
    const auto raw = [&](const char* key) { return parameters.getRawParameterValue(params::autoSlotId(slot, key))->load(); };
    const auto bars = static_cast<size_t>(std::clamp(static_cast<int>(raw(params::kAutoBars)), 0, 6));
    return {core::kAutoBarCounts[bars], std::clamp(static_cast<int>(raw(params::kAutoFillPreset)), 0, core::kNumFillPresets - 1),
        std::clamp(static_cast<int>(raw(params::kAutoGrid)), 0, 4), raw(params::kAutoTailShorten), raw(params::kAutoCrushSweep)};
}

void OpenEdmKickProcessor::adoptDisplayedPattern() {
    for (const auto* key : std::array<const char*, 2>{params::kAutoBars, params::kAutoFillPreset}) {
        const auto id = params::autoSlotId(currentAutoSlot(), key);
        if (!routing_.isOverridden(id)) continue;
        const float value = routing_.displayValue(id);
        auto* parameter = parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        routing_.manualValue(id, value);
    }
}

void OpenEdmKickProcessor::syncEditorSequencer() const {
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
        const auto index = static_cast<size_t>(slot);
        const auto settings = hostAutoSlotSettings(slot);
        if (editorPresets_[index] == settings.fillPreset && editorBars_[index] == settings.bars) continue;
        editorSequencer_.loadPresetIntoSlot(slot, settings.fillPreset, settings.bars);
        editorPresets_[index] = settings.fillPreset;
        editorBars_[index] = settings.bars;
    }
}

void OpenEdmKickProcessor::publishPatterns() {
    auto& snapshot = patternSnapshots_[static_cast<size_t>(writePattern_)];
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot)
        snapshot.sequencer.copyNotesForSlot(slot, editorSequencer_.notesForSlot(slot));
    snapshot.presets = editorPresets_;
    snapshot.bars = editorBars_;
    // The dirty bit transfers ownership; the producer never writes the reader's buffer.
    writePattern_ = middlePattern_.exchange(writePattern_ | 4, std::memory_order_acq_rel) & 3;
}

void OpenEdmKickProcessor::togglePianoRollNote(int step64, int length64, int layer, int ratchets) {
    changeSound("Pattern", [&] {
        const std::scoped_lock lock(stateMutex_);
        adoptDisplayedPattern();
        syncEditorSequencer();
        const int slot = currentAutoSlot();
        editorSequencer_.toggleNoteAt(slot, step64, length64, autoSlotSettings(slot).bars, layer, ratchets);
        publishPatterns();
        presetEdited_.store(true);
        return true;
    });
}

void OpenEdmKickProcessor::cyclePianoRollRatchets(int step64, int layer) {
    changeSound("Pattern", [&] {
        const std::scoped_lock lock(stateMutex_);
        adoptDisplayedPattern();
        syncEditorSequencer();
        editorSequencer_.cycleRatchetsAt(currentAutoSlot(), step64, layer);
        publishPatterns();
        presetEdited_.store(true);
        return true;
    });
}

void OpenEdmKickProcessor::syncSequencerPresets(
    const std::array<core::AutoSlotSettings, core::kNumAutoSlots>& slots) {
    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int slot) {
        const auto index = static_cast<size_t>(slot);
        const bool baseline = slots[index].fillPreset == baselinePresets_[index] && slots[index].bars == baselineBars_[index];
        if (baseline) engine_.sequencer().referenceSlotPattern(slot, audioBaseline_);
        if (!baseline && (usesBaseline_[index] || slots[index].fillPreset != lastSlotPreset_[index] || slots[index].bars != lastSlotBars_[index]))
            engine_.sequencer().loadPresetIntoSlot(slot, slots[index].fillPreset, slots[index].bars);
        usesBaseline_[index] = baseline;
        lastSlotPreset_[index] = slots[index].fillPreset;
        lastSlotBars_[index] = slots[index].bars;
    });
}

juce::AudioProcessorParameter* OpenEdmKickProcessor::getBypassParameter() const {
    return parameters.getParameter(params::kMasterBypass);
}

core::EngineSettings OpenEdmKickProcessor::currentEngineSettings() const noexcept {
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    core::EngineSettings settings{};
    settings.synth = currentSynthSettings();
    settings.layers = currentLayerSettings();
    settings.autoEnabled = isOn(values_.autoEnabled);
    settings.autoBypass = isOn(values_.autoBypass);
    settings.activeAutoSlot = currentAutoSlot();
    settings.autoSlots = {autoSlotSettings(0), autoSlotSettings(1), autoSlotSettings(2), autoSlotSettings(3)};
    settings.effects = core::EffectSettings{
        static_cast<core::ReverseSyncMode>(std::clamp(choice(values_.reverseMode), 0, 4)),
        values_.reverseMix->load(), values_.crushRate->load(),
        choice(values_.crushBits), values_.crushMix->load(), choice(values_.stutterRate),
        values_.stutterMix->load(), choice(values_.gateRate), values_.gateMix->load(),
        choice(values_.pumpRate), values_.pumpMix->load(),
        juce::Decibels::decibelsToGain(values_.output->load()), isOn(values_.limiter),
        juce::Decibels::decibelsToGain(values_.ceiling->load()), values_.highPass->load(),
        values_.lowPass->load(), isOn(values_.highPassSyncEnabled), choice(values_.highPassSyncRate),
        isOn(values_.lowPassSyncEnabled), choice(values_.lowPassSyncRate),
        values_.reverseEnabled->load() > 0.5f, values_.crushEnabled->load() > 0.5f, values_.stutterEnabled->load() > 0.5f, values_.gateEnabled->load() > 0.5f, values_.midiVolume->load(), values_.midiExpression->load(), values_.midiBrightness->load(), values_.highPassMidi->load() > 0.5f, values_.lowPassMidi->load() > 0.5f,
        values_.stereoWidth->load(),
        core::ThreeBandEqSettings{
            values_.eqLowFreq->load(), values_.eqLowDb->load(),
            values_.eqMidFreq->load(), values_.eqMidDb->load(),
            values_.eqHighFreq->load(), values_.eqHighDb->load()
        }
    };
    settings.clickSample = {
        isOn(values_.clickSampleOn),
        values_.clickSampleLevel->load(),
        values_.clickSampleStart->load(),
        values_.clickSamplePitch->load(),
        values_.clickSampleHp->load()
    };
    settings.masterBypass = isOn(values_.masterBypass);
    const int signature = choice(values_.timeSignature);
    settings.manualBpm = isOn(values_.clockSync) ? 0 : static_cast<double>(values_.tempoBpm->load());
    settings.manualBeatsPerBar = (!isOn(values_.clockSync) || signature != 0) ? params::beatsPerBarForSignature(signature) : 0;

    return settings;
}

void OpenEdmKickProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };

    const std::array<core::AutoSlotSettings, core::kNumAutoSlots> slotSettings{
        autoSlotSettings(0), autoSlotSettings(1), autoSlotSettings(2), autoSlotSettings(3)
    };
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
        const auto index = static_cast<size_t>(slot);
        const int preset = std::clamp(static_cast<int>(hostFillParams_[index]->load()), 0, core::kNumFillPresets - 1);
        const int barChoice = std::clamp(static_cast<int>(hostBarParams_[index]->load()), 0, 6);
        const int bars = core::kAutoBarCounts[static_cast<size_t>(barChoice)];
        if (preset == baselinePresets_[index] && bars == baselineBars_[index]) continue;
        audioBaseline_.loadPresetIntoSlot(slot, preset, bars);
        baselinePresets_[index] = preset;
        baselineBars_[index] = bars;
    }
    if ((middlePattern_.load(std::memory_order_acquire) & 4) != 0) {
        readPattern_ = middlePattern_.exchange(readPattern_, std::memory_order_acq_rel) & 3;
        auto& snapshot = patternSnapshots_[static_cast<size_t>(readPattern_)];
        for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
            const auto index = static_cast<size_t>(slot);
            if (snapshot.presets[index] != baselinePresets_[index] || snapshot.bars[index] != baselineBars_[index]) continue;
            audioBaseline_.swapSlotPattern(slot, snapshot.sequencer);
        }
    }

    syncSequencerPresets(slotSettings);
    auto settings = currentEngineSettings();

    const double manualBpm = static_cast<double>(values_.tempoBpm->load());
    const int signatureChoice = choice(values_.timeSignature);
    const double manualBeatsPerBar = params::beatsPerBarForSignature(signatureChoice);
    const core::Transport transport = [&] {
        auto* playHead = getPlayHead();
        if (playHead == nullptr) return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar, true };
        const auto position = playHead->getPosition();
        if (!position.hasValue()) return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar, true };
        const double bpm = position->getBpm().orFallback(manualBpm);
        double beatsPerBar = manualBeatsPerBar;
        if (signatureChoice == 0 && position->getTimeSignature().hasValue()) {
            const auto signature = *position->getTimeSignature();
            if (signature.numerator > 0 && signature.denominator > 0)
                beatsPerBar = static_cast<double>(signature.numerator) * (4.0 / static_cast<double>(signature.denominator));
        }
        const bool playing = position->getIsPlaying();
        if (!playing || !position->getPpqPosition().hasValue())
            return core::Transport{ bpm, std::nullopt, beatsPerBar, playing };
        return core::Transport{ bpm, *position->getPpqPosition(), beatsPerBar, true };
    }();

    midiEventScratch_.clear();
    const int lastSample = std::max(0, buffer.getNumSamples() - 1);
    std::ranges::for_each(midi, [&](const juce::MidiMessageMetadata& metadata) {
        if (midiEventScratch_.size() >= kMaxMidiEvents - 1) return;
        const auto message = metadata.getMessage();
        const int sample = std::clamp(metadata.samplePosition, 0, lastSample);
        if (message.isNoteOn() && message.getFloatVelocity() > 0.0f) {
            const int note = message.getNoteNumber();
            const float velocity = message.getFloatVelocity();
            midiEventScratch_.emplace_back(core::MidiTrigger{ sample, note, velocity, false, message.getChannel() });
            displayMidiNote.store(note, std::memory_order_relaxed);
            displayMidiHz.store(core::noteFrequencyHz(note), std::memory_order_relaxed);
            displayVelocity.store(velocity, std::memory_order_relaxed);
            return;
        }
        if (message.isNoteOff()) {
            midiEventScratch_.emplace_back(core::MidiNoteOff{sample, message.getNoteNumber(), message.getChannel()});
            return;
        }
        if (!message.isController()) return;
        const int controller = message.getControllerNumber();
        midiEventScratch_.emplace_back(core::MidiCcEvent{ sample, controller, message.getControllerValue() });
    });

    if (auditionPending_.exchange(false)) {
        const auto afterSampleZero = std::upper_bound(midiEventScratch_.begin(), midiEventScratch_.end(), 0,
            [](int sample, const core::MidiInputEvent& input) {
                return sample < std::visit([](const auto& event) { return event.sampleOffset; }, input);
            });
        midiEventScratch_.insert(afterSampleZero, core::MidiTrigger{ 0, core::kDefaultMidiNote, 1.0f, true });
        displayMidiNote.store(core::kDefaultMidiNote, std::memory_order_relaxed);
        displayMidiHz.store(core::noteFrequencyHz(core::kDefaultMidiNote), std::memory_order_relaxed);
        displayVelocity.store(1.0f, std::memory_order_relaxed);
    }

    const float* clickData = nullptr;
    int clickLength = 0;
    double clickRate = 44100.0;
    selectClickSource(clickData, clickLength, clickRate);
    userAudioSeen_.store(userPublished_.load(std::memory_order_acquire), std::memory_order_release);
    engine_.setClickSampleSource(clickData, clickLength, clickRate);

    core::MidiRouting routing{
        this,
        [](void* context, int cc, int value, core::EngineSettings& current) noexcept {
            auto& processor = *static_cast<OpenEdmKickProcessor*>(context);
            if (cc == 121) processor.routing_.clearPerformance();
            if (processor.routing_.applyCc(cc, value) || cc == 121) {
                current = processor.currentEngineSettings();
            }
        },
        [](void* context) noexcept { static_cast<OpenEdmKickProcessor*>(context)->routing_.saveSample(); },
        [](void* context, core::EngineSettings& current) noexcept {
            auto& processor = *static_cast<OpenEdmKickProcessor*>(context);
            processor.routing_.restoreSample();
            current = processor.currentEngineSettings();
        },
        [](void* context, const core::EngineSettings& current) noexcept {
            static_cast<OpenEdmKickProcessor*>(context)->syncSequencerPresets(current.autoSlots);
        }
    };
    buffer.clear();
    engine_.process(settings, transport, {}, buffer.getArrayOfWritePointers(),
                    buffer.getNumChannels(), buffer.getNumSamples(), {}, midiEventScratch_, routing);
    displayTailSeconds_.store(engine_.remainingTailSeconds());
    // The editor takes the peak with exchange, so keep the highest block peak until it reads.
    const float blockPeak = engine_.currentPeak();
    float unread = displayPeak.load(std::memory_order_relaxed);
    while (blockPeak > unread && !displayPeak.compare_exchange_weak(unread, blockPeak, std::memory_order_relaxed)) {}
    displayAutoPhase.store(engine_.currentAutoPhase());
    if (transport.isPlaying || settings.manualBpm > 0)
        displayBeatPhase.store(engine_.currentBeatPhase(), std::memory_order_relaxed);
    displayBpm.store(static_cast<float>(engine_.currentBpm()));
    displayBeatsPerBar.store(static_cast<float>(transport.beatsPerBar));
    displayReversing.store(engine_.isVoiceReversing());
    displayTriggerCount.store(engine_.triggerSerial(), std::memory_order_relaxed);
}

void OpenEdmKickProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    processBlock(buffer, midi);
    buffer.clear();
    displayPeak.store(0);
}

juce::AudioProcessorEditor* OpenEdmKickProcessor::createEditor() {
    return new OpenEdmKickEditor(*this);
}

int OpenEdmKickProcessor::getNumPrograms() {
    return static_cast<int>(openedmkick::presets::factoryPresets().size());
}

void OpenEdmKickProcessor::setCurrentProgram(int index) {
    const auto& presets = openedmkick::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    const bool record = history_ != nullptr && manager != nullptr && manager->isThisTheMessageThread();
    if (record) history_->beginGroup("Preset");
    {
        const std::scoped_lock lock(stateMutex_);
        currentProgram_ = index;
        const auto locked = routing_.lockedValues();
        openedmkick::presets::apply(presets[static_cast<size_t>(index)], parameters);
        routing_.restoreLocked(locked);
        restoredCustomState_.store(false);
        {
            const std::scoped_lock presetLock(userPresetMutex_);
            userPresetName_ = {};
        }
        presetEdited_.store(false);
    }
    if (record) history_->endGroup();
    juce::AudioProcessorListener::ChangeDetails details;
    details.programChanged = true;
    updateHostDisplay(details);
}

juce::String OpenEdmKickProcessor::presetName() {
    if (!restoredCustomState_.load()) return getProgramName(currentProgram_);
    const std::scoped_lock lock(userPresetMutex_);
    return userPresetName_;
}

juce::String OpenEdmKickProcessor::presetDisplayName() {
    const auto name = presetName();
    if (name.isEmpty()) return "Custom sound";
    return name + (presetEdited_.load() ? " *" : "");
}

void OpenEdmKickProcessor::setUserPreset(const juce::String& name) {
    restoredCustomState_.store(true);
    {
        const std::scoped_lock lock(userPresetMutex_);
        userPresetName_ = name;
    }
    presetEdited_.store(false);
}

const juce::String OpenEdmKickProcessor::getProgramName(int index) {
    const auto& presets = openedmkick::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) return {};
    return presets[static_cast<size_t>(index)].name;
}

juce::ValueTree OpenEdmKickProcessor::buildSoundTree() {
    auto state = parameters.copyState();
    routing_.writeState(state);
    state.setProperty("program", currentProgram_, nullptr);
    state.setProperty("presetCatalogVersion", openedmkick::presets::kCatalogVersion, nullptr);
    state.setProperty("programCustom", restoredCustomState_.load(), nullptr);
    {
        const std::scoped_lock lock(userPresetMutex_);
        state.setProperty("presetName", userPresetName_, nullptr);
    }
    state.setProperty("presetEdited", presetEdited_.load(), nullptr);
    syncEditorSequencer();
    juce::ValueTree patterns("LayerPatterns");
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
        juce::ValueTree pattern("Pattern");
        pattern.setProperty("slot", slot, nullptr);
        for (const auto& note : editorSequencer_.notesForSlot(slot)) {
            juce::ValueTree hit("Hit");
            hit.setProperty("step", note.step64, nullptr);
            hit.setProperty("length", note.length64, nullptr);
            hit.setProperty("velocity", note.velocity, nullptr);
            hit.setProperty("reverse", note.reverseCue, nullptr);
            hit.setProperty("layer", note.layer, nullptr);
            hit.setProperty("ratchets", note.ratchets, nullptr);
            pattern.appendChild(hit, nullptr);
        }
        patterns.appendChild(pattern, nullptr);
    }
    state.removeChild(state.getChildWithName("LayerPatterns"), nullptr);
    state.appendChild(patterns, nullptr);
    state.removeChild(state.getChildWithName("ClickSample"), nullptr);
    const int sampleSlot = userPublished_.load(std::memory_order_acquire);
    const auto& user = userSlots_[static_cast<size_t>(sampleSlot)];
    if (!user.samples.empty()) {
        juce::MemoryBlock pcm(user.samples.data(), user.samples.size() * sizeof(float));
        juce::ValueTree sample("ClickSample");
        sample.setProperty("rate", user.sampleRate, nullptr);
        sample.setProperty("name", user.name, nullptr);
        sample.setProperty("data", pcm.toBase64Encoding(), nullptr);
        state.appendChild(sample, nullptr);
    }
    return state;
}

juce::MemoryBlock OpenEdmKickProcessor::encodeTree(const juce::ValueTree& tree) const {
    juce::MemoryBlock block;
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, block);
    return block;
}

juce::MemoryBlock OpenEdmKickProcessor::captureSoundState() {
    const std::scoped_lock lock(stateMutex_);
    return encodeTree(buildSoundTree());
}

void OpenEdmKickProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const std::scoped_lock lock(stateMutex_);
    auto state = buildSoundTree();
    const auto sound = encodeTree(state);
    ab_.setSlot(ab_.active(), sound);
    state.setProperty("abSlot", ab_.active(), nullptr);
    const auto& other = ab_.slot(1 - ab_.active());
    if (other.getSize() > 0) state.setProperty("abOther", other.toBase64Encoding(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

bool OpenEdmKickProcessor::recallSound(const void* data, int sizeInBytes, bool loadCompareSlots, bool forceCustom) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return false;
    auto state = juce::ValueTree::fromXml(*xml);
    std::ranges::for_each(getParameters(), [&](juce::AudioProcessorParameter* parameter) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged != nullptr && !state.getChildWithProperty("id", ranged->paramID).isValid()) {
            juce::ValueTree value("PARAM");
            value.setProperty("id", ranged->paramID, nullptr);
            value.setProperty("value", ranged->convertFrom0to1(ranged->getDefaultValue()), nullptr);
            state.appendChild(value, nullptr);
        }
    });
    const bool hasCurrentCatalog = static_cast<int>(state.getProperty("presetCatalogVersion", 0)) == openedmkick::presets::kCatalogVersion;
    currentProgram_ = hasCurrentCatalog
        ? std::clamp(static_cast<int>(state.getProperty("program", 0)), 0,
                     static_cast<int>(openedmkick::presets::factoryPresets().size()) - 1)
        : 0;
    restoredCustomState_.store(
        !hasCurrentCatalog || static_cast<bool>(state.getProperty("programCustom", false)));
    if (forceCustom) restoredCustomState_.store(true);
    parameters.replaceState(state);
    routing_.restoreState(state);
    {
        const std::scoped_lock lock(userPresetMutex_);
        userPresetName_ = state.getProperty("presetName").toString();
    }
    presetEdited_.store(state.getProperty("presetEdited", false));
    const auto sample = state.getChildWithName("ClickSample");
    if (sample.isValid()) {
        juce::MemoryBlock pcm;
        pcm.fromBase64Encoding(sample.getProperty("data").toString());
        const auto count = static_cast<int>(pcm.getSize() / sizeof(float));
        std::vector<float> samples(static_cast<size_t>(std::max(0, count)));
        if (count > 0) std::memcpy(samples.data(), pcm.getData(), static_cast<size_t>(count) * sizeof(float));
        publishUserSample(std::move(samples), static_cast<double>(sample.getProperty("rate", 44100.0)),
                          sample.getProperty("name").toString());
    } else {
        publishUserSample({}, 44100.0, {});
    }
    {
        const std::scoped_lock lock(stateMutex_);
        editorPresets_.fill(-1);
        syncEditorSequencer();
        const auto patterns = state.getChildWithName("LayerPatterns");
        for (const auto pattern : patterns) {
            const int slot = pattern.getProperty("slot", -1);
            if (slot < 0 || slot >= core::kNumAutoSlots) continue;
            std::vector<core::PianoRollNote> notes;
            for (const auto hit : pattern) {
                if (notes.size() >= core::AutoSequencer::kMaxPatternNotes) break;
                notes.push_back({hit.getProperty("step", 0), hit.getProperty("length", 1),
                    hit.getProperty("velocity", 1.0f), hit.getProperty("reverse", false),
                    hit.getProperty("layer", -1), hit.getProperty("ratchets", 1)});
            }
            editorSequencer_.setNotesForSlot(slot, std::move(notes));
        }
        publishPatterns();
        if (loadCompareSlots) {
            const auto sound = encodeTree(buildSoundTree());
            juce::MemoryBlock other;
            if (state.hasProperty("abOther")) other.fromBase64Encoding(state.getProperty("abOther").toString());
            if (other.getSize() > 0) {
                const int slot = static_cast<int>(state.getProperty("abSlot", 0)) == 0 ? 0 : 1;
                ab_.setActive(slot);
                ab_.setSlot(slot, sound);
                ab_.setSlot(1 - slot, other);
            } else {
                ab_.seed(sound);
            }
        }
    }
    return true;
}

void OpenEdmKickProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (!recallSound(data, sizeInBytes, true, false)) return;
    if (history_ != nullptr) {
        history_->clear();
        history_->selectHistory(abSlot());
    }
}

struct SoundSwapAction : public juce::UndoableAction {
    SoundSwapAction(OpenEdmKickProcessor& processor, juce::MemoryBlock before, juce::MemoryBlock after)
        : processor_(processor), before_(std::move(before)), after_(std::move(after)) {}

    bool perform() override {
        if (first_) {
            first_ = false;
            return true;
        }
        return processor_.recallSound(after_.getData(), static_cast<int>(after_.getSize()), false, false);
    }

    bool undo() override {
        return processor_.recallSound(before_.getData(), static_cast<int>(before_.getSize()), false, false);
    }

    int getSizeInUnits() override { return 1; }

    OpenEdmKickProcessor& processor_;
    juce::MemoryBlock before_;
    juce::MemoryBlock after_;
    bool first_ = true;
};

bool OpenEdmKickProcessor::undo() { return history_ != nullptr && history_->undo(); }
bool OpenEdmKickProcessor::redo() { return history_ != nullptr && history_->redo(); }
bool OpenEdmKickProcessor::canUndo() const { return history_ != nullptr && history_->canUndo(); }
bool OpenEdmKickProcessor::canRedo() const { return history_ != nullptr && history_->canRedo(); }

void OpenEdmKickProcessor::selectAbSlot(int slot) {
    const int next = slot == 0 ? 0 : 1;
    if (next == ab_.active()) return;
    juce::MemoryBlock restored;
    {
        const std::scoped_lock lock(stateMutex_);
        ab_.setSlot(ab_.active(), encodeTree(buildSoundTree()));
        ab_.setActive(next);
        restored = ab_.slot(next);
    }
    if (restored.getSize() > 0)
        recallSound(restored.getData(), static_cast<int>(restored.getSize()), false, false);
    if (history_ != nullptr) history_->selectHistory(next);
}

void OpenEdmKickProcessor::copyAbSlot(bool aToB) {
    const int source = aToB ? 0 : 1;
    const int destination = aToB ? 1 : 0;
    juce::MemoryBlock copied;
    bool apply = false;
    {
        const std::scoped_lock lock(stateMutex_);
        ab_.setSlot(ab_.active(), encodeTree(buildSoundTree()));
        copied = ab_.slot(source);
        ab_.setSlot(destination, copied);
        apply = ab_.active() == destination;
    }
    if (apply && copied.getSize() > 0) recallSound(copied.getData(), static_cast<int>(copied.getSize()), false, false);
    if (history_ != nullptr) history_->clearHistory(destination);
}

bool OpenEdmKickProcessor::applySoundState(const juce::MemoryBlock& state, const juce::String& userPresetName) {
    return changeSound("Sound", [&] {
        if (!recallSound(state.getData(), static_cast<int>(state.getSize()), false, userPresetName.isNotEmpty())) return false;
        if (userPresetName.isNotEmpty()) setUserPreset(userPresetName);
        return true;
    });
}

// Records the change as one undo step when it runs on the message thread.
bool OpenEdmKickProcessor::changeSound(const juce::String& name, const std::function<bool()>& change) {
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    const bool record = history_ != nullptr && manager != nullptr && manager->isThisTheMessageThread();
    juce::MemoryBlock before;
    if (record) before = captureSoundState();
    if (!change()) return false;
    if (!record) return true;
    history_->manager().beginNewTransaction(name);
    history_->manager().perform(new SoundSwapAction(*this, std::move(before), captureSoundState()));
    return true;
}

void OpenEdmKickProcessor::publishUserSample(std::vector<float> samples, double sampleRate, juce::String name) {
    if (samples.size() > 96000) samples.resize(96000);
    const int published = userPublished_.load(std::memory_order_acquire);
    const int seen = userAudioSeen_.load(std::memory_order_acquire);
    int slot = (published + 1) % 3;
    if (slot == seen) slot = (published + 2) % 3;
    auto& destination = userSlots_[static_cast<size_t>(slot)];
    destination.samples = std::move(samples);
    destination.sampleRate = sampleRate > 1.0 ? sampleRate : 44100.0;
    destination.name = std::move(name);
    userPublished_.store(slot, std::memory_order_release);
}

void OpenEdmKickProcessor::selectClickSource(const float*& data, int& length, double& rate) const noexcept {
    const int source = std::clamp(static_cast<int>(values_.clickSampleSource->load()), 0, core::kUserClickSource);
    if (source < core::kUserClickSource) {
        const auto& built = core::builtInClick(static_cast<core::BuiltInClick>(source));
        data = built.samples.data();
        length = static_cast<int>(built.samples.size());
        rate = built.sampleRate;
        return;
    }
    const int slot = userPublished_.load(std::memory_order_acquire);
    const auto& user = userSlots_[static_cast<size_t>(slot)];
    data = user.samples.data();
    length = static_cast<int>(user.samples.size());
    rate = user.sampleRate;
}

bool OpenEdmKickProcessor::loadUserClickSample(const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& name) {
    if (buffer.getNumSamples() < 2 || buffer.getNumChannels() < 1 || sampleRate < 1000.0) return false;
    const int count = std::min(buffer.getNumSamples(), 96000);
    std::vector<float> mono(static_cast<size_t>(count));
    const float scale = 1.0f / static_cast<float>(buffer.getNumChannels());
    for (int index = 0; index < count; ++index) {
        float sum = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) sum += buffer.getSample(channel, index);
        mono[static_cast<size_t>(index)] = sum * scale;
    }
    const int fade = std::min(32, count / 8);
    for (int index = 0; index < fade; ++index) {
        const float window = static_cast<float>(index) / static_cast<float>(fade);
        mono[static_cast<size_t>(index)] *= window;
        mono[static_cast<size_t>(count - 1 - index)] *= window;
    }
    publishUserSample(std::move(mono), sampleRate, name);
    if (auto* parameter = parameters.getParameter(params::kClickSampleSource)) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(core::kUserClickSource)));
        parameter->endChangeGesture();
    }
    return true;
}

bool OpenEdmKickProcessor::loadUserClickSample(const juce::File& file) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples < 2) return false;
    const int count = std::min(static_cast<int>(reader->lengthInSamples), 96000);
    juce::AudioBuffer<float> buffer(static_cast<int>(std::max<unsigned>(reader->numChannels, 1u)), count);
    reader->read(&buffer, 0, count, 0, true, true);
    return loadUserClickSample(buffer, reader->sampleRate, file.getFileNameWithoutExtension());
}

bool OpenEdmKickProcessor::renderCurrentHit(juce::AudioBuffer<float>& destination) const {
    constexpr double rate = hitSampleRate;
    core::KickEngine engine;
    engine.prepare(rate);
    std::vector<float> clickCopy;
    double clickRate = rate;
    const float* live = nullptr;
    int liveLength = 0;
    selectClickSource(live, liveLength, clickRate);
    if (live != nullptr && liveLength > 1) clickCopy.assign(live, live + liveLength);
    engine.setClickSampleSource(clickCopy.data(), static_cast<int>(clickCopy.size()), clickRate);
    auto settings = currentEngineSettings();
    settings.autoEnabled = false;
    settings.masterBypass = false;
    const double bpm = std::clamp(static_cast<double>(values_.tempoBpm->load()), 40.0, 240.0);
    const int signature = std::clamp(static_cast<int>(values_.timeSignature->load()), 0,
                                     static_cast<int>(params::kTimeSignatures.size()) - 1);
    const double beats = params::beatsPerBarForSignature(signature);
    const int note = std::clamp(displayMidiNote.load(), 0, 127);
    float velocity = displayVelocity.load();
    if (!(velocity > 0.0f)) velocity = 1.0f;
    constexpr int block = 256;
    const int maximum = static_cast<int>(rate * 8.0);
    juce::AudioBuffer<float> rendered(2, maximum);
    rendered.clear();
    int written = 0;
    int silent = 0;
    bool started = false;
    while (written + block <= maximum) {
        float* channels[]{ rendered.getWritePointer(0) + written, rendered.getWritePointer(1) + written };
        if (!started) {
            const core::Transport transport{ bpm, 0.0, beats, true };
            const std::array<core::MidiTrigger, 1> trigger{{ { 0, note, velocity, false, 0 } }};
            engine.process(settings, transport, trigger, channels, 2, block);
            started = true;
        } else {
            const core::Transport transport{ bpm, std::nullopt, beats, true };
            engine.process(settings, transport, {}, channels, 2, block);
        }
        const float magnitude = std::max(rendered.getMagnitude(0, written, block), rendered.getMagnitude(1, written, block));
        written += block;
        const bool tail = engine.remainingTailSeconds() > 0.003 || engine.activeNoteCount() > 0;
        if (magnitude < 1.0e-4f && !tail) silent += block;
        else silent = 0;
        if (written > static_cast<int>(rate * 0.05) && silent > static_cast<int>(rate * 0.05)) break;
    }
    const int keep = std::clamp(written - std::max(0, silent - static_cast<int>(rate * 0.02)), block, written);
    destination.setSize(2, keep);
    for (int channel = 0; channel < 2; ++channel)
        destination.copyFrom(channel, 0, rendered, channel, 0, keep);
    return destination.getNumSamples() > 0;
}

juce::File OpenEdmKickProcessor::renderHitToTemporaryWav() const {
    juce::AudioBuffer<float> hit;
    if (!renderCurrentHit(hit)) return {};
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openKicker-hits");
    if (!directory.createDirectory()) return {};
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::hours(12.0);
    for (const auto& old : directory.findChildFiles(juce::File::findFiles, false, "openKicker-*.wav"))
        if (old.getLastModificationTime() < cutoff) old.deleteFile();
    const int note = std::clamp(displayMidiNote.load(), 0, 127);
    const auto noteText = juce::String(core::noteName(note).data()) + juce::String(core::noteOctave(note));
    const auto file = directory.getChildFile("openKicker-" + noteText.replaceCharacter('#', 's') + "-"
        + juce::String(juce::Time::currentTimeMillis()) + ".wav");
    if (!suite::writeStereoWav(file, hit, hitSampleRate, 24)) return {};
    return file;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmKickProcessor();
}
