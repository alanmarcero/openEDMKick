#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <algorithm>
#include <ranges>

namespace core = openedmkick::core;
namespace params = openedmkick::params;

OpenEdmKickProcessor::OpenEdmKickProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openEDMKick", params::createParameterLayout()),
      values_([this] {
          const auto raw = [this](const juce::String& id) { return parameters.getRawParameterValue(id); };
          const auto slotParams = [&](int slot) {
              const auto sRaw = [&](const char* key) { return raw(params::autoSlotId(slot, key)); };
              return AutoSlotParams{
                  sRaw(params::kAutoBars),
                  sRaw(params::kAutoFillPreset),
                  sRaw(params::kAutoGrid),
                  sRaw(params::kAutoTailShorten),
                  sRaw(params::kAutoPitchRise),
                  sRaw(params::kAutoCrushSweep)
              };
          };
          return ParameterValues{
              raw(params::kNote), raw(params::kOctave), raw(params::kFineTune),
              raw(params::kStartPitch), raw(params::kPitchDecay), raw(params::kPitchCurve),
              raw(params::kTailLength), raw(params::kTailCurve), raw(params::kSubDrive),
              raw(params::kClickLevel), raw(params::kClickFreq), raw(params::kPunch),
              raw(params::kSnareLevel), raw(params::kSnareTone), raw(params::kSnareDecay),
              raw(params::kClapLevel), raw(params::kClapSpread), raw(params::kClapTone),
              raw(params::kHatLevel), raw(params::kHatTone), raw(params::kHatDecay),
              raw(params::kLockIn), raw(params::kClockSync), raw(params::kTempoBpm),
              raw(params::kTimeSignature), raw(params::kAutoEnabled), raw(params::kAutoBypass),
              raw(params::kAutoSlot),
              { slotParams(0), slotParams(1), slotParams(2), slotParams(3) },
              raw(params::kReverseMode), raw(params::kReverseMix), raw(params::kDrive),
              raw(params::kCrushRate), raw(params::kCrushBits), raw(params::kCrushMix),
              raw(params::kStutterRate), raw(params::kStutterMix), raw(params::kGateRate),
              raw(params::kGateMix), raw(params::kPumpRate), raw(params::kPumpMix),
              raw(params::kOutput), raw(params::kLimiter), raw(params::kCeiling),
              raw(params::kMasterBypass)
          };
      }()) {
    setLatencySamples(core::KickEngine::kLatencySamples);
    engine_.prepare(48000.0);
}

void OpenEdmKickProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const std::scoped_lock lock(stateMutex_);
    engine_.prepare(sampleRate);
    midiScratch_.reserve(static_cast<size_t>(std::max(64, samplesPerBlock)));
    // An existing Lock In WAV is resampled on playback. Re-rendering here would
    // replace the frozen hit whenever the host changes sample rate.
    if (values_.lockIn->load() > 0.5f && !engine_.isKickLocked()) {
        engine_.lockCurrentKick(currentSynthSettings());
    }
    displayLockedIn.store(engine_.isKickLocked());
}

bool OpenEdmKickProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

int OpenEdmKickProcessor::currentAutoSlot() const noexcept {
    return std::clamp(static_cast<int>(values_.autoSlot->load()), 0, core::kNumAutoSlots - 1);
}

core::KickSynthSettings OpenEdmKickProcessor::currentSynthSettings() const noexcept {
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    return core::KickSynthSettings{
        choice(values_.note),
        choice(values_.octave),
        values_.fineTune->load(),
        values_.startPitch->load(),
        values_.pitchDecay->load(),
        values_.pitchCurve->load(),
        values_.tailLength->load(),
        values_.tailCurve->load(),
        values_.subDrive->load(),
        values_.clickLevel->load(),
        values_.clickFreq->load(),
        values_.punch->load(),
        values_.snareLevel->load(),
        values_.snareTone->load(),
        values_.snareDecay->load(),
        values_.clapLevel->load(),
        values_.clapSpread->load(),
        values_.clapTone->load(),
        values_.hatLevel->load(),
        values_.hatTone->load(),
        values_.hatDecay->load()
    };
}

core::AutoSlotSettings OpenEdmKickProcessor::autoSlotSettings(int slot) const noexcept {
    const int safeSlot = std::clamp(slot, 0, core::kNumAutoSlots - 1);
    const auto& s = values_.autoSlots[static_cast<size_t>(safeSlot)];
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    const auto barIdx = static_cast<size_t>(
        std::clamp(choice(s.bars), 0, static_cast<int>(core::kAutoBarCounts.size()) - 1));
    return core::AutoSlotSettings{
        core::kAutoBarCounts[barIdx],
        std::clamp(choice(s.fillPreset), 0, core::kNumFillPresets - 1),
        std::clamp(choice(s.grid), 0, static_cast<int>(core::kGridStepSizes64.size()) - 1),
        s.tailShorten->load(),
        s.pitchRise->load(),
        s.crushSweep->load()
    };
}

core::AutoSlotSettings OpenEdmKickProcessor::currentAutoSlotSettings() const noexcept {
    return autoSlotSettings(currentAutoSlot());
}

std::vector<float> OpenEdmKickProcessor::previewKickWaveform() const {
    const std::scoped_lock lock(stateMutex_);
    if (engine_.isKickLocked() && !engine_.wavCache().samples().empty()) {
        return engine_.wavCache().samples();
    }
    return core::renderKickWaveform(currentSynthSettings(), 24000.0, 0.0f);
}

std::vector<core::PianoRollNote> OpenEdmKickProcessor::currentPianoRollNotes() const {
    const std::scoped_lock lock(stateMutex_);
    return engine_.sequencer().notesForSlot(currentAutoSlot());
}

void OpenEdmKickProcessor::togglePianoRollNote(int step64, int length64, int semitoneOffset) {
    const std::scoped_lock lock(stateMutex_);
    const int slot = currentAutoSlot();
    const auto settings = autoSlotSettings(slot);
    engine_.sequencer().toggleNoteAt(slot, step64, length64, semitoneOffset, settings.bars);
}

void OpenEdmKickProcessor::syncSequencerPresets(
    const std::array<core::AutoSlotSettings, core::kNumAutoSlots>& slots) {
    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int slot) {
        const auto idx = static_cast<size_t>(slot);
        const int preset = slots[idx].fillPreset;
        const int bars = slots[idx].bars;
        if (preset != lastSlotPreset_[idx] || bars != lastSlotBars_[idx]) {
            engine_.sequencer().loadPresetIntoSlot(slot, preset, bars);
            lastSlotPreset_[idx] = preset;
            lastSlotBars_[idx] = bars;
        }
    });
}

juce::AudioProcessorParameter* OpenEdmKickProcessor::getBypassParameter() const {
    return parameters.getParameter(params::kMasterBypass);
}

void OpenEdmKickProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const std::scoped_lock lock(stateMutex_);

    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };

    const std::array<core::AutoSlotSettings, core::kNumAutoSlots> slotSettings{
        autoSlotSettings(0), autoSlotSettings(1), autoSlotSettings(2), autoSlotSettings(3)
    };
    syncSequencerPresets(slotSettings);

    const core::EngineSettings settings{
        currentSynthSettings(),
        isOn(values_.lockIn),
        isOn(values_.autoEnabled),
        isOn(values_.autoBypass),
        currentAutoSlot(),
        slotSettings,
        core::EffectSettings{
            static_cast<core::ReverseSyncMode>(std::clamp(choice(values_.reverseMode), 0, 4)),
            values_.reverseMix->load(),
            values_.drive->load(),
            values_.crushRate->load(),
            choice(values_.crushBits),
            values_.crushMix->load(),
            choice(values_.stutterRate),
            values_.stutterMix->load(),
            choice(values_.gateRate),
            values_.gateMix->load(),
            choice(values_.pumpRate),
            values_.pumpMix->load(),
            juce::Decibels::decibelsToGain(values_.output->load()),
            isOn(values_.limiter),
            juce::Decibels::decibelsToGain(values_.ceiling->load())
        },
        isOn(values_.masterBypass)
    };

    const bool syncToHost = isOn(values_.clockSync);
    const double manualBpm = static_cast<double>(values_.tempoBpm->load());
    const int sigChoice = choice(values_.timeSignature);
    const double manualBeatsPerBar = params::beatsPerBarForSignature(sigChoice);

    const core::Transport transport = [&] {
        if (!syncToHost) {
            return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar, true };
        }
        auto* playHead = getPlayHead();
        if (playHead == nullptr) {
            return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar, true };
        }
        const auto position = playHead->getPosition();
        if (!position.hasValue()) {
            return core::Transport{ manualBpm, std::nullopt, manualBeatsPerBar, true };
        }
        const double bpm = position->getBpm().orFallback(manualBpm);
        double beatsPerBar = manualBeatsPerBar;
        if (sigChoice == 0 && position->getTimeSignature().hasValue()) {
            const auto sig = *position->getTimeSignature();
            if (sig.numerator > 0 && sig.denominator > 0) {
                beatsPerBar = static_cast<double>(sig.numerator) * (4.0 / static_cast<double>(sig.denominator));
            }
        }
        const bool playing = position->getIsPlaying();
        if (!playing || !position->getPpqPosition().hasValue()) {
            return core::Transport{ bpm, std::nullopt, beatsPerBar, playing };
        }
        return core::Transport{ bpm, *position->getPpqPosition(), beatsPerBar, true };
    }();

    midiScratch_.clear();
    std::ranges::for_each(midi, [&](const juce::MidiMessageMetadata& meta) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && msg.getFloatVelocity() > 0.0f) {
            midiScratch_.push_back(core::MidiTrigger{
                std::clamp(meta.samplePosition, 0, std::max(0, buffer.getNumSamples() - 1)),
                msg.getNoteNumber(),
                msg.getFloatVelocity(),
                false
            });
        }
    });

    if (auditionPending_.exchange(false)) {
        midiScratch_.push_back(core::MidiTrigger{ 0, 60, 1.0f, true });
    }

    buffer.clear();
    engine_.process(
        settings,
        transport,
        midiScratch_,
        buffer.getArrayOfWritePointers(),
        buffer.getNumChannels(),
        buffer.getNumSamples());

    displayPeak.store(engine_.currentPeak());
    displayAutoPhase.store(engine_.currentAutoPhase());
    displayBeatPhase.store(engine_.currentBeatPhase());
    displayBpm.store(static_cast<float>(transport.bpm));
    displayBeatsPerBar.store(static_cast<float>(transport.beatsPerBar));
    displayLockedIn.store(engine_.isKickLocked());
    displayReversing.store(engine_.isVoiceReversing());
}

juce::AudioProcessorEditor* OpenEdmKickProcessor::createEditor() {
    return new OpenEdmKickEditor(*this);
}

int OpenEdmKickProcessor::getNumPrograms() {
    return static_cast<int>(openedmkick::presets::factoryPresets().size());
}

void OpenEdmKickProcessor::setCurrentProgram(int index) {
    const auto& presets = openedmkick::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        return;
    }
    currentProgram_ = index;
    openedmkick::presets::apply(presets[static_cast<size_t>(index)], parameters);
    juce::AudioProcessorListener::ChangeDetails details;
    details.programChanged = true;
    updateHostDisplay(details);
}

const juce::String OpenEdmKickProcessor::getProgramName(int index) {
    const auto& presets = openedmkick::presets::factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) {
        return {};
    }
    return presets[static_cast<size_t>(index)].name;
}

void OpenEdmKickProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const std::scoped_lock lock(stateMutex_);
    auto state = parameters.copyState();
    state.setProperty("program", currentProgram_, nullptr);
    if (engine_.isKickLocked() && !engine_.wavCache().wavBytes().empty()) {
        const auto& bytes = engine_.wavCache().wavBytes();
        const auto b64 = juce::Base64::toBase64(bytes.data(), bytes.size());
        state.setProperty("lockedWavBase64", b64, nullptr);
    }
    const auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void OpenEdmKickProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) {
        return;
    }
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
    currentProgram_ = state.getProperty("program", 0);
    parameters.replaceState(state);

    const std::scoped_lock lock(stateMutex_);
    lastSlotPreset_.fill(-1);
    const auto b64 = state.getProperty("lockedWavBase64", juce::String{}).toString();
    if (b64.isNotEmpty()) {
        juce::MemoryOutputStream decoded;
        if (juce::Base64::convertFromBase64(decoded, b64)) {
            const auto* ptr = static_cast<const uint8_t*>(decoded.getData());
            std::vector<uint8_t> wavBytes(ptr, ptr + decoded.getDataSize());
            engine_.restoreLockedWav(wavBytes);
        }
    }
    displayLockedIn.store(engine_.isKickLocked());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmKickProcessor();
}
