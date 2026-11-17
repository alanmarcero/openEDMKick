#include "PluginEditor.h"

#include "Presets.h"
#include "Theme.h"

#include <ranges>

namespace {

namespace theme = openedmkick::theme;
namespace presets = openedmkick::presets;

constexpr int kEditorWidth = 1080;
constexpr int kEditorHeight = 716;

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
    setResizable(true, true);
    setResizeLimits(900, 570, 1600, 1280);
    workspace_.setScrollBarsShown(true, false);
    workspace_.setScrollBarThickness(10);
    workspace_.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible(workspace_);
    auditionBtn_.setComponentID("audition");
    auditionBtn_.setTooltip("Play a one-shot at C2 with the current layers and effects.");
    auditionBtn_.setColour(juce::TextButton::buttonColourId, theme::kMint);
    auditionBtn_.setColour(juce::TextButton::textColourOffId, theme::kInk);
    auditionBtn_.onClick = [this] { processor_.triggerAudition(); };
    addAndMakeVisible(auditionBtn_);

    const auto& factory = presets::factoryPresets();
    juce::String family;
    juce::PopupMenu sounds;
    for (size_t i = 0; i < factory.size(); ++i) {
        const juce::String name(factory[i].name);
        const auto nextFamily = name.upToFirstOccurrenceOf(" / ", false, false);
        if (family != nextFamily && sounds.getNumItems() > 0) {
            presetBox_.getRootMenu()->addSubMenu(family, sounds);
            sounds.clear();
        }
        family = nextFamily;
        sounds.addItem(static_cast<int>(i + 1), name);
    }
    presetBox_.getRootMenu()->addSubMenu(family, sounds);
    presetBox_.setComponentID("factoryPreset");
    presetBox_.setName("Factory preset");
    presetBox_.setTextWhenNothingSelected("Custom sound");
    presetBox_.setTooltip("Choose a factory kick. Locked controls keep their current values.");
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
        const int current = processor_.hasRestoredCustomState() ? 0 : processor_.getCurrentProgram();
        processor_.setCurrentProgram((current + count - 1) % count);
        syncPresetBox();
    };
    nextPresetBtn_.onClick = [this] {
        const int count = processor_.getNumPrograms();
        if (count <= 0) return;
        const int next = processor_.hasRestoredCustomState() ? 0 : (processor_.getCurrentProgram() + 1) % count;
        processor_.setCurrentProgram(next);
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

    std::ranges::for_each(std::array<juce::Component*, 8>{
        &presetBox_, &prevPresetBtn_, &nextPresetBtn_, &viewToggleBtn_, &kickTab_, &layersTab_,
        &patternTab_, &effectsTab_},
        [this](juce::Component* component) { addAndMakeVisible(component); });

    viewToggleBtn_.setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
    setPage(Page::kick);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(5);
    syncPresetBox();
}

OpenEdmKickEditor::~OpenEdmKickEditor() {
    workspace_.setViewedComponent(nullptr);
    setLookAndFeel(nullptr);
}

void OpenEdmKickEditor::timerCallback() { syncPresetBox(); }

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
    viewToggleBtn_.setToggleState(advanced, juce::dontSendNotification);
    workspace_.setViewPosition(0, 0);
    resized();
    repaint();
}

void OpenEdmKickEditor::setPage(Page page) {
    page_ = page;
    juce::Component* panels[]{ &synthPanel_, &layerPanel_, &autoPanel_, &effectsPanel_ };
    workspace_.setViewedComponent(panels[static_cast<int>(page)], false);
    workspace_.setViewPosition(0, 0);
    viewToggleBtn_.setVisible(page != Page::layers);
    kickTab_.setToggleState(page == Page::kick, juce::dontSendNotification);
    layersTab_.setToggleState(page == Page::layers, juce::dontSendNotification);
    patternTab_.setToggleState(page == Page::pattern, juce::dontSendNotification);
    effectsTab_.setToggleState(page == Page::effects, juce::dontSendNotification);
    resized(); repaint();
}

void OpenEdmKickEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::kInk);
    g.setColour(theme::kSteel);
    g.fillRect(0, 0, getWidth(), 72);
    g.setColour(theme::kViolet);
    g.fillRoundedRectangle(20, 16, 40, 40, 11);
    g.setColour(theme::kInk);
    juce::Path mark;
    mark.startNewSubPath(26, 39);
    mark.lineTo(30, 39);
    mark.lineTo(34, 26);
    mark.lineTo(39, 46);
    mark.lineTo(44, 31);
    mark.lineTo(48, 39);
    mark.lineTo(54, 39);
    g.strokePath(mark, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("openKicker", 72, 12, 180, 30, juce::Justification::centredLeft);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Tuned drum synthesizer", 73, 40, 175, 18, juce::Justification::centredLeft);
    auto presetLabel = presetBox_.getBounds().translated(3, -19).withHeight(18);
    g.drawText("Factory preset", presetLabel, juce::Justification::centredLeft);
}

void OpenEdmKickEditor::resized() {
    auto bounds = getLocalBounds().reduced(20, 0);
    auto header = bounds.removeFromTop(72).reduced(0, 9);
    auditionBtn_.setBounds(header.removeFromRight(122).withSizeKeepingCentre(122, 40));
    header.removeFromRight(20);
    nextPresetBtn_.setBounds(header.removeFromRight(66).removeFromBottom(36));
    header.removeFromRight(5);
    prevPresetBtn_.setBounds(header.removeFromRight(84).removeFromBottom(36));
    header.removeFromRight(6);
    presetBox_.setBounds(header.removeFromRight(std::min(310, header.getWidth() - 242)).removeFromBottom(36));

    auto nav = bounds.removeFromTop(60).reduced(0, 10);
    for (auto* tab : { &kickTab_, &layersTab_, &patternTab_, &effectsTab_ }) {
        tab->setBounds(nav.removeFromLeft(90));
        nav.removeFromLeft(4);
    }
    viewToggleBtn_.setBounds(nav.removeFromRight(164));
    bounds.removeFromBottom(16);
    workspace_.setBounds(bounds);
    auto* panel = workspace_.getViewedComponent();
    if (panel == nullptr) return;
    int contentHeight = 560;
    if (page_ == Page::kick && isAdvancedView()) contentHeight = 920;
    if (page_ == Page::effects) contentHeight = isAdvancedView() ? 995 : 560;
    const bool scroll = bounds.getHeight() < contentHeight;
    panel->setSize(bounds.getWidth() - (scroll ? workspace_.getScrollBarThickness() : 0),
                   std::max(bounds.getHeight(), contentHeight));
}
