#include "PluginProcessor.h"

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

OpenEdmKickProcessor::OpenEdmKickProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openEDMKick", params::createParameterLayout()),
      routing_(parameters),
      values_([this] {
          const auto raw = [this](const juce::String& id) { return routing_.effectiveRaw(id); };
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
              raw(params::kReverseMode), raw(params::kReverseMix),
              raw(params::kDistType), raw(params::kDistDrive), raw(params::kDistTone), raw(params::kDistMix),
              raw(params::kCompThresh), raw(params::kCompRatio), raw(params::kCompAttack),
              raw(params::kCompRelease), raw(params::kCompMakeup), raw(params::kCompMix),
              raw(params::kReverbSize), raw(params::kReverbDecay), raw(params::kReverbDamp),
              raw(params::kReverbGate), raw(params::kReverbMix),
              raw(params::kHighPass), raw(params::kLowPass), raw(params::kHighPassSyncEnabled),
              raw(params::kHighPassSyncRate), raw(params::kLowPassSyncEnabled), raw(params::kLowPassSyncRate),
              raw(params::kOutput),
              raw(params::kLimiterEnabled), raw(params::kCeiling),
              raw(params::kAmpEnabled), raw(params::kAmpAttack), raw(params::kAmpDecay), raw(params::kAmpSustain), raw(params::kAmpRelease), raw(params::kReverseEnabled), raw(params::kMidiVolume), raw(params::kMidiExpression), raw(params::kMidiBrightness), raw(params::kHighPassMidi), raw(params::kLowPassMidi),
              raw(params::kSubOctave), raw(params::kClickPunch), raw(params::kStereoWidth),
              raw(params::kEqLowFreq), raw(params::kEqLowDb), raw(params::kEqMidFreq),
              raw(params::kEqMidDb), raw(params::kEqHighFreq), raw(params::kEqHighDb),
              raw(params::kKeyTracking)
          };
      }()) {
    setLatencySamples(core::KickEngine::kLatencySamples);
    engine_.prepare(48000.0);
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
    engine_.prepare(sampleRate);
    midiEventScratch_.reserve(kMaxMidiEvents);
    juce::ignoreUnused(samplesPerBlock);
}

bool OpenEdmKickProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

openedmkick::core::KickSettings OpenEdmKickProcessor::currentSynthSettings() const noexcept {
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    return openedmkick::core::KickSettings{
        values_.sweepOctaves->load(), values_.sweepMs->load(), values_.sweepShape->load(),
        values_.snapOctaves->load(), values_.snapMs->load(), values_.bodyTone->load(),
        values_.knockHz->load(), values_.knockDb->load(),
        values_.toneHz->load(), values_.subLevel->load(), values_.startPhase->load(),
        static_cast<openedmkick::core::ClickType>(choice(values_.clickType)), values_.clickLevel->load(),
        values_.clickHz->load(), values_.clickMs->load(), values_.attackMs->load(),
        values_.holdMs->load(), values_.decayMs->load(), values_.decayShape->load(),
        values_.lowCutHz->load(),
        values_.ampEnabled->load() > 0.5f, values_.ampAttackMs->load(), values_.ampDecayMs->load(),
        values_.ampSustain->load(), values_.ampReleaseMs->load(),
        values_.subOctave->load(), values_.clickPunch->load(), values_.keyTracking->load()
    };
}

std::array<openedmkick::core::DrumLayerSettings, openedmkick::core::kNumSoundLayers>
OpenEdmKickProcessor::currentLayerSettings() const noexcept {
    return {{ { true, openedmkick::core::DrumSoundType::kick, 1.0f, 0.0f, 300.0f } }};
}

juce::AudioProcessorParameter* OpenEdmKickProcessor::getBypassParameter() const {
    return nullptr;
}

core::EngineSettings OpenEdmKickProcessor::currentEngineSettings() const noexcept {
    const auto isOn = [](const std::atomic<float>* value) { return value->load() > 0.5f; };
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
    core::EngineSettings settings{};
    settings.synth = currentSynthSettings();
    settings.layers = currentLayerSettings();
    settings.effects = core::EffectSettings{
        static_cast<core::ReverseSyncMode>(std::clamp(choice(values_.reverseMode), 0, 4)),
        values_.reverseMix->load(),
        static_cast<core::DistortionType>(std::clamp(choice(values_.distType), 0, 3)),
        values_.distDrive->load(), values_.distTone->load(), values_.distMix->load(),
        core::CompressorSettings{
            values_.compThresh->load(), values_.compRatio->load(), values_.compAttack->load(),
            values_.compRelease->load(), values_.compMakeup->load(), values_.compMix->load()
        },
        values_.reverbSize->load(), values_.reverbDecay->load(), values_.reverbDamp->load(),
        values_.reverbGate->load(), values_.reverbMix->load(),
        juce::Decibels::decibelsToGain(values_.output->load()), isOn(values_.limiter),
        juce::Decibels::decibelsToGain(values_.ceiling->load()), values_.highPass->load(),
        values_.lowPass->load(), isOn(values_.highPassSyncEnabled), choice(values_.highPassSyncRate),
        isOn(values_.lowPassSyncEnabled), choice(values_.lowPassSyncRate),
        values_.reverseEnabled->load() > 0.5f, values_.midiVolume->load(), values_.midiExpression->load(), values_.midiBrightness->load(), values_.highPassMidi->load() > 0.5f, values_.lowPassMidi->load() > 0.5f,
        values_.stereoWidth->load(),
        core::ThreeBandEqSettings{
            values_.eqLowFreq->load(), values_.eqLowDb->load(),
            values_.eqMidFreq->load(), values_.eqMidDb->load(),
            values_.eqHighFreq->load(), values_.eqHighDb->load()
        }
    };
    const int signature = choice(values_.timeSignature);
    settings.manualBpm = isOn(values_.clockSync) ? 0 : static_cast<double>(values_.tempoBpm->load());
    settings.manualBeatsPerBar = (!isOn(values_.clockSync) || signature != 0) ? params::beatsPerBarForSignature(signature) : 0;

    return settings;
}

void OpenEdmKickProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto choice = [](const std::atomic<float>* value) { return static_cast<int>(value->load()); };
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

    core::MidiRouting routing{
        this,
        [](void* context, int cc, int, core::EngineSettings& current) noexcept {
            if (cc == 121) {
                auto& processor = *static_cast<OpenEdmKickProcessor*>(context);
                processor.routing_.clearPerformance();
                current = processor.currentEngineSettings();
            }
        },
        [](void* context) noexcept { static_cast<OpenEdmKickProcessor*>(context)->routing_.saveSample(); },
        [](void* context, core::EngineSettings& current) noexcept {
            auto& processor = *static_cast<OpenEdmKickProcessor*>(context);
            processor.routing_.restoreSample();
            current = processor.currentEngineSettings();
        },
        nullptr
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
    state.removeChild(state.getChildWithName("LayerPatterns"), nullptr);
    state.removeChild(state.getChildWithName("ClickSample"), nullptr);
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
    const auto renameId = [&](const char* from, const char* to) {
        if (auto legacy = state.getChildWithProperty("id", from); legacy.isValid()
            && !state.getChildWithProperty("id", to).isValid())
            legacy.setProperty("id", to, nullptr);
    };
    renameId("limiter", params::kLimiterEnabled);
    renameId("output", params::kOutput);
    renameId("ceiling", params::kCeiling);
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
    {
        const std::scoped_lock lock(stateMutex_);
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
        const bool isUserPreset = userPresetName.isNotEmpty();
        const auto locked = isUserPreset ? routing_.lockedValues() : std::vector<ParameterRouting::LockedValue>{};
        if (!recallSound(state.getData(), static_cast<int>(state.getSize()), false, isUserPreset)) return false;
        if (isUserPreset) {
            routing_.restoreLocked(locked);
            for (const auto& entry : locked) routing_.setLocked(entry.entry->parameter->paramID, true);
            setUserPreset(userPresetName);
        }
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

bool OpenEdmKickProcessor::renderCurrentHit(juce::AudioBuffer<float>& destination) const {
    constexpr double rate = hitSampleRate;
    core::KickEngine engine;
    engine.prepare(rate);
    auto settings = currentEngineSettings();
    settings.autoEnabled = false;
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
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openEDMKick-hits");
    if (!directory.createDirectory()) return {};
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::hours(12.0);
    for (const auto& old : directory.findChildFiles(juce::File::findFiles, false, "openEDMKick-*.wav"))
        if (old.getLastModificationTime() < cutoff) old.deleteFile();
    const int note = std::clamp(displayMidiNote.load(), 0, 127);
    const auto noteText = juce::String(core::noteName(note).data()) + juce::String(core::noteOctave(note));
    const auto file = directory.getChildFile("openEDMKick-" + noteText.replaceCharacter('#', 's') + "-"
        + juce::String(juce::Time::currentTimeMillis()) + ".wav");
    if (!suite::writeStereoWav(file, hit, hitSampleRate, 24)) return {};
    return file;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenEdmKickProcessor();
}
