#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <numeric>
#include <new>
#include <ranges>
#include <ranges>
#include <string>
#include <vector>

namespace {
thread_local size_t* callbackAllocations = nullptr;
}

void* operator new(std::size_t bytes) {
    if (callbackAllocations != nullptr) ++*callbackAllocations;
    if (auto* memory = std::malloc(std::max<size_t>(bytes, 1))) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }

namespace {
class TestPlayHead final : public juce::AudioPlayHead {
public:
    bool playing = false;

    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setBpm(128.0);
        info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
        info.setIsPlaying(playing);
        info.setPpqPosition(0.0);
        return info;
    }
};

void setParam(OpenEdmKickProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
} // namespace

using Catch::Matchers::WithinAbs;

TEST_CASE("A/B/C/D Auto Mode slots store independent bar counts, fills, and tail trims", "[plugin]") {
    OpenEdmKickProcessor processor;
    auto* selector = processor.parameters.getParameter("auto_slot");
    REQUIRE(selector != nullptr);
    REQUIRE(selector->isAutomatable());
    REQUIRE(selector->getNumSteps() == 4);

    const std::array<juce::String, 4> prefixes{ "auto_", "auto_b_", "auto_c_", "auto_d_" };
    std::ranges::for_each(std::views::iota(0, 4), [&](int slot) {
        const auto& prefix = prefixes[static_cast<size_t>(slot)];
        setParam(processor, prefix + "bars", static_cast<float>(slot + 1));
        setParam(processor, prefix + "fill", static_cast<float>(slot * 2 + 1));
        setParam(processor, prefix + "tail_shorten", 0.2f * static_cast<float>(slot + 1));
    });

    std::ranges::for_each(std::array{ 3, 0, 2, 1 }, [&](int slot) {
        setParam(processor, "auto_slot", static_cast<float>(slot));
        const auto settings = processor.currentAutoSlotSettings();
        REQUIRE(settings.bars == openedmkick::core::kAutoBarCounts[static_cast<size_t>(slot + 1)]);
        REQUIRE(settings.fillPreset == slot * 2 + 1);
        REQUIRE_THAT(settings.tailShorten, WithinAbs(0.2f * static_cast<float>(slot + 1), 0.001f));
    });

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (int slot = 0; slot < 4; ++slot) {
        setParam(restored, "auto_slot", static_cast<float>(slot));
        REQUIRE_THAT(restored.currentAutoSlotSettings().tailShorten,
                     WithinAbs(0.2f * static_cast<float>(slot + 1), 0.001f));
    }
}

TEST_CASE("Host stop silences Auto Mode and host play resumes it", "[plugin]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setParam(processor, "auto_on", 1.0f);
    setParam(processor, "clock_sync", 1.0f);
    setParam(processor, "auto_bars", 0.0f);
    setParam(processor, "auto_fill", 0.0f);
    TestPlayHead playHead;
    processor.setPlayHead(&playHead);

    juce::AudioBuffer<float> buffer(2, 1024);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    float stoppedEnergy = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        stoppedEnergy += std::abs(buffer.getSample(0, sample));
    REQUIRE_THAT(stoppedEnergy, WithinAbs(0.0f, 1.0e-5f));

    playHead.playing = true;
    processor.processBlock(buffer, midi);
    float playingEnergy = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        playingEnergy += std::abs(buffer.getSample(0, sample));
    REQUIRE(playingEnergy > 5.0f);
    processor.setPlayHead(nullptr);
}

TEST_CASE("Editor shows the new sound pages and current MIDI note", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
    processor.processBlock(buffer, midi);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() == 1100);
    REQUIRE(editor->getHeight() == 940);
    auto* kickEditor = dynamic_cast<OpenEdmKickEditor*>(editor.get());
    REQUIRE(kickEditor != nullptr);
    REQUIRE(processor.currentMidiNote() == 69);
    REQUIRE_THAT(processor.displayMidiHz.load(), WithinAbs(440.0f, 0.001f));
    REQUIRE_FALSE(kickEditor->isAdvancedView());
    kickEditor->setAdvancedView(true);
    REQUIRE(kickEditor->isAdvancedView());
    kickEditor->setAdvancedView(false);
    REQUIRE_FALSE(kickEditor->isAdvancedView());
}

TEST_CASE("MIDI input tracks pitch and octave, including notes from other channels", "[plugin][midi]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setParam(processor, "drive", 0.0f);
    setParam(processor, "limiter", 0.0f);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(0)), 0);
    midi.addEvent(juce::MidiMessage::noteOff(3, 60), 20);
    processor.processBlock(buffer, midi);
    float silent = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        silent += std::abs(buffer.getSample(0, sample));
    REQUIRE_THAT(silent, WithinAbs(0.0f, 1.0e-5f));

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(5, 69, 0.9f), 16);
    processor.processBlock(buffer, midi);
    float heard = 0.0f;
    for (int sample = 16; sample < buffer.getNumSamples(); ++sample)
        heard += std::abs(buffer.getSample(0, sample));
    REQUIRE(heard > 1.0f);
    REQUIRE(processor.currentMidiNote() == 69);
    REQUIRE_THAT(processor.displayMidiHz.load(), WithinAbs(440.0f, 0.001f));

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(5, 57, 0.9f), 0);
    processor.processBlock(buffer, midi);
    REQUIRE(processor.currentMidiNote() == 57);
    REQUIRE_THAT(processor.displayMidiHz.load(), WithinAbs(220.0f, 0.001f));
}

TEST_CASE("Layer and host-sync FX controls are automatable and saved with the session", "[plugin][parameters]") {
    OpenEdmKickProcessor processor;
    const std::array<juce::String, 11> ids{
        "layer_a_enabled", "layer_a_sound", "layer_a_gain", "layer_a_transpose", "layer_a_decay",
        "layer_a_hp", "layer_a_lp", "hp_sync_on", "hp_sync_rate", "lp_sync_on", "lp_sync_rate"
    };
    std::ranges::for_each(ids, [&](const juce::String& id) {
        auto* parameter = processor.parameters.getParameter(id);
        REQUIRE(parameter != nullptr);
        REQUIRE(parameter->isAutomatable());
    });
    REQUIRE(processor.currentLayerSettings()[0].enabled);
    REQUIRE_FALSE(processor.currentLayerSettings()[1].enabled);
    REQUIRE(processor.parameters.getParameter("layer_b_sound")->getNumSteps() == 4);

    setParam(processor, "layer_d_enabled", 1.0f);
    setParam(processor, "layer_d_sound", 3.0f);
    setParam(processor, "layer_d_hp", 1800.0f);
    setParam(processor, "hp_sync_on", 1.0f);
    setParam(processor, "lp_sync_rate", 4.0f);
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);

    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    const auto layer = restored.currentLayerSettings()[3];
    REQUIRE(layer.enabled);
    REQUIRE(layer.sound == openedmkick::core::DrumSoundType::hat);
    REQUIRE_THAT(layer.highPassHz, WithinAbs(1800.0f, 1.0f));
    REQUIRE_THAT(restored.parameters.getRawParameterValue("hp_sync_on")->load(), WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(restored.parameters.getRawParameterValue("lp_sync_rate")->load(), WithinAbs(4.0f, 0.001f));
}

TEST_CASE("MIDI CC21 activates the host-synced high-pass in the plug-in", "[plugin][midi][effects]") {
    const auto render = [](bool sendCc) {
        OpenEdmKickProcessor processor;
        processor.prepareToPlay(48000.0, 8192);
        setParam(processor, "limiter", 0.0f);
        setParam(processor, "drive", 0.0f);
        juce::AudioBuffer<float> buffer(2, 8192);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
        if (sendCc) midi.addEvent(juce::MidiMessage::controllerEvent(1, 21, 127), 0);
        processor.processBlock(buffer, midi);
        std::vector<float> result(static_cast<size_t>(buffer.getNumSamples()));
        std::ranges::for_each(std::views::iota(0, buffer.getNumSamples()), [&](int sample) {
            result[static_cast<size_t>(sample)] = buffer.getSample(0, sample);
        });
        return result;
    };
    const auto dry = render(false);
    const auto synced = render(true);
    const double difference = std::inner_product(dry.begin(), dry.end(), synced.begin(), 0.0,
        std::plus<>(), [](float left, float right) { return std::abs(static_cast<double>(left - right)); });
    REQUIRE(difference > 10.0);
}

TEST_CASE("V3 factory bank contains the new kick and fill catalogs", "[plugin][presets]") {
    const auto& programs = openedmkick::presets::factoryPresets();
    const auto& fills = openedmkick::presets::fillBuildPresets();
    REQUIRE(programs.size() == 48u);
    REQUIRE(fills.size() == 28u);
    REQUIRE(std::string(programs.front().name) == "Plasma Breaker");
    REQUIRE(std::string(programs.back().name) == "Eventide Engine");
    REQUIRE(std::string(fills.front().name) == "Ion Drive Four");
    REQUIRE(std::string(fills.back().name) == "Final Orbit Reverse");

    OpenEdmKickProcessor processor;
    REQUIRE(processor.getNumPrograms() == static_cast<int>(programs.size()));
    for (int index = 0; index < processor.getNumPrograms(); ++index) {
        processor.setCurrentProgram(index);
        REQUIRE(processor.getProgramName(index) == programs[static_cast<size_t>(index)].name);
    }

    processor.setCurrentProgram(0);
    const auto first = processor.currentSynthSettings();
    REQUIRE_THAT(first.bodyTone, WithinAbs(0.78f, 0.01f));
    REQUIRE(first.clickLevel > 0.8f);

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(restored.getCurrentProgram() == 0);
    REQUIRE_FALSE(restored.hasRestoredCustomState());
}

TEST_CASE("All 48 trance kick presets load and report valid program names", "[plugin][presets]") {
    OpenEdmKickProcessor processor;
    const auto& presets = openedmkick::presets::factoryPresets();
    REQUIRE(processor.getNumPrograms() == 48);

    std::vector<std::string> names;
    for (int index = 0; index < processor.getNumPrograms(); ++index) {
        const auto name = processor.getProgramName(index);
        REQUIRE(name.isNotEmpty());
        processor.setCurrentProgram(index);
        REQUIRE(processor.getCurrentProgram() == index);
        names.push_back(name.toStdString());
    }
    std::ranges::sort(names);
    REQUIRE(std::ranges::adjacent_find(names) == names.end());
    REQUIRE(presets.size() == 48u);
}

TEST_CASE("New layer controls automate and restore independently", "[layer-improvements]") {
    OpenEdmKickProcessor processor;
    setParam(processor, "layer_a_velocity_curve", 0.5f);
    REQUIRE(processor.parameters.getParameter("layer_a_velocity_curve")->getVersionHint() == 3);
    setParam(processor, "layer_a_velocity_timbre", 0.2f);
    setParam(processor, "layer_a_key_low", 36);
    setParam(processor, "layer_a_key_high", 48);
    setParam(processor, "layer_a_velocity_low", 32);
    setParam(processor, "layer_a_velocity_high", 96);
    setParam(processor, "layer_a_drive_offset", 0.3f);
    setParam(processor, "layer_a_tone_scale", 0.4f);
    setParam(processor, "layer_a_sweep_scale", 0.6f);
    setParam(processor, "layer_a_solo", 1);
    setParam(processor, "layer_a_pan", -0.7f);
    setParam(processor, "layer_a_polarity", 1);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto layers = restored.currentLayerSettings();
    REQUIRE_THAT(layers[0].velocityCurve, WithinAbs(0.5f, 0.001));
    REQUIRE_THAT(layers[0].velocityTimbre, WithinAbs(0.2f, 0.001));
    REQUIRE(layers[0].keyLow == 36);
    REQUIRE(layers[0].keyHigh == 48);
    REQUIRE(layers[0].velocityLow == 32);
    REQUIRE(layers[0].velocityHigh == 96);
    REQUIRE_THAT(layers[0].driveOffset, WithinAbs(0.3f, 0.001));
    REQUIRE_THAT(layers[0].toneScale, WithinAbs(0.4f, 0.001));
    REQUIRE_THAT(layers[0].sweepScale, WithinAbs(0.6f, 0.001));
    REQUIRE(layers[0].solo);
    REQUIRE(layers[0].invertPolarity);
    REQUIRE_THAT(layers[0].pan, WithinAbs(-0.7f, 0.001));
    REQUIRE(layers[1].keyLow == 0);
    REQUIRE_THAT(layers[1].pan, WithinAbs(0.0f, 0.00001f));
}

TEST_CASE("Layer lane notes and ratchets survive session recall", "[layer-improvements]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 512);
    processor.togglePianoRollNote(7, 4, 2, 3);
    const auto before = processor.currentPianoRollNotes();
    const auto found = std::ranges::find_if(before, [](const auto& note) { return note.step64 == 7 && note.layer == 2; });
    REQUIRE(found != before.end());
    REQUIRE(found->ratchets == 3);
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    const auto sameNote = [](const auto& left, const auto& right) {
        return left.step64 == right.step64 && left.length64 == right.length64
            && std::abs(left.velocity - right.velocity) < 0.00001f && left.reverseCue == right.reverseCue
            && left.layer == right.layer && left.ratchets == right.ratchets;
    };
    REQUIRE(std::ranges::equal(restored.currentPianoRollNotes(), before, sameNote));
    restored.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    restored.processBlock(audio, midi);
    REQUIRE(std::ranges::equal(restored.currentPianoRollNotes(), before, sameNote));
}

TEST_CASE("Older state fills additive layer controls with neutral defaults", "[layer-improvements]") {
    OpenEdmKickProcessor processor;
    setParam(processor, "layer_a_pan", 1);
    auto state = processor.parameters.copyState();
    const auto isNewControl = [](const juce::String& id) {
        return id.startsWith("layer_") && (id.endsWith("velocity_curve") || id.endsWith("velocity_timbre")
            || id.endsWith("key_low") || id.endsWith("key_high") || id.endsWith("velocity_low")
            || id.endsWith("velocity_high") || id.endsWith("drive_offset") || id.endsWith("tone_scale")
            || id.endsWith("sweep_scale") || id.endsWith("solo") || id.endsWith("pan") || id.endsWith("polarity"));
    };
    for (int index = state.getNumChildren() - 1; index >= 0; --index)
        if (isNewControl(state.getChild(index).getProperty("id").toString())) state.removeChild(index, nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(*state.createXml(), saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    const auto layer = processor.currentLayerSettings()[0];
    REQUIRE_THAT(layer.velocityCurve, WithinAbs(1.45f, 0.001f));
    REQUIRE_THAT(layer.velocityTimbre, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(layer.pan, WithinAbs(0.0f, 0.001f));
    REQUIRE(layer.keyLow == 0);
    REQUIRE(layer.keyHigh == 127);
    REQUIRE_FALSE(layer.solo);
    REQUIRE_FALSE(layer.invertPolarity);
}

TEST_CASE("Publishing layer lanes and processing dense MIDI allocates no callback storage", "[layer-improvements][realtime]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 32);
    processor.togglePianoRollNote(4, 4, 3, 8);
    setParam(processor, "auto_bars", 6);
    setParam(processor, "auto_fill", 20);
    juce::AudioBuffer<float> buffer(2, 32);
    juce::MidiBuffer midi;
    for (int event = 0; event < 70000; ++event)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, event % 128), 0);
    size_t allocations = 0;
    callbackAllocations = &allocations;
    const auto started = std::chrono::steady_clock::now();
    processor.processBlock(buffer, midi);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    callbackAllocations = nullptr;
    CAPTURE(elapsed);
    REQUIRE(elapsed < 250.0);
    REQUIRE(allocations == 0);
    REQUIRE(std::isfinite(buffer.getSample(0, 31)));
}

TEST_CASE("Layer editor lays out additive controls on each layer page", "[layer-improvements][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::LayerPage page(processor, 0);
    page.setSize(1000, 600);
    const auto find = [](auto&& self, juce::Component& parent, const juce::String& id) -> juce::Component* {
        if (parent.getComponentID() == id) return &parent;
        for (auto* child : parent.getChildren())
            if (auto* found = self(self, *child, id)) return found;
        return nullptr;
    };
    auto* velocity = find(find, page, "layer_a_velocity_curve");
    auto* zone = find(find, page, "layer_a_key_low");
    auto* solo = find(find, page, "layer_a_solo");
    auto* pan = find(find, page, "layer_a_pan");
    REQUIRE(velocity != nullptr);
    REQUIRE(zone != nullptr);
    REQUIRE(solo != nullptr);
    REQUIRE(pan != nullptr);
    REQUIRE(velocity->getWidth() > 60);
    REQUIRE(zone->getHeight() > 40);
    REQUIRE(solo->getHeight() > 20);
    REQUIRE(pan->getWidth() > 60);
}

TEST_CASE("Audition remains at sample zero before later MIDI controller events", "[layer-improvements][midi]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 256);
    setParam(processor, "limiter", 0);
    processor.triggerAudition();
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 0), 128);
    processor.processBlock(buffer, midi);
    REQUIRE(processor.displayTriggerCount.load() == 1);
    REQUIRE(buffer.getMagnitude(0, 128) > 0.01f);
    REQUIRE_THAT(buffer.getMagnitude(128, 128), WithinAbs(0.0f, 0.00001f));
}
