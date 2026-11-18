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
#include <numbers>
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
    double bpm = 128.0;
    double ppq = 0.0;

    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setBpm(bpm);
        info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
        info.setIsPlaying(playing);
        info.setPpqPosition(ppq);
        return info;
    }
};

void setParam(OpenEdmKickProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void checkInteractiveTargets(juce::Component& component) {
    for (auto* child : component.getChildren()) {
        if (!child->isVisible()) continue;
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            INFO(button->getButtonText());
            CHECK(button->getHeight() >= 40);
            CHECK(button->getWidth() >= 40);
            const juce::Font face(juce::FontOptions(15.0f, juce::Font::bold));
            CHECK(juce::GlyphArrangement::getStringWidthInt(face, button->getButtonText()) <= button->getWidth() - 8);
            CHECK(button->getLookAndFeel().getTextButtonFont(*button, button->getHeight()).getHeight() >= 14.0f);
            CHECK(button->getButtonText() != "<");
            CHECK(button->getButtonText() != ">");
        }
        if (dynamic_cast<openedmkick::ui::ParameterControl*>(child) != nullptr) {
            CHECK(child->getWidth() >= 40);
            CHECK(child->getHeight() >= 40);
        }
        checkInteractiveTargets(*child);
    }
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

TEST_CASE("The SVG pulse follows host seeks, tempo and stop, and runs on the standalone clock", "[plugin][clock]") {
    juce::ScopedJuceInitialiser_GUI gui;
    TestPlayHead host;
    host.playing = true;
    host.bpm = 120;
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 240);
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> buffer(2, 240);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    CHECK_THAT(processor.displayBeatPhase.load(), WithinAbs(239.0 / 24000, 0.0001));
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    const juce::Rectangle<int> badge(12, 6, 58, 60);
    const auto downbeat = editor->createComponentSnapshot(badge);
    host.ppq = 8.5;
    host.bpm = 60;
    processor.processBlock(buffer, midi);
    const auto offbeat = editor->createComponentSnapshot(badge);
    int changedPixels = 0;
    for (int x = 0; x < badge.getWidth(); ++x)
        for (int y = 0; y < badge.getHeight(); ++y)
            if (downbeat.getPixelAt(x, y) != offbeat.getPixelAt(x, y)) ++changedPixels;
    CHECK(changedPixels > 100);
    CHECK_THAT(processor.displayBeatPhase.load(), WithinAbs(0.5 + 239.0 / 48000, 0.0001));
    host.ppq = -0.75;
    processor.processBlock(buffer, midi);
    const float phase = processor.displayBeatPhase.load();
    CHECK_THAT(phase, WithinAbs(0.25 + 239.0 / 48000, 0.0001));
    host.playing = false;
    processor.processBlock(buffer, midi);
    CHECK(processor.displayBeatPhase.load() == phase);
    processor.setPlayHead(nullptr);
    processor.processBlock(buffer, midi);
    CHECK(processor.displayBeatPhase.load() != phase);
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
    REQUIRE(editor->getWidth() == 1080);
    REQUIRE(editor->getHeight() == 716);
    auto* kickEditor = dynamic_cast<OpenEdmKickEditor*>(editor.get());
    REQUIRE(kickEditor != nullptr);
    REQUIRE(processor.currentMidiNote() == 69);
    auto* audition = dynamic_cast<juce::TextButton*>(editor->findChildWithID("audition"));
    REQUIRE(audition != nullptr);
    const auto triggersBefore = processor.displayTriggerCount.load();
    audition->onClick();
    midi.clear();
    processor.processBlock(buffer, midi);
    REQUIRE(processor.displayTriggerCount.load() == triggersBefore + 1);
    REQUIRE(buffer.getMagnitude(0, 512) > 0.1f);
    REQUIRE_FALSE(kickEditor->isAdvancedView());
    auto* presets = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("factoryPreset"));
    REQUIRE(presets != nullptr);
    CHECK(presets->getRootMenu()->getNumItems() == 6);
    presets->setSelectedId(48, juce::sendNotificationSync);
    REQUIRE(processor.getCurrentProgram() == 47);
    auto* next = dynamic_cast<juce::TextButton*>(editor->findChildWithID("nextPreset"));
    REQUIRE(next != nullptr);
    next->onClick();
    CHECK(processor.getCurrentProgram() == 0);
    CHECK(presets->getSelectedId() == 1);
    kickEditor->setAdvancedView(true);
    REQUIRE(kickEditor->isAdvancedView());
    kickEditor->setAdvancedView(false);
    REQUIRE_FALSE(kickEditor->isAdvancedView());
    kickEditor->setSize(900, 570);
    kickEditor->setAdvancedView(true);
    for (const auto* name : {"kick", "layers", "pattern", "fx"}) {
        auto* tab = dynamic_cast<juce::TextButton*>(kickEditor->findChildWithID(juce::String("page_") + name));
        REQUIRE(tab != nullptr);
        tab->onClick();
        checkInteractiveTargets(*kickEditor);
    }
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
    REQUIRE(std::string(programs.front().name) == "Uplifting / First Light");
    REQUIRE(std::string(programs.back().name) == "Classic / Final Press");
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
    REQUIRE_THAT(first.bodyTone, WithinAbs(0.49f, 0.01f));
    REQUIRE(first.clickLevel <= 0.55f);

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(restored.getCurrentProgram() == 0);
    REQUIRE_FALSE(restored.hasRestoredCustomState());
}

TEST_CASE("Hold MIDI notes swaps the one-shot tail for an editable envelope on the Kick page", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::KickSynthPanel panel(processor);
    panel.setSize(860, 560);
    juce::TextButton* gate{};
    juce::Slider* sustain{};
    juce::Slider* hold{};
    for (auto* control : panel.getChildren()) {
        if (auto* input = dynamic_cast<juce::TextButton*>(control->findChildWithID("amp_enabled"))) gate = input;
        if (auto* input = dynamic_cast<juce::Slider*>(control->findChildWithID("amp_sustain"))) sustain = input;
        if (auto* input = dynamic_cast<juce::Slider*>(control->findChildWithID("hold_ms"))) hold = input;
    }
    REQUIRE(gate != nullptr);
    REQUIRE(sustain != nullptr);
    REQUIRE(hold != nullptr);
    for (bool gated : {true, false}) {
        gate->setToggleState(gated, juce::sendNotificationSync);
        gate->getToggleStateValue().getValueSource().sendChangeMessage(true);
        CHECK(processor.currentSynthSettings().ampEnabled == gated);
        CHECK(sustain->getParentComponent()->isVisible() == gated);
        CHECK(hold->getParentComponent()->isVisible() == !gated);
    }
}

TEST_CASE("A previous bank restores its saved sound as custom", "[plugin][presets][state]") {
    OpenEdmKickProcessor processor;
    auto legacy = processor.parameters.copyState();
    legacy.setProperty("presetCatalogVersion", 3, nullptr);
    legacy.setProperty("program", 12, nullptr);
    legacy.getChildWithProperty("id", "decay_ms").setProperty("value", 347.0f, nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(processor.hasRestoredCustomState());
    REQUIRE_THAT(processor.currentSynthSettings().decayMs, WithinAbs(347.0f, 0.01f));
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

TEST_CASE("Factory trance one-shots have a defined attack, tuned sub and short tail", "[plugin][presets][audio]") {
    constexpr int rate = 48000;
    OpenEdmKickProcessor processor;
    juce::AudioBuffer<float> hit(2, rate / 4);
    processor.prepareToPlay(rate, hit.getNumSamples());
    for (int index = 0; index < processor.getNumPrograms(); ++index) {
        processor.setCurrentProgram(index);
        const auto settings = processor.currentSynthSettings();
        INFO(processor.getProgramName(index).toStdString());
        CHECK(settings.saturation == openedmkick::core::Saturation::warm);
        CHECK(settings.drive <= 0.16f);
        CHECK(settings.clip <= 0.05f);
        CHECK_FALSE(settings.ampEnabled);
        for (int note : {29, 33, 36}) {
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, note, 1.0f), 0);
            processor.processBlock(hit, midi);
            CAPTURE(note);
            const float peak = hit.getMagnitude(0, hit.getNumSamples());
            CHECK(peak <= 1.0f);
            CHECK(hit.getMagnitude(0, rate / 50) > 0.25f);
            CHECK(hit.getMagnitude(rate / 5, rate / 20) < 0.001f);

            // Measure two cycles after the pitch dive, projecting onto the played fundamental.
            const double hz = openedmkick::core::noteFrequencyHz(note);
            const int count = juce::roundToInt(2.0 * rate / hz);
            const int start = juce::roundToInt(settings.sweepMs * rate / 1000.0);
            double real = 0, imag = 0, energy = 0;
            for (int i = 0; i < count; ++i) {
                const double sample = hit.getSample(0, start + i);
                const double phase = 2 * std::numbers::pi * hz * i / rate;
                real += sample * std::cos(phase);
                imag += sample * std::sin(phase);
                energy += sample * sample;
            }
            const double fundamentalPower = 2 * (real * real + imag * imag) / (count * count);
            CHECK(std::sqrt(fundamentalPower) > 0.10);
            CHECK(fundamentalPower > 0.60 * energy / count);
        }
    }
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

TEST_CASE("Parameter and section locks preserve the latest edit through presets", "[routing]") {
    OpenEdmKickProcessor processor;
    processor.routing().setLocked("body_tone", true);
    setParam(processor, "body_tone", 0.123f);
    processor.setCurrentProgram(1);
    REQUIRE_THAT(processor.currentSynthSettings().bodyTone, WithinAbs(0.123f, 0.0001f));
    setParam(processor, "body_tone", 0.321f);
    processor.setCurrentProgram(2);
    REQUIRE_THAT(processor.currentSynthSettings().bodyTone, WithinAbs(0.321f, 0.0001f));
    processor.routing().toggleSection({"body_tone", "body_drive"});
    REQUIRE(processor.routing().isLocked("body_tone"));
    REQUIRE(processor.routing().isLocked("body_drive"));
    REQUIRE_FALSE(processor.routing().isLocked("clip"));
    processor.routing().toggleSection({"body_tone", "body_drive"});
    REQUIRE_FALSE(processor.routing().isLocked("body_tone"));
    REQUIRE_FALSE(processor.routing().isLocked("body_drive"));
}

TEST_CASE("CC menus expose all controllers and retain factory assignments", "[routing]") {
    OpenEdmKickProcessor processor;
    REQUIRE(processor.routing().assignment("reverse_on") == 20);
    REQUIRE(processor.routing().assignment("hp_midi_on") == 21);
    REQUIRE(processor.routing().assignment("lp_midi_on") == 22);
    REQUIRE(processor.routing().assignment("midi_volume") == 7);
    REQUIRE(processor.routing().assignment("midi_expression") == 11);
    REQUIRE(processor.routing().assignment("midi_brightness") == 74);
    REQUIRE(processor.routing().assignment("body_tone") == -1);
    const auto menu = processor.routing().midiMenu("body_tone");
    REQUIRE(menu.getNumItems() == 130);
    std::array<bool, 128> entries{};
    for (juce::PopupMenu::MenuItemIterator iterator(menu); iterator.next();) {
        const int cc = iterator.getItem().itemID - 2;
        if (cc >= 0 && cc < 128) entries[static_cast<size_t>(cc)] = true;
    }
    REQUIRE(std::all_of(entries.begin(), entries.end(), [](bool present) { return present; }));
}

TEST_CASE("Routing metadata survives state restore without overriding project values", "[routing]") {
    OpenEdmKickProcessor source;
    setParam(source, "body_tone", 0.234f);
    source.routing().setLocked("body_tone", true);
    source.routing().assignCc("clip", 35);
    juce::MemoryBlock saved;
    source.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.routing().setLocked("body_tone", true);
    setParam(restored, "body_tone", 0.9f);
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(restored.currentSynthSettings().bodyTone, WithinAbs(0.234f, 0.0001f));
    REQUIRE(restored.routing().isLocked("body_tone"));
    REQUIRE(restored.routing().assignment("clip") == 35);
}

TEST_CASE("Assigned CC endpoints and discrete values reach runtime without APVTS writes", "[routing]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 64);
    processor.routing().assignCc("clip", 35);
    processor.routing().assignCc("click_type", 35);
    const float hostClip = processor.parameters.getRawParameterValue("clip")->load();
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 127), 16);
    processor.processBlock(audio, midi);
    REQUIRE_THAT(processor.routing().displayValue("clip"), WithinAbs(1.0f, 0.0001f));
    REQUIRE_THAT(processor.routing().displayValue("click_type"), WithinAbs(3.0f, 0.0001f));
    REQUIRE_THAT(processor.parameters.getRawParameterValue("clip")->load(), WithinAbs(hostClip, 0.0001f));
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 0), 0);
    processor.processBlock(audio, midi);
    REQUIRE_THAT(processor.routing().displayValue("clip"), WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(processor.routing().displayValue("click_type"), WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("Parameter controls show MIDI mirrors without editing host automation", "[routing][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::Knob knob(processor.parameters, "body_tone", "Harmonics", juce::Colours::orange);
    knob.setSize(160, 150);
    REQUIRE(knob.findChildWithID("routing_body_tone") != nullptr);
    processor.routing().assignCc("body_tone", 35);
    const float saved = processor.parameters.getRawParameterValue("body_tone")->load();
    processor.routing().applyCc(35, 127);
    knob.refreshMidiMirror();
    REQUIRE_THAT(static_cast<float>(knob.slider().getValue()), WithinAbs(1.0f, 0.0001f));
    REQUIRE_THAT(processor.parameters.getRawParameterValue("body_tone")->load(), WithinAbs(saved, 0.0001f));
}

TEST_CASE("Processor ADSR note-offs retain their offsets and host tails finish", "[followup][adsr][plugin]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(8000, 128);
    setParam(processor, "amp_enabled", 1);
    setParam(processor, "amp_attack_ms", 0);
    setParam(processor, "amp_decay_ms", 0);
    setParam(processor, "amp_sustain", 1);
    setParam(processor, "amp_release_ms", 100);
    setParam(processor, "limiter", 0);
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(2, 36, 1.0f), 8);
    processor.processBlock(audio, midi);
    REQUIRE(processor.getTailLengthSeconds() >= 0.099);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 36), 16);
    processor.processBlock(audio, midi);
    REQUIRE(processor.getTailLengthSeconds() >= 0.099);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(2, 36), 32);
    processor.processBlock(audio, midi);
    REQUIRE(processor.getTailLengthSeconds() < 0.1);
    midi.clear();
    for (int block = 0; block < 7; ++block) processor.processBlock(audio, midi);
    REQUIRE_THAT(static_cast<float>(processor.getTailLengthSeconds()), WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("ADSR disabled retains one-shot note-off behavior", "[followup][adsr][plugin]") {
    const auto render = [](bool sendOff) {
        OpenEdmKickProcessor processor;
        processor.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> audio(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 16);
        if (sendOff) midi.addEvent(juce::MidiMessage::noteOff(1, 36), 32);
        processor.processBlock(audio, midi);
        return std::vector<float>(audio.getReadPointer(0), audio.getReadPointer(0) + 512);
    };
    REQUIRE(render(true) == render(false));
}

TEST_CASE("Factory filter CC off preserves a host-enabled sync switch", "[routing][midi]") {
    const auto render = [](bool ccOff) {
        OpenEdmKickProcessor processor;
        processor.prepareToPlay(48000, 1024);
        setParam(processor, "hp_sync_on", 1);
        setParam(processor, "limiter", 0);
        juce::AudioBuffer<float> audio(2, 1024);
        juce::MidiBuffer midi;
        if (ccOff) midi.addEvent(juce::MidiMessage::controllerEvent(1, 21, 0), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        processor.processBlock(audio, midi);
        return std::vector<float>(audio.getReadPointer(0), audio.getReadPointer(0) + 1024);
    };
    const auto withOff = render(true);
    const auto withoutOff = render(false);
    const double difference = std::inner_product(withOff.begin(), withOff.end(), withoutOff.begin(), 0.0,
        std::plus<>(), [](float a, float b) { return std::abs(a - b); });
    REQUIRE(difference < 0.0001);
}

TEST_CASE("Native section locks cover their own parameter groups", "[routing][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::KickSynthPanel panel(processor);
    auto* dive = dynamic_cast<openedmkick::ui::SectionLock*>(panel.findChildWithID("section_lock_dive"));
    REQUIRE(dive != nullptr);
    REQUIRE(dive->parameterIds() == juce::StringArray{"sweep_octaves", "sweep_ms", "sweep_shape", "snap_octaves", "snap_ms"});
    dive->onClick();
    REQUIRE(processor.routing().isLocked("sweep_ms"));
    REQUIRE_FALSE(processor.routing().isLocked("body_tone"));
    dive->onClick();
    REQUIRE_FALSE(processor.routing().isLocked("sweep_ms"));
}

TEST_CASE("Routing restores legacy defaults and maps boolean/shared controllers", "[routing]") {
    OpenEdmKickProcessor processor;
    processor.routing().assignCc("amp_enabled", 35);
    processor.routing().assignCc("click_type", 35);
    processor.routing().applyCc(35, 127);
    REQUIRE_THAT(processor.routing().displayValue("amp_enabled"), WithinAbs(1.0f, 0.0001f));
    processor.routing().applyCc(35, 0);
    REQUIRE_THAT(processor.routing().displayValue("amp_enabled"), WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(processor.routing().displayValue("click_type"), WithinAbs(0.0f, 0.0001f));
    processor.routing().setLocked("clip", true);
    processor.routing().assignCc("reverse_on", 90);
    auto state = processor.parameters.copyState();
    state.removeChild(state.getChildWithName("ParameterRouting"), nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(*state.createXml(), saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_FALSE(processor.routing().isLocked("clip"));
    REQUIRE(processor.routing().assignment("reverse_on") == 20);
    REQUIRE(processor.routing().assignment("amp_enabled") == -1);
}

TEST_CASE("Mapped CC order controls the following note rather than the preceding one", "[routing][midi]") {
    const auto render = [](bool ccFirst) {
        OpenEdmKickProcessor processor;
        processor.prepareToPlay(48000, 512);
        processor.routing().assignCc("layer_a_transpose", 35);
        juce::AudioBuffer<float> audio(2, 512);
        juce::MidiBuffer midi;
        if (ccFirst) midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 127), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        if (!ccFirst) midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 127), 0);
        processor.processBlock(audio, midi);
        return std::vector<float>(audio.getReadPointer(0), audio.getReadPointer(0) + 512);
    };
    const auto before = render(true);
    const auto after = render(false);
    const double difference = std::inner_product(before.begin(), before.end(), after.begin(), 0.0,
        std::plus<>(), [](float a, float b) { return std::abs(a - b); });
    REQUIRE(difference > 1);
}

TEST_CASE("Capture the compiled native editor workspaces", "[capture]") {
    const auto directory = juce::SystemStats::getEnvironmentVariable("OEK_CAPTURE_DIR", {});
    if (directory.isEmpty()) SKIP("Set OEK_CAPTURE_DIR or build the openedmkick_screenshots target");
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    processor.routing().setLocked("sweep_ms", true);
    processor.routing().assignCc("body_tone", 35);
    processor.togglePianoRollNote(8, 8, 0, 2);
    processor.togglePianoRollNote(24, 8, 1, 3);
    processor.togglePianoRollNote(40, 8, 2, 4);
    processor.togglePianoRollNote(56, 8, 3, 5);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* native = dynamic_cast<OpenEdmKickEditor*>(editor.get());
    REQUIRE(native != nullptr);
    CHECK(native->getWidth() == 1080);
    CHECK(native->getHeight() == 716);
    const juce::File destination(directory);
    REQUIRE(destination.createDirectory().wasOk());
    const std::array<juce::String, 5> pages{"kick", "held", "layers", "pattern", "fx"};
    for (const auto& page : pages) {
        const bool held = page == "held";
        setParam(processor, "amp_enabled", held ? 1.0f : 0.0f);
        auto* tab = dynamic_cast<juce::TextButton*>(native->findChildWithID("page_" + (held ? "kick" : page)));
        REQUIRE(tab != nullptr);
        tab->onClick();
        native->setAdvancedView(page == "fx");
        native->setSize(1080, page == "fx" ? 1160 : 716);
        checkInteractiveTargets(*native);
        const auto image = native->createComponentSnapshot(native->getLocalBounds());
        REQUIRE(image.getWidth() == 1080);
        REQUIRE(image.getHeight() == native->getHeight());
        auto output = destination.getChildFile("openKicker-" + page + ".png").createOutputStream();
        REQUIRE(output != nullptr);
        REQUIRE(output->setPosition(0));
        REQUIRE(output->truncate().wasOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(image, *output));
    }
}

TEST_CASE("Temporary fill CC overrides do not replace saved custom pattern edits", "[routing][state]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 32);
    processor.togglePianoRollNote(7, 4, 2, 3);
    processor.routing().assignCc("auto_fill", 35);
    juce::AudioBuffer<float> audio(2, 32);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 127), 0);
    processor.processBlock(audio, midi);
    (void)processor.currentPianoRollNotes();
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    const auto notes = restored.currentPianoRollNotes();
    REQUIRE(std::any_of(notes.begin(), notes.end(), [](const auto& note) { return note.step64 == 7 && note.layer == 2 && note.ratchets == 3; }));
}

TEST_CASE("A direct edit back to the host value clears a CC mirror", "[routing][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::Knob knob(processor.parameters, "body_tone", "Harmonics", juce::Colours::orange);
    const float host = processor.parameters.getRawParameterValue("body_tone")->load();
    processor.routing().assignCc("body_tone", 35);
    processor.routing().applyCc(35, 127);
    knob.refreshMidiMirror();
    knob.slider().setValue(host, juce::sendNotificationSync);
    REQUIRE_FALSE(processor.routing().isOverridden("body_tone"));
    REQUIRE_THAT(processor.routing().displayValue("body_tone"), WithinAbs(host, 0.0001f));
}

TEST_CASE("Factory programs keep ADSR opt-in and respect an ADSR lock", "[followup][routing][presets]") {
    OpenEdmKickProcessor processor;
    setParam(processor, "amp_enabled", 1);
    processor.setCurrentProgram(1);
    REQUIRE_FALSE(processor.currentSynthSettings().ampEnabled);
    processor.routing().setLocked("amp_enabled", true);
    setParam(processor, "amp_enabled", 1);
    processor.setCurrentProgram(2);
    REQUIRE(processor.currentSynthSettings().ampEnabled);
}

TEST_CASE("Native controls provide routing affordances for every host parameter", "[routing][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    juce::StringArray represented;
    const auto visit = [&](auto&& self, juce::Component& parent) -> void {
        if (parent.getComponentID().startsWith("routing_")) represented.addIfNotAlreadyThere(parent.getComponentID().substring(8));
        for (auto* child : parent.getChildren()) self(self, *child);
    };
    for (int scene = 0; scene < 4; ++scene) {
        setParam(processor, "auto_slot", static_cast<float>(scene));
        for (const auto* page : {"kick", "layers", "pattern", "fx"}) {
            auto* tab = dynamic_cast<juce::TextButton*>(editor->findChildWithID(juce::String("page_") + page));
            REQUIRE(tab != nullptr);
            tab->onClick();
            visit(visit, *editor);
        }
    }
    for (auto* parameter : processor.getParameters()) {
        const auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        REQUIRE(ranged != nullptr);
        CAPTURE(ranged->paramID);
        REQUIRE(represented.contains(ranged->paramID));
    }
}

TEST_CASE("Host bypass processing still consumes matching ADSR note-offs", "[followup][adsr][tail]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(8000, 128);
    setParam(processor, "amp_enabled", 1);
    setParam(processor, "amp_release_ms", 10);
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    processor.processBlock(audio, midi);
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 36), 0);
    processor.processBlockBypassed(audio, midi);
    REQUIRE_THAT(static_cast<float>(processor.getTailLengthSeconds()), WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(audio.getMagnitude(0, 128), WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("Dense fill CC streams coalesce factory reloads before the Auto trigger", "[routing][realtime]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 32);
    for (int slot = 0; slot < 4; ++slot) {
        const auto prefix = slot == 0 ? juce::String("auto_") : "auto_" + juce::String::charToString(static_cast<juce::juce_wchar>('a' + slot)) + "_";
        setParam(processor, prefix + "bars", 6);
        processor.routing().assignCc(prefix + "fill", 35);
    }
    juce::AudioBuffer<float> audio(2, 32);
    juce::MidiBuffer midi;
    for (int event = 0; event < 65535; ++event) midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, event % 128), 0);
    const auto began = std::chrono::steady_clock::now();
    processor.processBlock(audio, midi);
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    CAPTURE(elapsed);
    REQUIRE(elapsed < 500.0);
    REQUIRE(processor.autoSlotSettings(0).fillPreset == 27);
    REQUIRE(processor.autoSlotSettings(3).fillPreset == 27);
}

TEST_CASE("Resetting temporary fill CC control restores the live custom pattern", "[routing][state]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 32);
    TestPlayHead host;
    host.playing = true;
    processor.setPlayHead(&host);
    setParam(processor, "auto_on", 1);
    processor.togglePianoRollNote(7, 4, 2, 3);
    processor.routing().assignCc("auto_fill", 35);
    juce::AudioBuffer<float> shortAudio(2, 32);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 127), 0);
    processor.processBlock(shortAudio, midi);
    const auto before = processor.displayTriggerCount.load();
    juce::AudioBuffer<float> audio(2, 13000);
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 121, 127), 0);
    processor.processBlock(audio, midi);
    REQUIRE(processor.displayTriggerCount.load() - before == 3);
}

TEST_CASE("Sub-octave, transient punch, and stereo width shape the kick output", "[plugin][dsp]") {
    OpenEdmKickProcessor dryProc, enhancedProc;
    dryProc.prepareToPlay(48000.0, 2048);
    enhancedProc.prepareToPlay(48000.0, 2048);
    setParam(enhancedProc, "sub_octave", 0.85f);
    setParam(enhancedProc, "click_punch", 0.85f);
    setParam(enhancedProc, "stereo_width", 0.85f);

    juce::AudioBuffer<float> dryBuf(2, 2048), enhancedBuf(2, 2048);
    juce::MidiBuffer midi1, midi2;
    midi1.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    midi2.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    dryProc.processBlock(dryBuf, midi1);
    enhancedProc.processBlock(enhancedBuf, midi2);

    float diff = 0.0f;
    float stereoSide = 0.0f;
    for (int i = 0; i < 2048; ++i) {
        diff += std::abs(dryBuf.getSample(0, i) - enhancedBuf.getSample(0, i));
        stereoSide += std::abs(enhancedBuf.getSample(0, i) - enhancedBuf.getSample(1, i));
    }
    REQUIRE(diff > 1.0f);
    REQUIRE(stereoSide > 0.1f);
}
