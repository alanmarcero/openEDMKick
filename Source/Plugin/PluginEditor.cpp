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
constexpr int kEditorHeight = 940;

void styleTab(juce::TextButton& button, const juce::String& id) {
    button.setComponentID(id);
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
    button.setTooltip("Open this workspace page.");
}

} // namespace

OpenEdmKickEditor::OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      synthPanel_(processorToEdit),
      layerPanel_(processorToEdit),
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
    presetBox_.setTooltip("48 tuned, impact-focused trance kick starting points.");
    presetBox_.setColour(juce::ComboBox::outlineColourId, theme::kViolet);
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
    viewToggleBtn_.setTooltip("Show the essential controls or every sound design control.");
    viewToggleBtn_.onClick = [this] { setAdvancedView(!isAdvancedView()); };
    styleTab(kickTab_, "page_kick");
    styleTab(layersTab_, "page_layers");
    styleTab(patternTab_, "page_pattern");
    styleTab(effectsTab_, "page_fx");
    kickTab_.onClick = [this] { setPage(Page::kick); };
    layersTab_.onClick = [this] { setPage(Page::layers); };
    patternTab_.onClick = [this] { setPage(Page::pattern); };
    effectsTab_.onClick = [this] { setPage(Page::effects); };

    std::ranges::for_each(std::array<juce::Component*, 12>{
        &presetBox_, &prevPresetBtn_, &nextPresetBtn_, &viewToggleBtn_, &kickTab_, &layersTab_,
        &patternTab_, &effectsTab_, &synthPanel_, &layerPanel_, &autoPanel_, &effectsPanel_},
        [this](juce::Component* component) { addAndMakeVisible(component); });

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
    viewToggleBtn_.setButtonText(advanced ? "SIMPLE VIEW" : "ADVANCED");
    viewToggleBtn_.setColour(juce::TextButton::buttonOnColourId, advanced ? theme::kBrass : theme::kViolet);
    resized();
    repaint();
}

void OpenEdmKickEditor::setPage(Page page) {
    page_ = page;
    synthPanel_.setVisible(page == Page::kick);
    layerPanel_.setVisible(page == Page::layers);
    autoPanel_.setVisible(page == Page::pattern);
    effectsPanel_.setVisible(page == Page::effects);
    kickTab_.setToggleState(page == Page::kick, juce::dontSendNotification);
    layersTab_.setToggleState(page == Page::layers, juce::dontSendNotification);
    patternTab_.setToggleState(page == Page::pattern, juce::dontSendNotification);
    effectsTab_.setToggleState(page == Page::effects, juce::dontSendNotification);
    resized(); repaint();
}

void OpenEdmKickEditor::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(theme::kInk.brighter(0.035f), 0.0f, 0.0f,
                                           theme::kInk.darker(0.08f), 0.0f, static_cast<float>(getHeight()), false));
    g.fillAll();
    g.setColour(theme::kViolet.withAlpha(0.10f));
    g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(getWidth()) - 260.0f, -70.0f, 300.0f, 210.0f), 42.0f);
    g.setColour(theme::kCoral.withAlpha(0.045f));
    g.fillEllipse(juce::Rectangle<float>(static_cast<float>(getWidth()) - 150.0f, 0.0f, 105.0f, 105.0f));
    g.setColour(theme::kEdge.withAlpha(0.62f));
    g.drawLine(18.0f, 60.0f, static_cast<float>(getWidth() - 18), 60.0f, 1.0f);
    g.setColour(theme::kViolet.withAlpha(0.42f));
    g.fillRoundedRectangle(18.0f, 60.0f, 130.0f, 2.0f, 1.0f);

    g.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText("open", 20, 8, 58, 32, juce::Justification::centredLeft);
    const int openWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), "open");
    g.setColour(theme::kViolet);
    g.drawText("Kicker", 20 + openWidth, 8, 90, 32, juce::Justification::centredLeft);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("TRANCE KICK SYNTHESIS  /  MIDI TRACKING", 22, 39, 300, 14, juce::Justification::centredLeft);
    g.setColour(theme::kBrass);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("V3.1  ·  FOUR LAYERS, ONE HIT", 330, 17, 220, 20, juce::Justification::centredLeft);
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
    layersTab_.setBounds(tabs.removeFromLeft(110).withHeight(32)); tabs.removeFromLeft(6);
    patternTab_.setBounds(tabs.removeFromLeft(132).withHeight(32)); tabs.removeFromLeft(6);
    effectsTab_.setBounds(tabs.removeFromLeft(92).withHeight(32));
    bounds.removeFromTop(10);

    synthPanel_.setBounds(bounds);
    layerPanel_.setBounds(bounds);
    autoPanel_.setBounds(bounds);
    effectsPanel_.setBounds(bounds);
}
