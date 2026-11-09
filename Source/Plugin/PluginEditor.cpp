#include "PluginEditor.h"
#include "BinaryData.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include <ranges>

namespace {

namespace theme = openedmkick::theme;

constexpr int kEditorWidth = 1100;
constexpr int kEditorHeight = 972;
constexpr int kMargin = 12;
constexpr int kHeaderHeight = 44;
constexpr int kTopRowHeight = 370;
constexpr int kAutoRowHeight = 324;

} // namespace

OpenEdmKickEditor::OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      masterBypass_(processorToEdit.parameters, openedmkick::params::kMasterBypass, "BYPASS", theme::kAccentOut),
      synthPanel_(processorToEdit),
      muleStage_(processorToEdit),
      autoPanel_(processorToEdit),
      effectsPanel_(processorToEdit),
      tooltipWindow_(this, 700) {
    setLookAndFeel(&lookAndFeel_);
    faceplate_ = juce::ImageCache::getFromMemory(
        BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);

    const auto& presets = openedmkick::presets::factoryPresets();
    std::ranges::for_each(presets, [&, id = 1](const auto& preset) mutable {
        presetBox_.addItem(preset.name, id++);
    });
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kText);
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_.onChange = [this] {
        processor_.setCurrentProgram(presetBox_.getSelectedId() - 1);
    };

    prevPresetBtn_.setComponentID("prevPreset");
    prevPresetBtn_.setTooltip("Previous preset");
    prevPresetBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentMint);
    prevPresetBtn_.onClick = [this] {
        const int num = processor_.getNumPrograms();
        if (num <= 0) return;
        const int cur = processor_.getCurrentProgram();
        const int nextProg = (cur <= 0 ? num : cur) - 1;
        processor_.setCurrentProgram(nextProg);
        syncPresetBox();
    };

    nextPresetBtn_.setComponentID("nextPreset");
    nextPresetBtn_.setTooltip("Next preset");
    nextPresetBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentMint);
    nextPresetBtn_.onClick = [this] {
        const int num = processor_.getNumPrograms();
        if (num <= 0) return;
        const int cur = processor_.getCurrentProgram();
        const int nextProg = (cur + 1) % num;
        processor_.setCurrentProgram(nextProg);
        syncPresetBox();
    };

    std::ranges::for_each(std::initializer_list<juce::Component*>{
        &prevPresetBtn_, &presetBox_, &nextPresetBtn_, &masterBypass_, &synthPanel_, &muleStage_, &autoPanel_, &effectsPanel_
    }, [this](juce::Component* c) { addAndMakeVisible(c); });

    presetBox_.setTooltip("Factory kicks. Auto Mode, Lock In, and the effect chain are stored with each preset.");
    masterBypass_.setTooltip("Silence the instrument and clear the output.");
    processor_.addListener(this);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(4);
}

OpenEdmKickEditor::~OpenEdmKickEditor() {
    processor_.removeListener(this);
    setLookAndFeel(nullptr);
}

void OpenEdmKickEditor::timerCallback() {
    syncPresetBox();
}

void OpenEdmKickEditor::audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) {
    if (details.programChanged) {
        syncPresetBox();
    }
}

void OpenEdmKickEditor::audioProcessorParameterChanged(juce::AudioProcessor*, int, float) {
}

void OpenEdmKickEditor::syncPresetBox() {
    const int programId = processor_.getCurrentProgram() + 1;
    if (presetBox_.getSelectedId() != programId) {
        presetBox_.setSelectedId(programId, juce::dontSendNotification);
    }
}

void OpenEdmKickEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kBackground);
    g.setOpacity(0.76f);
    g.drawImage(faceplate_, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 5.0f, 1.0f);

    const auto header = getLocalBounds()
        .reduced(kMargin, 0)
        .removeFromTop(kHeaderHeight + kMargin)
        .withTrimmedTop(kMargin);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", header, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kAccentMint);
    g.drawText("EDM", header.withTrimmedLeft(openWidth), juce::Justification::centredLeft);
    const int edmWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "EDM");
    g.setColour(theme::kAccentGold);
    g.drawText("Kick", header.withTrimmedLeft(openWidth + edmWidth), juce::Justification::centredLeft);
    const int kickWidth = openWidth + edmWidth + juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "Kick");
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(theme::kTextDim.withAlpha(0.65f));
    g.drawText("ANALOG TRANSIENT SYNTHESIS", header.withTrimmedLeft(kickWidth + 24), juce::Justification::centredLeft);
}

void OpenEdmKickEditor::resized() {
    auto bounds = getLocalBounds().reduced(kMargin);
    auto header = bounds.removeFromTop(kHeaderHeight);
    nextPresetBtn_.setBounds(header.removeFromRight(26).withSizeKeepingCentre(26, 28));
    header.removeFromRight(4);
    presetBox_.setBounds(header.removeFromRight(200).withSizeKeepingCentre(200, 28));
    header.removeFromRight(4);
    prevPresetBtn_.setBounds(header.removeFromRight(26).withSizeKeepingCentre(26, 28));
    header.removeFromRight(8);
    masterBypass_.setBounds(header.removeFromRight(78));

    bounds.removeFromTop(4);
    auto topRow = bounds.removeFromTop(kTopRowHeight);
    synthPanel_.setBounds(topRow.removeFromLeft(topRow.getWidth() - 368 - kMargin));
    topRow.removeFromLeft(kMargin);
    muleStage_.setBounds(topRow);

    bounds.removeFromTop(10);
    autoPanel_.setBounds(bounds.removeFromTop(kAutoRowHeight));
    bounds.removeFromTop(10);
    effectsPanel_.setBounds(bounds);
}
