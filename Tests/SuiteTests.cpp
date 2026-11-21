#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "suite/PresetLibrary.h"
#include "suite/SuiteBar.h"
#include "suite/WavFile.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

namespace {
void setParam(OpenEdmKickProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float raw(OpenEdmKickProcessor& processor, const char* id) {
    return processor.parameters.getRawParameterValue(id)->load();
}

juce::Component* findId(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findId(*child, id)) return found;
    return nullptr;
}

float energy(const juce::AudioBuffer<float>& buffer) {
    double sum = 0.0;
    for (int index = 0; index < buffer.getNumSamples(); ++index)
        sum += std::abs(buffer.getSample(0, index));
    return static_cast<float>(sum);
}
} // namespace

using Catch::Matchers::WithinAbs;

TEST_CASE("Parameter gestures undo and redo as one step", "[suite][undo]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    auto* decay = processor.parameters.getParameter("decay_ms");
    REQUIRE(decay != nullptr);
    const float original = decay->getValue();
    decay->beginChangeGesture();
    decay->setValueNotifyingHost(decay->convertTo0to1(120.0f));
    decay->setValueNotifyingHost(decay->convertTo0to1(240.0f));
    decay->setValueNotifyingHost(decay->convertTo0to1(480.0f));
    decay->endChangeGesture();
    REQUIRE_THAT(raw(processor, "decay_ms"), WithinAbs(480.0f, 0.1f));
    REQUIRE(processor.canUndo());
    REQUIRE(processor.undo());
    REQUIRE_THAT(decay->getValue(), WithinAbs(original, 0.0001f));
    REQUIRE(processor.redo());
    REQUIRE_THAT(raw(processor, "decay_ms"), WithinAbs(480.0f, 0.1f));

    const float beforePreset = raw(processor, "body_tone");
    processor.setCurrentProgram(0);
    processor.setCurrentProgram(4);
    REQUIRE(std::abs(raw(processor, "body_tone") - beforePreset) > 0.05f);
    REQUIRE(processor.getCurrentProgram() == 4);
    REQUIRE(processor.undo());
    REQUIRE(processor.getCurrentProgram() == 0);
    REQUIRE_THAT(raw(processor, "body_tone"), WithinAbs(beforePreset, 0.0001f));
}

TEST_CASE("A and B keep two full sounds", "[suite][ab]") {
    OpenEdmKickProcessor processor;
    setParam(processor, "decay_ms", 420.0f);
    processor.copyAbSlot(true);
    setParam(processor, "decay_ms", 90.0f);
    processor.selectAbSlot(1);
    REQUIRE_THAT(raw(processor, "decay_ms"), WithinAbs(420.0f, 0.1f));
    processor.selectAbSlot(0);
    REQUIRE_THAT(raw(processor, "decay_ms"), WithinAbs(90.0f, 0.1f));
    processor.copyAbSlot(false);
    REQUIRE_THAT(raw(processor, "decay_ms"), WithinAbs(420.0f, 0.1f));
    setParam(processor, "decay_ms", 90.0f);

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    OpenEdmKickProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE_THAT(raw(restored, "decay_ms"), WithinAbs(90.0f, 0.1f));
    restored.selectAbSlot(1);
    REQUIRE_THAT(raw(restored, "decay_ms"), WithinAbs(420.0f, 0.1f));
}

TEST_CASE("Each compare slot keeps its own undo history", "[suite][ab][undo]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    auto* decay = processor.parameters.getParameter("decay_ms");
    const auto drag = [decay](float ms) {
        decay->beginChangeGesture();
        decay->setValueNotifyingHost(decay->convertTo0to1(ms));
        decay->endChangeGesture();
    };
    const float original = raw(processor, "decay_ms");
    drag(120.0f);
    processor.selectAbSlot(1);
    CHECK_FALSE(processor.canUndo());
    drag(900.0f);
    REQUIRE(processor.undo());
    CHECK_THAT(raw(processor, "decay_ms"), WithinAbs(original, 0.1f));
    processor.selectAbSlot(0);
    CHECK_THAT(raw(processor, "decay_ms"), WithinAbs(120.0f, 0.1f));
    REQUIRE(processor.undo());
    CHECK_THAT(raw(processor, "decay_ms"), WithinAbs(original, 0.1f));
    processor.copyAbSlot(true);
    processor.selectAbSlot(1);
    CHECK_FALSE(processor.canUndo());
    CHECK_FALSE(processor.canRedo());
}

TEST_CASE("User presets save, search, star, rename, and delete", "[suite][presets]") {
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("openkicker-preset-test-" + juce::String(juce::Time::currentTimeMillis()));
    directory.deleteRecursively();
    openedmkick::suite::PresetLibrary library("openKicker", { "Uplifting / First Light", "Rolling / Pocket" });
    library.setDirectory(directory);
    juce::MemoryBlock state("kick", 4);
    REQUIRE_FALSE(library.save("Uplifting / First Light", state));
    REQUIRE_FALSE(library.remove("Rolling / Pocket"));
    REQUIRE(library.save("My Room", state));
    REQUIRE(directory.getChildFile("My Room.oekpreset").existsAsFile());
    const juce::MemoryBlock replacement("deep", 4);
    REQUIRE(library.save("My Room", replacement));
    juce::MemoryBlock resaved;
    REQUIRE(library.load("My Room", resaved));
    REQUIRE(resaved == replacement);
    auto found = library.list("room");
    REQUIRE(found.size() == 1);
    REQUIRE(found.front().name == "My Room");
    REQUIRE_FALSE(found.front().factory);
    REQUIRE(library.list("zz").empty());
    REQUIRE(library.list("pocket").size() == 1);
    REQUIRE(library.list("pocket").front().factory);
    REQUIRE(library.setFavorite("My Room", true));
    REQUIRE(library.setFavorite("Rolling / Pocket", true));
    openedmkick::suite::PresetLibrary reopened("openKicker", { "Uplifting / First Light", "Rolling / Pocket" });
    reopened.setDirectory(directory);
    const auto starred = reopened.list({});
    REQUIRE(starred.size() >= 2);
    REQUIRE(starred[0].favorite);
    REQUIRE(starred[1].favorite);
    REQUIRE(library.rename("My Room", "Late Room"));
    REQUIRE_FALSE(directory.getChildFile("My Room.oekpreset").existsAsFile());
    REQUIRE(directory.getChildFile("Late Room.oekpreset").existsAsFile());
    juce::MemoryBlock loaded;
    REQUIRE(library.load("Late Room", loaded));
    REQUIRE(loaded.getSize() == 4);
    REQUIRE(library.remove("Late Room"));
    REQUIRE_FALSE(library.load("Late Room", loaded));
    directory.deleteRecursively();
}

TEST_CASE("Sample click stays off for old sessions and round-trips in new ones", "[suite][click]") {
    OpenEdmKickProcessor dry;
    dry.prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> left(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    dry.processBlock(left, midi);
    REQUIRE_THAT(raw(dry, "click_sample_on"), WithinAbs(0.0f, 0.001f));

    OpenEdmKickProcessor enabled;
    enabled.prepareToPlay(48000.0, 512);
    setParam(enabled, "click_sample_on", 1.0f);
    setParam(enabled, "click_sample_level", 1.0f);
    juce::AudioBuffer<float> right(2, 512);
    juce::MidiBuffer again;
    again.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
    enabled.processBlock(right, again);
    REQUIRE(std::abs(energy(left) - energy(right)) > 0.05f);

    juce::AudioBuffer<float> user(1, 180);
    for (int index = 0; index < user.getNumSamples(); ++index)
        user.setSample(0, index, std::sin(0.7f * static_cast<float>(index)));
    REQUIRE(enabled.loadUserClickSample(user, 44100.0, "ramp"));
    REQUIRE_THAT(raw(enabled, "click_sample_source"), WithinAbs(4.0f, 0.001f));
    juce::MemoryBlock saved;
    enabled.getStateInformation(saved);
    const auto play = [](const juce::MemoryBlock& state) {
        OpenEdmKickProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        restored.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> played(2, 512);
        juce::MidiBuffer notes;
        notes.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
        restored.processBlock(played, notes);
        return played;
    };
    const auto withSample = play(saved);
    OpenEdmKickProcessor silentClick;
    setParam(silentClick, "click_sample_on", 0.0f);
    juce::MemoryBlock without;
    silentClick.getStateInformation(without);
    const auto noSample = play(without);
    REQUIRE(std::abs(energy(withSample) - energy(noSample)) > 0.2f);

    auto legacy = dry.parameters.copyState();
    for (int index = legacy.getNumChildren() - 1; index >= 0; --index)
        if (legacy.getChild(index).getProperty("id").toString().startsWith("click_sample"))
            legacy.removeChild(index, nullptr);
    juce::MemoryBlock oldSession;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), oldSession);
    OpenEdmKickProcessor recalled;
    recalled.setStateInformation(oldSession.getData(), static_cast<int>(oldSession.getSize()));
    REQUIRE_THAT(raw(recalled, "click_sample_on"), WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Offline hit export follows the last note, layers, and tail", "[suite][drag]") {
    OpenEdmKickProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> live(2, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, 0.4f), 0);
    processor.processBlock(live, midi);
    REQUIRE(processor.currentMidiNote() == 69);
    REQUIRE_THAT(processor.lastVelocity(), WithinAbs(0.4f, 0.01f));

    juce::AudioBuffer<float> high;
    REQUIRE(processor.renderCurrentHit(high));
    setParam(processor, "decay_ms", 40.0f);
    juce::AudioBuffer<float> shortHit;
    REQUIRE(processor.renderCurrentHit(shortHit));
    setParam(processor, "decay_ms", 900.0f);
    juce::AudioBuffer<float> longHit;
    REQUIRE(processor.renderCurrentHit(longHit));
    REQUIRE(longHit.getNumSamples() > shortHit.getNumSamples());

    juce::MidiBuffer other;
    other.addEvent(juce::MidiMessage::noteOn(1, 40, 1.0f), 0);
    processor.processBlock(live, other);
    juce::AudioBuffer<float> low;
    REQUIRE(processor.renderCurrentHit(low));
    REQUIRE(low.getNumSamples() > 64);
    double difference = 0.0;
    const int count = std::min(high.getNumSamples(), low.getNumSamples());
    for (int index = 0; index < count; ++index)
        difference += std::abs(high.getSample(0, index) - low.getSample(0, index));
    REQUIRE(difference > 1.0);

    const auto plain = energy(low);
    setParam(processor, "layer_b_enabled", 1.0f);
    setParam(processor, "layer_b_sound", 1.0f);
    juce::AudioBuffer<float> layered;
    REQUIRE(processor.renderCurrentHit(layered));
    REQUIRE(std::abs(energy(layered) - plain) > 0.5f);

    const auto file = processor.renderHitToTemporaryWav();
    REQUIRE(file.existsAsFile());
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    REQUIRE(reader != nullptr);
    REQUIRE_THAT(reader->sampleRate, WithinAbs(44100.0, 0.1));
    REQUIRE(reader->numChannels == 2);
    REQUIRE(reader->bitsPerSample == 24);
    REQUIRE(reader->lengthInSamples > 64);
    file.deleteFile();
    if (const auto demo = juce::SystemStats::getEnvironmentVariable("OEK_DEMO_WAV", {}); demo.isNotEmpty()) {
        processor.setCurrentProgram(0);
        setParam(processor, "click_sample_on", 1.0f);
        setParam(processor, "click_sample_level", 0.85f);
        juce::AudioBuffer<float> scratch(2, 64);
        juce::MidiBuffer demoMidi;
        demoMidi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
        processor.processBlock(scratch, demoMidi);
        juce::AudioBuffer<float> demoHit;
        REQUIRE(processor.renderCurrentHit(demoHit));
        REQUIRE(openedmkick::suite::writeStereoWav(juce::File(demo), demoHit, 44100.0, 16));
    }
}

TEST_CASE("Editor exposes undo, compare, drag, and preset search", "[suite][ui]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenEdmKickProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(findId(*editor, "undo") != nullptr);
    REQUIRE(findId(*editor, "redo") != nullptr);
    REQUIRE(findId(*editor, "abA") != nullptr);
    REQUIRE(findId(*editor, "abB") != nullptr);
    REQUIRE(findId(*editor, "copyAtoB") != nullptr);
    REQUIRE(findId(*editor, "copyBtoA") != nullptr);
    REQUIRE(findId(*editor, "dragHit") != nullptr);
    auto* presets = dynamic_cast<juce::TextButton*>(findId(*editor, "presets"));
    REQUIRE(presets != nullptr);
    presets->onClick();
    auto* search = dynamic_cast<juce::TextEditor*>(findId(*editor, "presetSearch"));
    REQUIRE(search != nullptr);
    REQUIRE(search->isVisible());
    auto* browser = dynamic_cast<openedmkick::suite::PresetBrowser*>(findId(*editor, "presetBrowser"));
    REQUIRE(browser != nullptr);
    const int all = browser->rowCount();
    REQUIRE(all >= 56);
    search->setText("pocket");
    search->onTextChange();
    REQUIRE(browser->rowCount() >= 1);
    REQUIRE(browser->rowCount() < all);
    REQUIRE(browser->rowName(0).containsIgnoreCase("pocket"));

    auto* decay = processor.parameters.getParameter("decay_ms");
    const float original = decay->getValue();
    decay->beginChangeGesture();
    decay->setValueNotifyingHost(decay->convertTo0to1(500.0f));
    decay->endChangeGesture();
    REQUIRE(editor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)));
    REQUIRE_THAT(decay->getValue(), WithinAbs(original, 0.0001f));
    REQUIRE(editor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE_THAT(raw(processor, "decay_ms"), WithinAbs(500.0f, 0.1f));

    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("openkicker-browser-" + juce::String(juce::Time::currentTimeMillis()));
    browser->setDirectory(directory);
    auto* name = dynamic_cast<juce::TextEditor*>(findId(*editor, "presetName"));
    auto* save = dynamic_cast<juce::TextButton*>(findId(*editor, "presetSave"));
    REQUIRE(name != nullptr);
    REQUIRE(save != nullptr);
    name->setText("Night Kick");
    save->onClick();
    const auto saved = directory.getChildFile("Night Kick.oekpreset");
    REQUIRE(saved.existsAsFile());
    const auto savedDecay = [&saved] {
        juce::MemoryBlock state;
        REQUIRE(saved.loadFileAsData(state));
        OpenEdmKickProcessor reloaded;
        REQUIRE(reloaded.applySoundState(state, "Night Kick"));
        return raw(reloaded, "decay_ms");
    };
    setParam(processor, "decay_ms", 900.0f);
    save->onClick();
    CHECK(save->getButtonText() == "Replace?");
    CHECK_THAT(savedDecay(), WithinAbs(500.0f, 0.1f));
    save->onClick();
    CHECK(save->getButtonText() == "Save");
    CHECK_THAT(savedDecay(), WithinAbs(900.0f, 0.1f));
    search->setText("night");
    search->onTextChange();
    REQUIRE(browser->rowName(0) == "Night Kick");
    auto* list = dynamic_cast<juce::ListBox*>(findId(*browser, "presetList"));
    auto* remove = dynamic_cast<juce::TextButton*>(findId(*editor, "presetDelete"));
    REQUIRE(list != nullptr);
    REQUIRE(remove != nullptr);
    list->selectRow(0);
    remove->onClick();
    CHECK(remove->getButtonText() == "Delete?");
    REQUIRE(saved.existsAsFile());
    remove->onClick();
    REQUIRE_FALSE(saved.existsAsFile());
    directory.deleteRecursively();
}
