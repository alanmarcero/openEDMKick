#include "PluginEditor.h"

#include "BinaryData.h"
#include "Presets.h"
#include "Theme.h"

#include <cmath>
#include <ranges>
#include <vector>

namespace {

namespace theme = openedmkick::theme;
namespace presets = openedmkick::presets;

constexpr int kEditorWidth = 1080;
constexpr int kEditorHeight = 764;

std::vector<juce::String> factoryPresetNames() {
    std::vector<juce::String> names;
    for (const auto& preset : presets::factoryPresets()) names.emplace_back(preset.name);
    return names;
}

void styleTab(juce::TextButton& button, const juce::String& id) {
    button.setComponentID(id);
    button.setTitle(button.getButtonText());
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
    button.setTooltip("Open the " + button.getButtonText() + " workspace.");
}

} // namespace

OpenEdmKickEditor::OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit)
    : AudioProcessorEditor(processorToEdit),
      processor_(processorToEdit),
      presetBrowser_(factoryPresetNames()),
      synthPanel_(processorToEdit),
      layerPanel_(processorToEdit),
      autoPanel_(processorToEdit),
      effectsPanel_(processorToEdit),
      tooltipWindow_(this, 650) {
    kickArtwork_ = juce::Drawable::createFromImageData(BinaryData::kickdrum_svg, BinaryData::kickdrum_svgSize);
    setLookAndFeel(&lookAndFeel_);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(900, 570, 1600, 1280);
    workspace_.setScrollBarsShown(true, false);
    workspace_.setScrollBarThickness(10);
    workspace_.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible(workspace_);
    auditionBtn_.setComponentID("audition");
    auditionBtn_.setTitle("Audition C2");
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
    presetBox_.setName("Preset");
    presetBox_.setTitle("Preset");
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
    prevPresetBtn_.setTitle("Previous preset");
    nextPresetBtn_.setTitle("Next preset");
    prevPresetBtn_.setTooltip("Previous preset");
    nextPresetBtn_.setTooltip("Next preset");
    prevPresetBtn_.onClick = [this] { presetBrowser_.step(processor_.presetName(), -1); };
    nextPresetBtn_.onClick = [this] { presetBrowser_.step(processor_.presetName(), 1); };

    viewToggleBtn_.setComponentID("viewToggle");
    viewToggleBtn_.setTitle("Advanced controls");
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
    suiteBar_.bind({
        [this] { processor_.undo(); suiteBar_.refresh(); },
        [this] { processor_.redo(); suiteBar_.refresh(); },
        [this] { processor_.selectAbSlot(0); suiteBar_.refresh(); syncPresetBox(); },
        [this] { processor_.selectAbSlot(1); suiteBar_.refresh(); syncPresetBox(); },
        [this] { processor_.copyAbSlot(true); suiteBar_.refresh(); syncPresetBox(); },
        [this] { processor_.copyAbSlot(false); suiteBar_.refresh(); syncPresetBox(); },
        [this] { return processor_.canUndo(); },
        [this] { return processor_.canRedo(); },
        [this] { return processor_.abSlot(); },
        [this] { return processor_.renderHitToTemporaryWav(); },
        [this] {
            presetBrowser_.setVisible(!presetBrowser_.isVisible());
            if (presetBrowser_.isVisible()) presetBrowser_.toFront(false);
            resized();
        }
    });
    presetBrowser_.captureState = [this] { return processor_.captureSoundState(); };
    presetBrowser_.loadFactory = [this](int index) {
        processor_.setCurrentProgram(index);
        syncPresetBox();
    };
    presetBrowser_.loadUser = [this](const juce::MemoryBlock& state, const juce::String& name) {
        processor_.applySoundState(state, name);
        syncPresetBox();
    };
    presetBrowser_.onSave = [this](const juce::String& name) {
        processor_.setUserPreset(name);
        syncPresetBox();
    };
    addAndMakeVisible(suiteBar_);
    addChildComponent(presetBrowser_);
    setPage(Page::kick);
    setSize(kEditorWidth, kEditorHeight);
    startTimerHz(30);
    syncPresetBox();
}

OpenEdmKickEditor::~OpenEdmKickEditor() {
    workspace_.setViewedComponent(nullptr);
    setLookAndFeel(nullptr);
}

void OpenEdmKickEditor::timerCallback() {
    syncPresetBox();
    suiteBar_.refresh();
    if (std::abs(processor_.displayBeatPhase.load(std::memory_order_relaxed) - lastDrawnBeat_) > 0.0001f)
        repaint(12, 6, 58, 60);
}

void OpenEdmKickEditor::syncPresetBox() {
    const auto shown = processor_.presetDisplayName();
    if (presetBox_.getText() != shown) presetBox_.setText(shown, juce::dontSendNotification);
}

bool OpenEdmKickEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && presetBrowser_.isVisible()) {
        presetBrowser_.setVisible(false);
        resized();
        return true;
    }
    const int code = key.getKeyCode();
    if (key.getModifiers().isCommandDown() && (code == 'z' || code == 'Z')) {
        if (key.getModifiers().isShiftDown()) processor_.redo();
        else processor_.undo();
        suiteBar_.refresh();
        syncPresetBox();
        return true;
    }
    return false;
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
    if (presetBrowser_.isVisible()) presetBrowser_.setVisible(false);
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
    lastDrawnBeat_ = processor_.displayBeatPhase.load(std::memory_order_relaxed);
    if (kickArtwork_ != nullptr) {
        const float pulse = std::exp(-5.0f * lastDrawnBeat_);
        const float size = 44.0f + 12.0f * pulse;
        const auto drum = juce::Rectangle<float>(size, size).withCentre({41.0f, 36.0f});
        kickArtwork_->drawWithin(g, drum, juce::RectanglePlacement::centred, 0.83f + 0.17f * pulse);
    }
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("openKicker", 72, 12, 146, 30, juce::Justification::centredLeft);
    g.setColour(theme::kText.withAlpha(0.75f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("v" JucePlugin_VersionString, 214, 17, 58, 24, juce::Justification::centredLeft);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Tuned drum synthesizer", 73, 40, 175, 18, juce::Justification::centredLeft);
    g.setColour(theme::kText.withAlpha(0.85f));
    g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    auto presetLabel = presetBox_.getBounds().translated(3, -19).withHeight(18);
    g.drawText("Preset", presetLabel, juce::Justification::centredLeft);
}

void OpenEdmKickEditor::resized() {
    auto bounds = getLocalBounds().reduced(20, 0);
    auto header = bounds.removeFromTop(72).reduced(0, 9);
    auditionBtn_.setBounds(header.removeFromRight(122).withSizeKeepingCentre(122, 40));
    header.removeFromRight(20);
    nextPresetBtn_.setBounds(header.removeFromRight(40).removeFromBottom(40));
    header.removeFromRight(5);
    prevPresetBtn_.setBounds(header.removeFromRight(40).removeFromBottom(40));
    header.removeFromRight(6);
    presetBox_.setBounds(header.removeFromRight(std::min(310, header.getWidth() - 242)).removeFromBottom(40));

    auto nav = bounds.removeFromTop(60).reduced(0, 10);
    for (auto* tab : { &kickTab_, &layersTab_, &patternTab_, &effectsTab_ }) {
        tab->setBounds(nav.removeFromLeft(90));
        nav.removeFromLeft(4);
    }
    viewToggleBtn_.setBounds(nav.removeFromRight(164));
    suiteBar_.setBounds(bounds.removeFromTop(48).reduced(0, 4));
    bounds.removeFromBottom(8);
    workspace_.setBounds(bounds);
    if (presetBrowser_.isVisible()) {
        auto browser = workspace_.getBounds().removeFromRight(std::min(420, workspace_.getWidth() - 24)).reduced(0, 8);
        browser.setHeight(std::min(460, browser.getHeight()));
        presetBrowser_.setBounds(browser);
        presetBrowser_.toFront(false);
    }
    auto* panel = workspace_.getViewedComponent();
    if (panel == nullptr) return;
    int contentHeight = 528;
    if (page_ == Page::kick && isAdvancedView()) contentHeight = 1040;
    if (page_ == Page::effects) contentHeight = isAdvancedView() ? 920 : 560;
    const bool scroll = bounds.getHeight() < contentHeight;
    panel->setSize(bounds.getWidth() - (scroll ? workspace_.getScrollBarThickness() : 0),
                   std::max(bounds.getHeight(), contentHeight));
}
