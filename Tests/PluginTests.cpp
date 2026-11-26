#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <numeric>
#include <numbers>
#include <new>
#include <ranges>
#include <ranges>
#include <cstdlib>
#include <fstream>
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

juce::Component* findId(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findId(*child, id)) return found;
    return nullptr;
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



TEST_CASE("Host play does not invent notes", "[plugin]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setParam(processor, "clock_sync", 1.0f);
    TestPlayHead playHead;
    playHead.playing = true;
    processor.setPlayHead(&playHead);

    juce::AudioBuffer<float> buffer(2, 1024);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    float playingEnergy = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        playingEnergy += std::abs(buffer.getSample(0, sample));
    REQUIRE_THAT(playingEnergy, WithinAbs(0.0f, 1.0e-4f));

    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    processor.processBlock(buffer, midi);
    float noted = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        noted += std::abs(buffer.getSample(0, sample));
    REQUIRE(noted > 1.0f);
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
    REQUIRE(editor->getHeight() == 764);
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
    dynamic_cast<suite::PresetBrowser*>(editor->findChildWithID("presetBrowser"))
        ->setDirectory(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openkicker-no-presets"));
    CHECK(presets->getRootMenu()->getNumItems() == 6);
    presets->setSelectedId(56, juce::sendNotificationSync);
    REQUIRE(processor.getCurrentProgram() == 55);
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
    for (const auto* name : {"kick", "fx"}) {
        auto* tab = dynamic_cast<juce::TextButton*>(kickEditor->findChildWithID(juce::String("page_") + name));
        REQUIRE(tab != nullptr);
        tab->onClick();
        checkInteractiveTargets(*kickEditor);
    }
}

TEST_CASE("Dials stay at least 40 px and controls do not overlap at the default size", "[plugin][ui][layout]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* native = dynamic_cast<OpenEdmKickEditor*>(editor.get());
    REQUIRE(native != nullptr);
    int dials = 0;
    const auto visit = [&](auto&& self, juce::Component& parent) -> void {
        std::vector<juce::Component*> shown;
        std::ranges::copy_if(parent.getChildren(), std::back_inserter(shown), [](auto* child) { return child->isVisible(); });
        std::ranges::for_each(shown, [&](juce::Component* child) {
            CAPTURE(parent.getComponentID(), child->getComponentID(), child->getName());
            CHECK(std::ranges::none_of(shown, [child](juce::Component* other) {
                return other != child && other->getBounds().intersects(child->getBounds());
            }));
            if (auto* slider = dynamic_cast<juce::Slider*>(child); slider != nullptr && slider->isRotary()) {
                const auto dial = slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds;
                CHECK(std::min(dial.getWidth(), dial.getHeight()) >= 40);
                ++dials;
            }
            self(self, *child);
        });
    };
    const std::array<std::pair<bool, float>, 3> views{ { { false, 0.0f }, { false, 1.0f }, { true, 0.0f } } };
    std::ranges::for_each(views, [&](const auto& view) {
        setParam(processor, "amp_enabled", view.second);
        native->setAdvancedView(view.first);
        std::ranges::for_each(std::array{ "kick", "fx" }, [&](const char* page) {
            dynamic_cast<juce::TextButton*>(native->findChildWithID(juce::String("page_") + page))->onClick();
            visit(visit, *native);
            checkInteractiveTargets(*native);
        });
    });
    CHECK(native->getHeight() == 764);
    CHECK(dials > 100);
}

TEST_CASE("The waveform redraws the full hit soon after a change", "[plugin][ui][waveform]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::KickWaveformView view(processor);
    view.setSize(860, 98);
    const auto before = view.createComponentSnapshot(view.getLocalBounds());
    setParam(processor, "output_gain", -18.0f);
    juce::Thread::sleep(150);
    juce::Timer::callPendingTimersSynchronously();
    const auto after = view.createComponentSnapshot(view.getLocalBounds());
    const auto changed = std::ranges::count_if(std::views::iota(0, before.getWidth() * before.getHeight()), [&](int pixel) {
        const int x = pixel % before.getWidth();
        const int y = pixel / before.getWidth();
        return before.getPixelAt(x, y) != after.getPixelAt(x, y);
    });
    CHECK(changed > 200);
}

TEST_CASE("Every visible control shows the name its host parameter uses", "[plugin][ui][names]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* native = dynamic_cast<OpenEdmKickEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->setAdvancedView(true);
    const auto words = [](const juce::String& text) {
        juce::StringArray tokens;
        tokens.addTokens(text.toLowerCase().replaceCharacter('-', ' '), " ", {});
        tokens.removeEmptyStrings();
        return tokens;
    };
    int named = 0;
    const auto visit = [&](auto&& self, juce::Component& parent) -> void {
        std::ranges::for_each(parent.getChildren(), [&](juce::Component* child) {
            if (!child->isVisible()) return;
            auto* parameter = processor.parameters.getParameter(child->getComponentID());
            if (parameter != nullptr && child->getName().isNotEmpty()) {
                const auto hostWords = words(parameter->getName(100));
                CAPTURE(child->getName(), parameter->getName(100));
                CHECK(std::ranges::all_of(words(child->getName()), [&](const juce::String& word) { return hostWords.contains(word); }));
                ++named;
            }
            self(self, *child);
        });
    };
    std::ranges::for_each(std::array{ "kick", "fx" }, [&](const char* page) {
        dynamic_cast<juce::TextButton*>(native->findChildWithID(juce::String("page_") + page))->onClick();
        visit(visit, *native);
    });
    CHECK(named > 40);
}



TEST_CASE("Kick settings that have no effect draw dim and say what turns them on", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    openedmkick::ui::KickSynthPanel kick(processor);
    kick.setSize(1040, 576);
    auto* ceiling = dynamic_cast<juce::Slider*>(findId(kick, "limiter_ceiling"));
    REQUIRE(ceiling != nullptr);
    CHECK(ceiling->getParentComponent()->getAlpha() == 1.0f);
    setParam(processor, "limiter_enabled", 0.0f);
    kick.refreshActivity();
    CHECK(ceiling->getParentComponent()->getAlpha() < 0.5f);
    CHECK(ceiling->getTooltip() == "Turn on Limiter to use Ceiling.");
}

TEST_CASE("Double-click returns every visible dial to its default", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* native = dynamic_cast<OpenEdmKickEditor*>(editor.get());
    REQUIRE(native != nullptr);
    native->setAdvancedView(true);
    int dials = 0;
    const auto visit = [&](auto&& self, juce::Component& parent) -> void {
        std::ranges::for_each(parent.getChildren(), [&](juce::Component* child) {
            if (!child->isVisible()) return;
            auto* slider = dynamic_cast<juce::Slider*>(child);
            auto* parameter = processor.parameters.getParameter(child->getComponentID());
            if (slider != nullptr && parameter != nullptr) {
                CAPTURE(slider->getComponentID());
                CHECK(slider->isDoubleClickReturnEnabled());
                CHECK_THAT(slider->getDoubleClickReturnValue(),
                           WithinAbs(parameter->convertFrom0to1(parameter->getDefaultValue()), 0.0001));
                ++dials;
            }
            self(self, *child);
        });
    };
    std::ranges::for_each(std::array{ "kick", "fx" }, [&](const char* page) {
        auto* tab = dynamic_cast<juce::TextButton*>(native->findChildWithID(juce::String("page_") + page));
        REQUIRE(tab != nullptr);
        tab->onClick();
        visit(visit, *native);
    });
    CHECK(dials > 40);
}

TEST_CASE("MIDI input tracks pitch and octave, including notes from other channels", "[plugin][midi]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    setParam(processor, "limiter_enabled", 0.0f);

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



TEST_CASE("Every float parameter reads back the text it shows", "[plugin][parameters]") {
    OpenEdmKickProcessor processor;
    const float position = GENERATE(0.0f, 0.13f, 0.5f, 0.77f, 1.0f);
    int checked = 0;
    std::ranges::for_each(processor.getParameters(), [&](juce::AudioProcessorParameter* parameter) {
        const auto* ranged = dynamic_cast<juce::AudioParameterFloat*>(parameter);
        if (ranged == nullptr) return;
        const auto text = parameter->getText(position, 64);
        CAPTURE(ranged->paramID, text);
        const float parsed = parameter->getValueForText(text);
        CHECK(parameter->getText(parsed, 64) == text);
        ++checked;
    });
    CHECK(checked > 40);
}



TEST_CASE("Output peak reads after the limiter and holds until the editor takes it", "[plugin][meter]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 4096);
    setParam(processor, "output_gain", 12.0f);
    juce::AudioBuffer<float> buffer(2, 4096);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    processor.processBlock(buffer, midi);
    const float limited = processor.displayPeak.exchange(0.0f);
    CHECK_THAT(limited, WithinAbs(std::max(buffer.getMagnitude(0, 0, 4096), buffer.getMagnitude(1, 0, 4096)), 1.0e-6f));
    CHECK(limited > 0.5f);
    CHECK(limited <= juce::Decibels::decibelsToGain(-0.3f) + 1.0e-3f);
    CHECK(processor.displayPeak.exchange(0.0f) == 0.0f);

    setParam(processor, "limiter_enabled", 0.0f);
    processor.processBlock(buffer, midi);
    CHECK(processor.displayPeak.exchange(0.0f) > 1.0f);
}

TEST_CASE("MIDI CC21 activates the host-synced high-pass in the plug-in", "[plugin][midi][effects]") {
    const auto render = [](bool sendCc) {
        OpenEdmKickProcessor processor;
        processor.prepareToPlay(48000.0, 8192);
        setParam(processor, "limiter_enabled", 0.0f);
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
    REQUIRE(programs.size() == 56u);
    REQUIRE(fills.empty());
    REQUIRE(std::string(programs.front().name) == "Uplifting / First Light");
    REQUIRE(std::string(programs.back().name) == "Classic / Final Press");

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

TEST_CASE("Presets keep their name and edit marker through undo, A/B and the session", "[plugin][presets][state]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    CHECK(processor.presetName() == "Uplifting / First Light");
    CHECK_FALSE(processor.isPresetEdited());
    auto* decay = processor.parameters.getParameter("decay_ms");
    decay->beginChangeGesture();
    decay->setValueNotifyingHost(decay->convertTo0to1(640.0f));
    decay->endChangeGesture();
    CHECK(processor.isPresetEdited());
    REQUIRE(processor.undo());
    CHECK_FALSE(processor.isPresetEdited());

    REQUIRE(processor.applySoundState(processor.captureSoundState(), "Zed Kick"));
    CHECK(processor.presetName() == "Zed Kick");
    setParam(processor, "decay_ms", 300.0f);
    CHECK(processor.isPresetEdited());
    processor.selectAbSlot(1);
    CHECK(processor.presetName() == "Uplifting / First Light");
    CHECK_FALSE(processor.isPresetEdited());
    processor.selectAbSlot(0);
    CHECK(processor.presetName() == "Zed Kick");
    CHECK(processor.isPresetEdited());

    juce::MemoryBlock session;
    processor.getStateInformation(session);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    CHECK(restored.presetName() == "Zed Kick");
    CHECK(restored.isPresetEdited());
    restored.setCurrentProgram(3);
    CHECK(restored.presetName() == restored.getProgramName(3));
    CHECK_FALSE(restored.isPresetEdited());
}

TEST_CASE("Restoring a session saved on slot B keeps undo isolated from slot A", "[plugin][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    setParam(processor, "decay_ms", 400.0f);
    processor.selectAbSlot(1);
    setParam(processor, "decay_ms", 700.0f);

    juce::MemoryBlock session;
    processor.getStateInformation(session);

    OpenEdmKickProcessor restored;
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    REQUIRE(restored.abSlot() == 1);

    auto* decay = restored.parameters.getParameter("decay_ms");
    REQUIRE(decay != nullptr);
    decay->beginChangeGesture();
    decay->setValueNotifyingHost(decay->convertTo0to1(900.0f));
    decay->endChangeGesture();
    CHECK(restored.canUndo());

    restored.selectAbSlot(0);
    CHECK_FALSE(restored.canUndo());
    CHECK_FALSE(restored.undo());
    CHECK_THAT(decay->convertFrom0to1(decay->getValue()), WithinAbs(400.0f, 1.0f));

    restored.selectAbSlot(1);
    REQUIRE(restored.canUndo());
    REQUIRE(restored.undo());
    CHECK_THAT(decay->convertFrom0to1(decay->getValue()), WithinAbs(700.0f, 1.0f));
}

TEST_CASE("Previous and Next step the browser list and the header marks edits", "[plugin][presets][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    CHECK_FALSE(processor.isPresetEdited());
    auto* browser = dynamic_cast<suite::PresetBrowser*>(editor->findChildWithID("presetBrowser"));
    auto* presets = dynamic_cast<juce::ComboBox*>(editor->findChildWithID("factoryPreset"));
    auto* previous = dynamic_cast<juce::TextButton*>(editor->findChildWithID("prevPreset"));
    auto* next = dynamic_cast<juce::TextButton*>(editor->findChildWithID("nextPreset"));
    REQUIRE(browser != nullptr);
    REQUIRE(presets != nullptr);
    REQUIRE(previous != nullptr);
    REQUIRE(next != nullptr);
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("openkicker-step-" + juce::String(juce::Time::currentTimeMillis()));
    suite::PresetLibrary library("openEDMKick", {});
    library.setDirectory(directory);
    setParam(processor, "decay_ms", 640.0f);
    REQUIRE(library.save("Zed Kick", processor.captureSoundState()));
    browser->setDirectory(directory);
    const int last = processor.getNumPrograms() - 1;
    processor.setCurrentProgram(last);

    next->onClick();
    CHECK(presets->getText() == "Zed Kick");
    CHECK_THAT(processor.parameters.getRawParameterValue("decay_ms")->load(), WithinAbs(640.0f, 0.1f));
    next->onClick();
    CHECK(processor.getCurrentProgram() == 0);
    CHECK(presets->getText() == "Uplifting / First Light");
    previous->onClick();
    CHECK(presets->getText() == "Zed Kick");
    setParam(processor, "decay_ms", 200.0f);
    CHECK(processor.presetDisplayName() == "Zed Kick *");
    previous->onClick();
    CHECK(processor.getCurrentProgram() == last);
    CHECK(presets->getText() == processor.getProgramName(last));
    directory.deleteRecursively();
}

TEST_CASE("All 56 trance kick presets load and report valid program names", "[plugin][presets]") {
    OpenEdmKickProcessor processor;
    const auto& presets = openedmkick::presets::factoryPresets();
    REQUIRE(processor.getNumPrograms() == openedmkick::presets::kFactoryProgramCount);

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
    REQUIRE(presets.size() == 56u);
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
        CHECK_FALSE(settings.ampEnabled);
        for (int note : {29, 33, 36}) {
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, note, 1.0f), 0);
            processor.processBlock(hit, midi);
            CAPTURE(note);
            const float peak = hit.getMagnitude(0, hit.getNumSamples());
            CHECK(peak <= 1.0f);
            CHECK(hit.getMagnitude(0, rate / 50) > 0.25f);

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
            CHECK(fundamentalPower > 0.55 * energy / count);
        }
    }
}

TEST_CASE("Factory kicks stay off the limiter and in a usable level range", "[plugin][presets][quality]") {
    constexpr int rate = 48000;
    OpenEdmKickProcessor processor;
    juce::AudioBuffer<float> hit(2, rate / 2);
    processor.prepareToPlay(rate, hit.getNumSamples());
    float minPeak = 10.0f;
    float maxPeak = 0.0f;
    float minRms = 10.0f;
    float maxRms = 0.0f;
    for (int index = 0; index < processor.getNumPrograms(); ++index) {
        processor.setCurrentProgram(index);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        processor.processBlock(hit, midi);
        const float peak = hit.getMagnitude(0, hit.getNumSamples());
        double energy = 0.0;
        int nans = 0;
        for (int i = 0; i < hit.getNumSamples(); ++i) {
            const float sample = hit.getSample(0, i);
            if (!std::isfinite(sample)) ++nans;
            energy += static_cast<double>(sample) * sample;
        }
        INFO(processor.getProgramName(index).toStdString());
        CHECK(nans == 0);
        CHECK(peak < 0.90f);
        CHECK(peak > 0.12f);
        const float rms = std::sqrt(static_cast<float>(energy / hit.getNumSamples()));
        minPeak = std::min(minPeak, peak);
        maxPeak = std::max(maxPeak, peak);
        minRms = std::min(minRms, rms);
        maxRms = std::max(maxRms, rms);
        if (const char* dir = std::getenv("OEK_AUDITION_DIR")) {
            const auto path = std::string(dir) + "/" + (index < 9 ? "0" : "") + std::to_string(index + 1) + ".wav";
            std::ofstream out(path, std::ios::binary);
            const uint32_t dataBytes = static_cast<uint32_t>(hit.getNumSamples() * 6);
            auto u16 = [&](uint16_t v) { out.put(static_cast<char>(v)); out.put(static_cast<char>(v >> 8)); };
            auto u32 = [&](uint32_t v) {
                out.put(static_cast<char>(v)); out.put(static_cast<char>(v >> 8));
                out.put(static_cast<char>(v >> 16)); out.put(static_cast<char>(v >> 24));
            };
            out.write("RIFF", 4); u32(36 + dataBytes); out.write("WAVE", 4);
            out.write("fmt ", 4); u32(16); u16(1); u16(2); u32(rate); u32(rate * 6); u16(6); u16(24);
            out.write("data", 4); u32(dataBytes);
            for (int i = 0; i < hit.getNumSamples(); ++i) {
                for (int channel = 0; channel < 2; ++channel) {
                    const int v = std::clamp(static_cast<int>(std::lround(hit.getSample(channel, i) * 8388607.0)), -8388608, 8388607);
                    out.put(static_cast<char>(v)); out.put(static_cast<char>(v >> 8)); out.put(static_cast<char>(v >> 16));
                }
            }
        }
    }
    CHECK(maxPeak / std::max(minPeak, 1.0e-6f) < 3.2f);
    CHECK(maxRms / std::max(minRms, 1.0e-6f) < 4.0f);
}





TEST_CASE("The kick is one voice without layer zones, pattern scenes, or master bypass", "[plugin]") {
    OpenEdmKickProcessor processor;
    for (const char* id : { "layer_a_pan", "master_bypass", "auto_on", "crush_mix", "click_sample_on", "output", "ceiling" })
        REQUIRE(processor.parameters.getParameter(id) == nullptr);
    REQUIRE(processor.parameters.getParameter("output_gain") != nullptr);
    REQUIRE(processor.parameters.getParameter("limiter_ceiling") != nullptr);
    REQUIRE(processor.parameters.getParameter("dist_drive") != nullptr);
    REQUIRE(processor.parameters.getParameter("comp_mix") != nullptr);
    REQUIRE(processor.parameters.getParameter("reverb_mix") != nullptr);
    REQUIRE(processor.getBypassParameter() == nullptr);
    const auto layer = processor.currentLayerSettings()[0];
    REQUIRE(layer.sound == openedmkick::core::DrumSoundType::kick);
    REQUIRE(layer.keyLow == 0);
    REQUIRE(layer.keyHigh == 127);
    REQUIRE(processor.currentLayerSettings().size() == 1);
    REQUIRE_FALSE(processor.loadUserClickSample(juce::File{}));
}







TEST_CASE("Audition remains at sample zero before later MIDI controller events", "[layer-improvements][midi]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 256);
    setParam(processor, "limiter_enabled", 0);
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
    processor.routing().toggleSection({"body_tone", "sub_level"});
    REQUIRE(processor.routing().isLocked("body_tone"));
    REQUIRE(processor.routing().isLocked("sub_level"));
    REQUIRE_FALSE(processor.routing().isLocked("decay_ms"));
    processor.routing().toggleSection({"body_tone", "sub_level"});
    REQUIRE_FALSE(processor.routing().isLocked("body_tone"));
    REQUIRE_FALSE(processor.routing().isLocked("sub_level"));
}

TEST_CASE("CC menus expose all controllers and retain factory assignments", "[routing]") {
    OpenEdmKickProcessor processor;
    REQUIRE(processor.routing().assignment("reverse_on") == -1);
    REQUIRE(processor.routing().assignment("hp_midi_on") == -1);
    REQUIRE(processor.routing().assignment("lp_midi_on") == -1);
    REQUIRE(processor.routing().assignment("midi_volume") == -1);
    REQUIRE(processor.routing().assignment("midi_expression") == -1);
    REQUIRE(processor.routing().assignment("midi_brightness") == -1);
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
    source.routing().assignCc("low_cut_hz", 35);
    juce::MemoryBlock saved;
    source.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.routing().setLocked("body_tone", true);
    setParam(restored, "body_tone", 0.9f);
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(restored.currentSynthSettings().bodyTone, WithinAbs(0.234f, 0.0001f));
    REQUIRE(restored.routing().isLocked("body_tone"));
    REQUIRE(restored.routing().assignment("low_cut_hz") == -1);
}

TEST_CASE("Assigned CC endpoints and discrete values reach runtime without APVTS writes", "[routing]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000, 64);
    processor.routing().assignCc("limiter_enabled", 35);
    processor.routing().assignCc("click_type", 35);
    const float hostLimiter = processor.parameters.getRawParameterValue("limiter_enabled")->load();
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 0), 16);
    processor.processBlock(audio, midi);
    REQUIRE_THAT(processor.routing().displayValue("limiter_enabled"), WithinAbs(hostLimiter, 0.0001f));
    REQUIRE_THAT(processor.routing().displayValue("click_type"), WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(processor.parameters.getRawParameterValue("limiter_enabled")->load(), WithinAbs(hostLimiter, 0.0001f));
    midi.clear();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 35, 127), 0);
    processor.processBlock(audio, midi);
    REQUIRE_THAT(processor.routing().displayValue("limiter_enabled"), WithinAbs(hostLimiter, 0.0001f));
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
    const float shown = static_cast<float>(knob.slider().getValue());
    processor.routing().applyCc(35, 127);
    knob.refreshMidiMirror();
    REQUIRE_THAT(static_cast<float>(knob.slider().getValue()), WithinAbs(shown, 0.0001f));
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
    setParam(processor, "limiter_enabled", 0);
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
        setParam(processor, "limiter_enabled", 0);
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
    REQUIRE_THAT(processor.routing().displayValue("amp_enabled"), WithinAbs(0.0f, 0.0001f));
    processor.routing().applyCc(35, 0);
    REQUIRE_THAT(processor.routing().displayValue("amp_enabled"), WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(processor.routing().displayValue("click_type"), WithinAbs(0.0f, 0.0001f));
    processor.routing().setLocked("limiter_enabled", true);
    processor.routing().assignCc("reverse_on", 90);
    auto state = processor.parameters.copyState();
    state.removeChild(state.getChildWithName("ParameterRouting"), nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary(*state.createXml(), saved);
    processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_FALSE(processor.routing().isLocked("limiter_enabled"));
    REQUIRE(processor.routing().assignment("reverse_on") == -1);
    REQUIRE(processor.routing().assignment("amp_enabled") == -1);
}

TEST_CASE("Mapped CC order controls the following note rather than the preceding one", "[routing][midi]") {
    const auto render = [](bool ccFirst) {
        OpenEdmKickProcessor processor;
        processor.prepareToPlay(48000, 512);
        processor.routing().assignCc("sweep_octaves", 35);
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
    REQUIRE(difference < 0.001);
}



TEST_CASE("Capture the factory default editor at its default size", "[preview]") {
    const auto path = juce::SystemStats::getEnvironmentVariable("OEK_PREVIEW_PNG", {});
    if (path.isEmpty()) SKIP("Set OEK_PREVIEW_PNG to write the default editor preview");
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor != nullptr);
    CHECK(editor->getWidth() == 1080);
    CHECK(editor->getHeight() == 764);
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    REQUIRE(image.getWidth() == 1080);
    REQUIRE(image.getHeight() == 764);
    const juce::File destination(path);
    REQUIRE(destination.getParentDirectory().createDirectory().wasOk());
    auto output = destination.createOutputStream();
    REQUIRE(output != nullptr);
    REQUIRE(output->setPosition(0));
    REQUIRE(output->truncate().wasOk());
    REQUIRE(juce::PNGImageFormat().writeImageToStream(image, *output));
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
    for (int scene = 0; scene < 1; ++scene) {
        juce::ignoreUnused(scene);
        for (const auto* page : {"kick", "fx"}) {
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

TEST_CASE("Main page shows Output controls + 3-band EQ while drive/distortion params are removed", "[plugin][ui][eq]") {
    OpenEdmKickProcessor processor;
    std::ranges::for_each(std::array<const char*, 7>{
        "body_drive", "saturation", "grit", "clip", "drive", "layer_a_drive_offset", "layer_d_drive_offset"},
        [&](const char* id) { REQUIRE(processor.parameters.getParameter(id) == nullptr); });
    std::ranges::for_each(std::array<const char*, 6>{
        "eq_low_freq", "eq_low_db", "eq_mid_freq", "eq_mid_db", "eq_high_freq", "eq_high_db"},
        [&](const char* id) { REQUIRE(processor.parameters.getParameter(id) != nullptr); });

    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor != nullptr);
    const auto findById = [](auto&& self, juce::Component& parent, const juce::String& id) -> juce::Component* {
        if (parent.getComponentID() == id) return &parent;
        for (auto* child : parent.getChildren()) {
            if (auto* found = self(self, *child, id)) return found;
        }
        return nullptr;
    };
    std::ranges::for_each(std::array<const char*, 10>{
        "routing_eq_low_db", "routing_eq_low_freq", "routing_eq_mid_db", "routing_eq_mid_freq",
        "routing_eq_high_db", "routing_eq_high_freq", "routing_output_gain", "routing_stereo_width",
        "routing_limiter_ceiling", "routing_limiter_enabled"},
        [&](const char* id) {
            auto* control = findById(findById, *editor, id);
            REQUIRE(control != nullptr);
            CHECK(control->isVisible());
            CHECK(control->findParentComponentOfClass<openedmkick::ui::KickSynthPanel>() != nullptr);
        });
}

TEST_CASE("Every slider, combo box, and button has a screen-reader title", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor != nullptr);

    int checked = 0;
    const auto walk = [&](auto&& self, juce::Component& root) -> void {
        for (auto* child : root.getChildren()) {
            if (dynamic_cast<juce::Slider*>(child) != nullptr
                || dynamic_cast<juce::ComboBox*>(child) != nullptr
                || dynamic_cast<juce::Button*>(child) != nullptr
                || dynamic_cast<openedmkick::ui::ParameterControl*>(child) != nullptr) {
                INFO(child->getComponentID() << " / " << child->getName());
                CHECK(child->getTitle().isNotEmpty());
                ++checked;
            }
            self(self, *child);
        }
    };
    walk(walk, *editor);
    CHECK(checked > 50);
}

TEST_CASE("Look and feel dims disabled buttons, sliders, and combo boxes", "[plugin][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    openedmkick::ui::EdmLookAndFeel lookAndFeel;
    const auto meanAlpha = [](const juce::Image& image) {
        double sum = 0.0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                sum += image.getPixelAt(x, y).getFloatAlpha();
        return static_cast<float>(sum / static_cast<double>(image.getWidth() * image.getHeight()));
    };
    const auto renderControl = [&](juce::Component& c, int w, int h) {
        c.setLookAndFeel(&lookAndFeel);
        c.setSize(w, h);
        juce::Image image(juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        juce::Graphics g(image);
        c.paintEntireComponent(g, true);
        c.setLookAndFeel(nullptr);
        return meanAlpha(image);
    };

    juce::TextButton button("Undo");
    const float buttonEnabledAlpha = renderControl(button, 80, 40);
    button.setEnabled(false);
    const float buttonDisabledAlpha = renderControl(button, 80, 40);
    CHECK(buttonEnabledAlpha > 0.5f);
    CHECK(buttonDisabledAlpha < buttonEnabledAlpha * 0.6f);

    juce::Slider slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    slider.setRange(0.0, 1.0);
    slider.setValue(0.5);
    const float sliderEnabledAlpha = renderControl(slider, 60, 60);
    slider.setEnabled(false);
    const float sliderDisabledAlpha = renderControl(slider, 60, 60);
    CHECK(sliderEnabledAlpha > 0.3f);
    CHECK(sliderDisabledAlpha < sliderEnabledAlpha * 0.6f);

    juce::ComboBox box;
    box.addItem("One", 1);
    box.setSelectedId(1);
    const float boxEnabledAlpha = renderControl(box, 120, 40);
    box.setEnabled(false);
    const float boxDisabledAlpha = renderControl(box, 120, 40);
    CHECK(boxEnabledAlpha > 0.5f);
    CHECK(boxDisabledAlpha < boxEnabledAlpha * 0.6f);
}

TEST_CASE("Every RangedAudioParameter can be locked, persists through state, and survives preset loads", "[routing][plugin]") {
    OpenEdmKickProcessor processor;

    std::vector<juce::RangedAudioParameter*> rangedParams;
    for (auto* param : processor.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            rangedParams.push_back(ranged);
    }
    REQUIRE(rangedParams.size() > 50);

    // Capture an unlocked user preset state before locking and modifying parameters.
    OpenEdmKickProcessor otherPresetSource;
    otherPresetSource.setCurrentProgram(3);
    const juce::MemoryBlock userPresetState = otherPresetSource.captureSoundState();

    // Set every parameter to a distinct non-preset normalized value and lock it.
    std::vector<float> expectedNormalized(rangedParams.size());
    for (size_t i = 0; i < rangedParams.size(); ++i) {
        auto* ranged = rangedParams[i];
        const float current = ranged->getValue();
        const float target = ranged->isDiscrete()
            ? (current < 0.5f ? 1.0f : 0.0f)
            : (current < 0.5f ? 0.8f : 0.2f);
        ranged->setValueNotifyingHost(target);
        expectedNormalized[i] = ranged->getValue();
        processor.routing().setLocked(ranged->paramID, true);
        REQUIRE(processor.routing().isLocked(ranged->paramID));
    }

    // Verify all locks and values survive getStateInformation / setStateInformation.
    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (size_t i = 0; i < rangedParams.size(); ++i) {
        const auto& id = rangedParams[i]->paramID;
        INFO("State restore failed for " << id);
        REQUIRE(restored.routing().isLocked(id));
        auto* restoredParam = dynamic_cast<juce::RangedAudioParameter*>(restored.parameters.getParameter(id));
        REQUIRE(restoredParam != nullptr);
        REQUIRE_THAT(restoredParam->getValue(), WithinAbs(expectedNormalized[i], 0.001f));
    }

    // Verify all locks and values survive factory preset load.
    restored.setCurrentProgram(1);
    for (size_t i = 0; i < rangedParams.size(); ++i) {
        const auto& id = rangedParams[i]->paramID;
        INFO("Factory preset load overwrote locked " << id);
        REQUIRE(restored.routing().isLocked(id));
        auto* restoredParam = dynamic_cast<juce::RangedAudioParameter*>(restored.parameters.getParameter(id));
        REQUIRE_THAT(restoredParam->getValue(), WithinAbs(expectedNormalized[i], 0.001f));
    }

    // Verify all locks and values survive user preset load.
    REQUIRE(restored.applySoundState(userPresetState, "Locked User Preset"));
    for (size_t i = 0; i < rangedParams.size(); ++i) {
        const auto& id = rangedParams[i]->paramID;
        INFO("User preset load overwrote locked " << id);
        REQUIRE(restored.routing().isLocked(id));
        auto* restoredParam = dynamic_cast<juce::RangedAudioParameter*>(restored.parameters.getParameter(id));
        REQUIRE_THAT(restoredParam->getValue(), WithinAbs(expectedNormalized[i], 0.001f));
    }
}

TEST_CASE("EQ bands snap to chromatic notes across two octaves", "[plugin][eq][notes]") {
    namespace params = openedmkick::params;
    OpenEdmKickProcessor processor;

    auto checkBand = [&](const char* id, int minMidi, int maxMidi, const char* lowName, const char* midName, const char* highName, const char* parsedName) {
        auto* param = processor.parameters.getParameter(id);
        REQUIRE(param != nullptr);
        REQUIRE(maxMidi - minMidi == 24);
        REQUIRE(param->getText(0.0f, 16) == lowName);
        REQUIRE(param->getText(0.5f, 16) == midName);
        REQUIRE(param->getText(1.0f, 16) == highName);

        const float parsedNorm = param->getValueForText(parsedName);
        REQUIRE(param->getText(parsedNorm, 16) == parsedName);
        const float hz = param->convertFrom0to1(parsedNorm);
        const float snapped = param->getNormalisableRange().snapToLegalValue(hz * 1.01f);
        REQUIRE_THAT(snapped, WithinAbs(hz, 0.01f));
    };

    checkBand(params::kEqLowFreq, params::kEqLowMinMidi, params::kEqLowMaxMidi, "A0", "A1", "A2", "G#1");
    checkBand(params::kEqMidFreq, params::kEqMidMinMidi, params::kEqMidMaxMidi, "A3", "A4", "A5", "G#4");
    checkBand(params::kEqHighFreq, params::kEqHighMinMidi, params::kEqHighMaxMidi, "A6", "A7", "A8", "G#7");
    const auto noteMenu = suite::eq::noteMenu(params::kEqLowMinMidi, params::kEqLowMaxMidi, 28, 1);
    CHECK(noteMenu.getNumItems() == params::kEqLowMaxMidi - params::kEqLowMinMidi + 1);
    CHECK(suite::eq::midiFromNoteItem(8, 1, params::kEqLowMinMidi, params::kEqLowMaxMidi) == 28);
    CHECK_FALSE(suite::eq::midiFromNoteItem(0, 1, params::kEqLowMinMidi, params::kEqLowMaxMidi).has_value());
}
