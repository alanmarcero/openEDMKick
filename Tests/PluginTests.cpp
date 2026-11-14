#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <ranges>
#include <ranges>
#include <string>
#include <vector>

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
