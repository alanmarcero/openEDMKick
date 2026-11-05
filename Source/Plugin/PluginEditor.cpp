#include "PluginEditor.h"
#include "BinaryData.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include <ranges>

namespace {

namespace theme = openedmkick::theme;

constexpr int kEditorWidth = 1100;
constexpr int kEditorHeight = 880;
constexpr int kMargin = 12;
constexpr int kHeaderHeight = 44;
constexpr int kTopRowHeight = 278;
constexpr int kAutoRowHeight = 324;

} // namespace

OpenEdmKickEditor::OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      masterBypass_(processorToEdit.parameters, openedmkick::params::kMasterBypass, "BYPASS", theme::kAccentOut),
      synthPanel_(processorToEdit),
      muleStage_(processorToEdit),
      autoPanel_(processorToEdit),
      effectsPanel_(processorToEdit.parameters) {
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

    std::ranges::for_each(std::initializer_list<juce::Component*>{
        &presetBox_, &masterBypass_, &synthPanel_, &muleStage_, &autoPanel_, &effectsPanel_
    }, [this](juce::Component* c) { addAndMakeVisible(c); });

    setSize(kEditorWidth, kEditorHeight);
}

OpenEdmKickEditor::~OpenEdmKickEditor() {
    setLookAndFeel(nullptr);
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
        .withTrimmedTop(kMargin)
        .withHeight(28);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", header, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kAccentMint);
    g.drawText("EDM", header.withTrimmedLeft(openWidth), juce::Justification::centredLeft);
    const int edmWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "EDM");
    g.setColour(theme::kAccentGold);
    g.drawText("Kick", header.withTrimmedLeft(openWidth + edmWidth), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(theme::kTextDim);
    g.drawText("12-Note Kick VSTi  /  Locked-In WAV Cache  /  A/B/C/D 1/64 Auto Builds  /  Beat-4 Synced Reverse & FX",
               header.translated(2, 27).withHeight(14).withWidth(680),
               juce::Justification::centredLeft);
}

void OpenEdmKickEditor::resized() {
    auto bounds = getLocalBounds().reduced(kMargin);
    auto header = bounds.removeFromTop(kHeaderHeight);
    presetBox_.setBounds(header.removeFromRight(260).withSizeKeepingCentre(260, 28));
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
