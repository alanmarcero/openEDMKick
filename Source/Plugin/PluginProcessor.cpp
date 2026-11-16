#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"

#include <algorithm>
#include <ranges>

namespace core = openedmkick::core;
namespace params = openedmkick::params;

OpenEdmKickProcessor::OpenEdmKickProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openEDMKick", params::createParameterLayout()),
      values_([this] {
          const auto raw = [this](const juce::String& id) { return parameters.getRawParameterValue(id); };
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
                                  layerRaw(params::kLayerDriveOffset), layerRaw(params::kLayerToneScale),
                                  layerRaw(params::kLayerSweepScale), layerRaw(params::kLayerSolo),
                                  layerRaw(params::kLayerPan), layerRaw(params::kLayerPolarity) };
          };
          return ParameterValues{
              raw(params::kSweepOctaves), raw(params::kSweepMs), raw(params::kSweepShape),
              raw(params::kSnapOctaves), raw(params::kSnapMs), raw(params::kBodyTone),
              raw(params::kBodyDrive), raw(params::kSaturation), raw(params::kKnockHz),
              raw(params::kKnockDb), raw(params::kGrit), raw(params::kToneHz),
              raw(params::kSubLevel), raw(params::kStartPhase), raw(params::kClickType),
              raw(params::kClickLevel), raw(params::kClickHz), raw(params::kClickMs),
              raw(params::kAttackMs), raw(params::kHoldMs), raw(params::kDecayMs),
              raw(params::kDecayShape), raw(params::kLowCutHz), raw(params::kClip),
              raw(params::kClockSync), raw(params::kTempoBpm), raw(params::kTimeSignature),
              raw(params::kAutoEnabled), raw(params::kAutoBypass), raw(params::kAutoSlot),
              { slotParams(0), slotParams(1), slotParams(2), slotParams(3) },
              { layerParams(0), layerParams(1), layerParams(2), layerParams(3) },
              raw(params::kReverseMode), raw(params::kReverseMix), raw(params::kDrive),
              raw(params::kCrushRate), raw(params::kCrushBits), raw(params::kCrushMix),
              raw(params::kStutterRate), raw(params::kStutterMix), raw(params::kGateRate),
              raw(params::kGateMix), raw(params::kPumpRate), raw(params::kPumpMix),
              raw(params::kHighPass), raw(params::kLowPass), raw(params::kHighPassSyncEnabled),
              raw(params::kHighPassSyncRate), raw(params::kLowPassSyncEnabled), raw(params::kLowPassSyncRate),
              raw(params::kOutput),
              raw(params::kLimiter), raw(params::kCeiling), raw(params::kMasterBypass)
          };
      }()) {
    setLatencySamples(core::KickEngine::kLatencySamples);
    engine_.prepare(48000.0);
    openedmkick::presets::apply(openedmkick::presets::factoryPresets().front(), parameters);
}

void OpenEdmKickProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const std::scoped_lock lock(stateMutex_);
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
        values_.bodyDrive->load(), static_cast<core::Saturation>(choice(values_.saturation)),
        values_.knockHz->load(), values_.knockDb->load(), values_.grit->load(),
        values_.toneHz->load(), values_.subLevel->load(), values_.startPhase->load(),
        static_cast<core::ClickType>(choice(values_.clickType)), values_.clickLevel->load(),
        values_.clickHz->load(), values_.clickMs->load(), values_.attackMs->load(),
        values_.holdMs->load(), values_.decayMs->load(), values_.decayShape->load(),
        values_.lowCutHz->load(), values_.clip->load()
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
            layer.driveOffset->load(), layer.toneScale->load(), layer.sweepScale->load(),
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

std::vector<float> OpenEdmKickProcessor::previewKickWaveform() const {
    constexpr double previewRate = 24000.0;
    auto wave = core::renderLayeredKick(currentSynthSettings(), currentLayerSettings(), previewRate, currentMidiNote());
    core::ToneFilter filter;
    const float highPass = values_.highPass->load();
    const float lowPass = values_.lowPass->load();
    std::ranges::for_each(wave, [&](float& sample) {
        sample = filter.process(sample, highPass, lowPass, previewRate);
    });
    return wave;
}

std::vector<core::PianoRollNote> OpenEdmKickProcessor::currentPianoRollNotes() const {
    const std::scoped_lock lock(stateMutex_);
    syncEditorSequencer();
    return editorSequencer_.notesForSlot(currentAutoSlot());
}

void OpenEdmKickProcessor::syncEditorSequencer() const {
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
        const auto index = static_cast<size_t>(slot);
        const auto settings = autoSlotSettings(slot);
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
    const std::scoped_lock lock(stateMutex_);
    syncEditorSequencer();
    const int slot = currentAutoSlot();
    editorSequencer_.toggleNoteAt(slot, step64, length64, autoSlotSettings(slot).bars, layer, ratchets);
    publishPatterns();
}

void OpenEdmKickProcessor::cyclePianoRollRatchets(int step64, int layer) {
    const std::scoped_lock lock(stateMutex_);
    syncEditorSequencer();
    editorSequencer_.cycleRatchetsAt(currentAutoSlot(), step64, layer);
    publishPatterns();
}

void OpenEdmKickProcessor::syncSequencerPresets(
    const std::array<core::AutoSlotSettings, core::kNumAutoSlots>& slots) {
    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int slot) {
        const auto index = static_cast<size_t>(slot);
        if (slots[index].fillPreset != lastSlotPreset_[index] || slots[index].bars != lastSlotBars_[index]) {
            engine_.sequencer().loadPresetIntoSlot(slot, slots[index].fillPreset, slots[index].bars);
            lastSlotPreset_[index] = slots[index].fillPreset;
            lastSlotBars_[index] = slots[index].bars;
        }
    });
}

juce::AudioProcessorParameter* OpenEdmKickProcessor::getBypassParameter() const {
    return parameters.getParameter(params::kMasterBypass);
}

void OpenEdmKickProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };

    const std::array<core::AutoSlotSettings, core::kNumAutoSlots> slotSettings{
        autoSlotSettings(0), autoSlotSettings(1), autoSlotSettings(2), autoSlotSettings(3)
    };
    syncSequencerPresets(slotSettings);
    if ((middlePattern_.load(std::memory_order_acquire) & 4) != 0) {
        readPattern_ = middlePattern_.exchange(readPattern_, std::memory_order_acq_rel) & 3;
        auto& snapshot = patternSnapshots_[static_cast<size_t>(readPattern_)];
        for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
            const auto index = static_cast<size_t>(slot);
            if (snapshot.presets[index] != slotSettings[index].fillPreset || snapshot.bars[index] != slotSettings[index].bars) continue;
            engine_.sequencer().swapSlotPattern(slot, snapshot.sequencer);
        }
    }

    core::EngineSettings settings{};
    settings.synth = currentSynthSettings();
    settings.layers = currentLayerSettings();
    settings.autoEnabled = isOn(values_.autoEnabled);
    settings.autoBypass = isOn(values_.autoBypass);
    settings.activeAutoSlot = currentAutoSlot();
    settings.autoSlots = slotSettings;
    settings.effects = core::EffectSettings{
        static_cast<core::ReverseSyncMode>(std::clamp(choice(values_.reverseMode), 0, 4)),
        values_.reverseMix->load(), values_.drive->load(), values_.crushRate->load(),
        choice(values_.crushBits), values_.crushMix->load(), choice(values_.stutterRate),
        values_.stutterMix->load(), choice(values_.gateRate), values_.gateMix->load(),
        choice(values_.pumpRate), values_.pumpMix->load(),
        juce::Decibels::decibelsToGain(values_.output->load()), isOn(values_.limiter),
        juce::Decibels::decibelsToGain(values_.ceiling->load()), values_.highPass->load(),
        values_.lowPass->load(), isOn(values_.highPassSyncEnabled), choice(values_.highPassSyncRate),
        isOn(values_.lowPassSyncEnabled), choice(values_.lowPassSyncRate)
    };
    settings.masterBypass = isOn(values_.masterBypass);

    const bool syncToHost = isOn(values_.clockSync);
    const double manualBpm = static_cast<double>(values_.tempoBpm->load());
    const int signatureChoice = choice(values_.timeSignature);
    const double manualBeatsPerBar = params::beatsPerBarForSignature(signatureChoice);
    const core::Transport transport = [&] {
        if (!syncToHost) return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar, true };
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
            midiEventScratch_.emplace_back(core::MidiTrigger{ sample, note, message.getFloatVelocity(), false });
            displayMidiNote.store(note, std::memory_order_relaxed);
            displayMidiHz.store(core::noteFrequencyHz(note), std::memory_order_relaxed);
            return;
        }
        if (!message.isController()) return;
        const int controller = message.getControllerNumber();
        if (controller != 7 && controller != 11 && controller != 20 && controller != 21 && controller != 22 && controller != 74
            && controller != 120 && controller != 121 && controller != 123) return;
        midiEventScratch_.emplace_back(core::MidiCcEvent{ sample, controller, message.getControllerValue() });
    });

    if (auditionPending_.exchange(false)) {
        const auto afterSampleZero = std::upper_bound(midiEventScratch_.begin(), midiEventScratch_.end(), 0,
            [](int sample, const core::MidiInputEvent& input) {
                return sample < std::visit([](const auto& event) { return event.sampleOffset; }, input);
            });
        midiEventScratch_.insert(afterSampleZero, core::MidiTrigger{ 0, core::kDefaultMidiNote, 1.0f, true });
    }

    buffer.clear();
    engine_.process(settings, transport, {}, buffer.getArrayOfWritePointers(),
                    buffer.getNumChannels(), buffer.getNumSamples(), {}, midiEventScratch_);
    displayPeak.store(engine_.currentPeak());
    displayAutoPhase.store(engine_.currentAutoPhase());
    displayBeatPhase.store(engine_.currentBeatPhase());
    displayBpm.store(static_cast<float>(transport.bpm));
    displayBeatsPerBar.store(static_cast<float>(transport.beatsPerBar));
    displayReversing.store(engine_.isVoiceReversing());
    displayTriggerCount.store(engine_.triggerSerial(), std::memory_order_relaxed);
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
    {
        const std::scoped_lock lock(stateMutex_);
        currentProgram_ = index;
        openedmkick::presets::apply(presets[static_cast<size_t>(index)], parameters);
        restoredCustomState_.store(false);
    }
    juce::AudioProcessorListener::ChangeDetails details;
    details.programChanged = true;
    updateHostDisplay(details);
}

const juce::String OpenEdmKickProcessor::getProgramName(int index) {
    const auto& presets = openedmkick::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) return {};
    return presets[static_cast<size_t>(index)].name;
}

void OpenEdmKickProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const std::scoped_lock lock(stateMutex_);
    auto state = parameters.copyState();
    state.setProperty("program", currentProgram_, nullptr);
    state.setProperty("presetCatalogVersion", 3, nullptr);
    state.setProperty("programCustom", restoredCustomState_.load(), nullptr);
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
    const auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void OpenEdmKickProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
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
    const bool hasV3Catalog = static_cast<int>(state.getProperty("presetCatalogVersion", 0)) == 3;
    currentProgram_ = hasV3Catalog
        ? std::clamp(static_cast<int>(state.getProperty("program", 0)), 0,
                     static_cast<int>(openedmkick::presets::factoryPresets().size()) - 1)
        : 0;
    restoredCustomState_.store(
        !hasV3Catalog || static_cast<bool>(state.getProperty("programCustom", false)));
    parameters.replaceState(state);
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
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmKickProcessor();
}
