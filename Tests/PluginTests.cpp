#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <ranges>

using Catch::Matchers::WithinAbs;

namespace {

void setParam(OpenEdmKickProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

} // namespace

TEST_CASE("A/B/C/D Auto Mode slots store independent bar counts, fill presets, and tail shorteners", "[plugin]") {
    OpenEdmKickProcessor processor;
    auto* selector = processor.parameters.getParameter("auto_slot");
    REQUIRE(selector != nullptr);
    REQUIRE(selector->isAutomatable());
    REQUIRE(selector->getNumSteps() == 4);

    setParam(processor, "auto_on", 1.0f);
    const std::array<juce::String, 4> prefixes{ "auto_", "auto_b_", "auto_c_", "auto_d_" };
    std::ranges::for_each(std::views::iota(0, 4), [&](int slot) {
        const auto& p = prefixes[static_cast<size_t>(slot)];
        setParam(processor, p + "bars", static_cast<float>(slot + 1));
        setParam(processor, p + "fill", static_cast<float>(slot * 2 + 1));
        setParam(processor, p + "tail_shorten", 0.2f * static_cast<float>(slot + 1));
    });

    std::ranges::for_each(std::array{ 3, 0, 2, 1 }, [&](int slot) {
        setParam(processor, "auto_slot", static_cast<float>(slot));
        const auto s = processor.currentAutoSlotSettings();
        REQUIRE(s.bars == openedmkick::core::kAutoBarCounts[static_cast<size_t>(slot + 1)]);
        REQUIRE(s.fillPreset == slot * 2 + 1);
        REQUIRE_THAT(s.tailShorten, WithinAbs(0.2f * static_cast<float>(slot + 1), 0.001f));
    });

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

    std::ranges::for_each(std::views::iota(0, 4), [&](int slot) {
        setParam(restored, "auto_slot", static_cast<float>(slot));
        REQUIRE_THAT(restored.currentAutoSlotSettings().tailShorten, WithinAbs(0.2f * static_cast<float>(slot + 1), 0.001f));
    });
}

TEST_CASE("Lock In caches WAV in processor state and recalls locked WAV across sessions", "[plugin]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setParam(processor, "note", 7.0f); // G1
    setParam(processor, "lock_in", 1.0f);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    processor.processBlock(buffer, midi);

    REQUIRE(processor.isKickLockedIn());
    const float lockedFirstSample = buffer.getSample(0, 10);

    // Mutating synth parameters while Locked In must not change the cached WAV output
    setParam(processor, "note", 0.0f);
    setParam(processor, "start_pitch", 2200.0f);
    setParam(processor, "tail_length", 50.0f);

    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);

    OpenEdmKickProcessor restored;
    restored.prepareToPlay(48000.0, 512);
    restored.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    REQUIRE(restored.isKickLockedIn());

    juce::AudioBuffer<float> restoredBuf(2, 512);
    restored.processBlock(restoredBuf, midi);
    REQUIRE_THAT(restoredBuf.getSample(0, 10), WithinAbs(lockedFirstSample, 0.001f));
}

TEST_CASE("Editor renders 12-note tuner, kicking mule stage, 1/64 piano roll, and captures native screenshot", "[plugin][ui]") {
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr) SKIP("Native editor requires a display");
#endif
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    processor.setCurrentProgram(12); // Festival 1/64 Drop Rush (8 Bar)
    setParam(processor, "lock_in", 1.0f);
    setParam(processor, "reverse_mode", 2.0f); // Beat 4 Auto

    juce::AudioBuffer<float> warmBuffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    processor.processBlock(warmBuffer, midi);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->getWidth() == 1100);
    REQUIRE(editor->getHeight() == 880);

    setParam(processor, "auto_slot", 2.0f);
    REQUIRE(processor.currentAutoSlot() == 2);

    if (const char* capture = std::getenv("OEK_CAPTURE")) {
        setParam(processor, "auto_slot", 0.0f);
        setParam(processor, "auto_bars", 0.0f); // 1 Bar view so 1/8, 1/16, 1/32, and 1/64 steps are crystal clear
        setParam(processor, "auto_fill", 2.0f); // Festival 1/64 Drop Rush
        setParam(processor, "crush_mix", 0.45f);
        setParam(processor, "stutter_mix", 0.35f);
        juce::AudioBuffer<float> renderBuf(2, 64);
        processor.processBlock(renderBuf, midi);
        processor.displayAutoPhase.store(0.84f);
        processor.displayPeak.store(0.92f);
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream stream{ juce::File(capture) };
        REQUIRE(stream.openedOk());
        REQUIRE(stream.setPosition(0));
        REQUIRE(stream.truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(snapshot, stream));
    }
    if (const char* captureRev = std::getenv("OEK_CAPTURE_REVERSE")) {
        processor.setCurrentProgram(4); // Beat 4 Reverse Pickup (E1)
        setParam(processor, "auto_on", 1.0f);
        setParam(processor, "auto_slot", 1.0f);
        setParam(processor, "auto_b_bars", 1.0f); // 2 Bars
        setParam(processor, "auto_b_fill", 8.0f); // Beat 4 Reverse Pickup Loop
        setParam(processor, "reverse_mode", 2.0f);
        setParam(processor, "crush_mix", 0.38f);
        setParam(processor, "gate_mix", 0.42f);
        juce::AudioBuffer<float> renderBuf(2, 64);
        processor.processBlock(renderBuf, midi);
        processor.displayAutoPhase.store(0.42f);
        processor.displayPeak.store(0.88f);
        processor.displayReversing.store(true);
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        juce::FileOutputStream stream{ juce::File(captureRev) };
        REQUIRE(stream.openedOk());
        REQUIRE(stream.setPosition(0));
        REQUIRE(stream.truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(snapshot, stream));
    }
}
