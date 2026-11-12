#include "PluginEditor.h"

#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"

#include <ranges>

namespace {

namespace theme = openedmkick::theme;
namespace params = openedmkick::params;
namespace presets = openedmkick::presets;

constexpr int kEditorWidth = 1100;
constexpr int kEditorHeight = 972;

void styleTab(juce::TextButton& button, const juce::String& id) {
    button.setComponentID(id);
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonOnColourId, theme::kCyan);
    button.setTooltip("Open this workspace page.");
}

} // namespace

OpenEdmKickEditor::OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      synthPanel_(processorToEdit),
      muleStage_(processorToEdit),
      autoPanel_(processorToEdit),
      effectsPanel_(processorToEdit),
      tooltipWindow_(this, 650) {
    setLookAndFeel(&lookAndFeel_);
    setResizable(false, false);

    const auto& factory = presets::factoryPresets();
    for (int i = 0; i < static_cast<int>(factory.size()); ++i)
        presetBox_.addItem(factory[static_cast<size_t>(i)].name, i + 1);
    presetBox_.setComponentID("factoryPreset");
    presetBox_.setTextWhenNothingSelected("CUSTOM SOUND");
    presetBox_.setTooltip("48 new trance kicks. Locked sections keep their current settings while you browse.");
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kCyan);
    presetBox_.onChange = [this] {
        const int selected = presetBox_.getSelectedId() - 1;
        if (selected >= 0 && selected < processor_.getNumPrograms()) processor_.setCurrentProgram(selected);
        syncPresetBox();
    };

    prevPresetBtn_.setComponentID("prevPreset");
    nextPresetBtn_.setComponentID("nextPreset");
    prevPresetBtn_.setTooltip("Previous factory kick.");
    nextPresetBtn_.setTooltip("Next factory kick.");
    prevPresetBtn_.onClick = [this] {
        const int count = processor_.getNumPrograms();
        if (count <= 0) return;
        const int current = processor_.getCurrentProgram();
        processor_.setCurrentProgram((current + count - 1) % count);
        syncPresetBox();
    };
    nextPresetBtn_.onClick = [this] {
        const int count = processor_.getNumPrograms();
        if (count <= 0) return;
        processor_.setCurrentProgram((processor_.getCurrentProgram() + 1) % count);
        syncPresetBox();
    };

    viewToggleBtn_.setComponentID("viewToggle");
    viewToggleBtn_.setTooltip("Toggle between the essential controls and the complete sound design controls.");
    viewToggleBtn_.onClick = [this] { setAdvancedView(!isAdvancedView()); };
    styleTab(kickTab_, "page_kick");
    styleTab(patternTab_, "page_pattern");
    styleTab(effectsTab_, "page_fx");
    kickTab_.onClick = [this] { setPage(Page::kick); };
    patternTab_.onClick = [this] { setPage(Page::pattern); };
    effectsTab_.onClick = [this] { setPage(Page::effects); };

    for (juce::Component* component : std::array<juce::Component*, 11>{
             &presetBox_, &prevPresetBtn_, &nextPresetBtn_, &viewToggleBtn_, &kickTab_, &patternTab_, &effectsTab_,
             &synthPanel_, &muleStage_, &autoPanel_, &effectsPanel_}) addAndMakeVisible(component);

    processor_.addListener(this);
    setPage(Page::kick);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(5);
    syncPresetBox();
}

OpenEdmKickEditor::~OpenEdmKickEditor() {
    processor_.removeListener(this);
    setLookAndFeel(nullptr);
}

void OpenEdmKickEditor::timerCallback() { syncPresetBox(); }

void OpenEdmKickEditor::audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) {
    if (details.programChanged) syncPresetBox();
}

void OpenEdmKickEditor::audioProcessorParameterChanged(juce::AudioProcessor*, int, float) {}

void OpenEdmKickEditor::syncPresetBox() {
    if (processor_.hasRestoredCustomState()) {
        if (presetBox_.getSelectedId() != 0) presetBox_.setSelectedId(0, juce::dontSendNotification);
        return;
    }
    const int id = processor_.getCurrentProgram() + 1;
    if (presetBox_.getSelectedId() != id) presetBox_.setSelectedId(id, juce::dontSendNotification);
}

void OpenEdmKickEditor::setAdvancedView(bool advanced) {
    synthPanel_.setAdvancedMode(advanced);
    autoPanel_.setAdvancedMode(advanced);
    effectsPanel_.setAdvancedMode(advanced);
    viewToggleBtn_.setButtonText(advanced ? "FULL CONTROLS" : "SIMPLE VIEW");
    viewToggleBtn_.setColour(juce::TextButton::buttonOnColourId, advanced ? theme::kAmber : theme::kCyan);
    resized();
    repaint();
}

void OpenEdmKickEditor::setPage(Page page) {
    page_ = page;
    synthPanel_.setVisible(page == Page::kick);
    muleStage_.setVisible(page == Page::kick);
    autoPanel_.setVisible(page == Page::pattern);
    effectsPanel_.setVisible(page == Page::effects);
    kickTab_.setToggleState(page == Page::kick, juce::dontSendNotification);
    patternTab_.setToggleState(page == Page::pattern, juce::dontSendNotification);
    effectsTab_.setToggleState(page == Page::effects, juce::dontSendNotification);
    resized(); repaint();
}

void OpenEdmKickEditor::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(theme::kInk.brighter(0.035f), 0.0f, 0.0f,
                                           theme::kInk.darker(0.08f), 0.0f, static_cast<float>(getHeight()), false));
    g.fillAll();
    g.setColour(theme::kCyan.withAlpha(0.08f));
    g.fillRoundedRectangle(juce::Rectangle<float>(getWidth() - 210.0f, -54.0f, 250.0f, 190.0f), 34.0f);
    g.setColour(theme::kEdge.withAlpha(0.62f));
    g.drawLine(18.0f, 60.0f, static_cast<float>(getWidth() - 18), 60.0f, 1.0f);
    g.setColour(theme::kCyan.withAlpha(0.35f));
    g.fillRoundedRectangle(18.0f, 60.0f, 110.0f, 2.0f, 1.0f);

    g.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", 20, 8, 58, 32, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kCyan);
    g.drawText("Kicker", 20 + openWidth, 8, 90, 32, juce::Justification::centredLeft);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("TRANCE KICK DESIGN SYSTEM", 22, 39, 230, 14, juce::Justification::centredLeft);

    const auto status = processor_.isKickLockedIn() ? "WAV FROZEN" : "SYNTH LIVE";
    g.setColour(processor_.isKickLockedIn() ? theme::kAmber : theme::kCyan);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(status, 254, 17, 95, 20, juce::Justification::centredLeft);
}

void OpenEdmKickEditor::resized() {
    auto bounds = getLocalBounds().reduced(16, 10);
    auto header = bounds.removeFromTop(46);
    viewToggleBtn_.setBounds(header.removeFromRight(112).withHeight(30).withY(header.getY() + 6));
    header.removeFromRight(14);
    nextPresetBtn_.setBounds(header.removeFromRight(34).withHeight(30).withY(header.getY() + 6));
    header.removeFromRight(4);
    presetBox_.setBounds(header.removeFromRight(245).withHeight(30).withY(header.getY() + 6));
    header.removeFromRight(4);
    prevPresetBtn_.setBounds(header.removeFromRight(34).withHeight(30).withY(header.getY() + 6));

    bounds.removeFromTop(8);
    auto tabs = bounds.removeFromTop(38);
    kickTab_.setBounds(tabs.removeFromLeft(132).withHeight(32)); tabs.removeFromLeft(6);
    patternTab_.setBounds(tabs.removeFromLeft(132).withHeight(32)); tabs.removeFromLeft(6);
    effectsTab_.setBounds(tabs.removeFromLeft(92).withHeight(32));
    bounds.removeFromTop(10);

    if (page_ == Page::kick) {
        auto kickArea = bounds;
        const int stageWidth = std::clamp(kickArea.getWidth() / 4, 250, 290);
        muleStage_.setBounds(kickArea.removeFromRight(stageWidth));
        kickArea.removeFromRight(10);
        synthPanel_.setBounds(kickArea);
    } else if (page_ == Page::pattern) {
        autoPanel_.setBounds(bounds);
    } else {
        effectsPanel_.setBounds(bounds);
    }
}
