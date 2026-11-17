#include "Controls.h"
#include "Theme.h"
#include "PluginProcessor.h"

#include <algorithm>

namespace openedmkick::ui {

namespace {
constexpr int kLabelHeight = 14;
constexpr int kKnobValueHeight = 15;
constexpr int kRoutingHeight = 40;
constexpr float kLabelFont = 9.5f;
}

ParameterControl::ParameterControl(Apvts& state, juce::String id, juce::Component& owner, juce::Component& input)
    : state_(state), id_(std::move(id)), owner_(owner), input_(input) {
    setComponentID("routing_" + id_);
    setTooltip("Click the lock to keep this value when loading a preset. Right-click to assign a MIDI CC.");
    owner_.addMouseListener(this, false);
    for (auto* child : owner_.getChildren()) if (child != this) child->addMouseListener(this, true);
    startTimerHz(25);
}
ParameterControl::~ParameterControl() {
    owner_.removeMouseListener(this);
    for (auto* child : owner_.getChildren()) if (child != this) child->removeMouseListener(this);
}
void ParameterControl::bind(juce::String id) { id_ = std::move(id); setComponentID("routing_" + id_); refresh(); }
void ParameterControl::paint(juce::Graphics& g) {
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    g.setColour(routing.isLocked(id_) ? theme::kBrass : theme::kTextDim);
    g.drawRoundedRectangle(12, 20, 16, 13, 2, 1.5f);
    if (routing.isLocked(id_)) g.drawRoundedRectangle(15, 8, 10, 18, 5, 1.5f);
    if (!routing.isLocked(id_)) {
        juce::Path shackle;
        shackle.startNewSubPath(15, 22);
        shackle.lineTo(15, 16);
        shackle.quadraticTo(15, 8, 21, 8);
        shackle.lineTo(25, 12);
        g.strokePath(shackle, juce::PathStrokeType(1.5f));
    }
    g.setFont(juce::FontOptions(12.0f));
    const int cc = routing.assignment(id_);
    g.setColour(cc >= 0 ? theme::kViolet : theme::kTextDim);
    g.drawText(cc >= 0 ? "CC " + juce::String(cc) : "CC -", getLocalBounds().withTrimmedLeft(44), juce::Justification::centredLeft);
}
void ParameterControl::mouseDown(const juce::MouseEvent& event) {
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    if (!event.mods.isPopupMenu() && !(event.eventComponent == this && event.position.x >= 40)) {
        if (event.eventComponent == this) routing.setLocked(id_, !routing.isLocked(id_));
        repaint();
        return;
    }
    juce::PopupMenu menu;
    menu.addSubMenu("MIDI CC", routing.midiMenu(id_));
    menu.addItem(200, routing.isLocked(id_) ? "Unlock for presets" : "Lock for presets");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safe = juce::Component::SafePointer<ParameterControl>(this)](int selected) {
            if (safe == nullptr || selected == 0) return;
            auto& model = static_cast<OpenEdmKickProcessor&>(safe->state_.processor).routing();
            if (selected == 200) model.setLocked(safe->id_, !model.isLocked(safe->id_));
            if (selected >= 1 && selected <= 129) model.assignCc(safe->id_, selected - 2);
            if (selected == 130) model.assignCc(safe->id_, model.defaultAssignment(safe->id_));
            safe->refresh();
        });
}
void ParameterControl::refresh() {
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    {
        const float value = routing.displayValue(id_);
        if (auto* slider = dynamic_cast<juce::Slider*>(&input_)) slider->setValue(value, juce::dontSendNotification);
        if (auto* choice = dynamic_cast<juce::ComboBox*>(&input_)) choice->setSelectedItemIndex(static_cast<int>(value), juce::dontSendNotification);
        if (auto* button = dynamic_cast<juce::Button*>(&input_)) button->setToggleState(value >= 0.5f, juce::dontSendNotification);
    }
    repaint();
}
SectionLock::SectionLock(Apvts& state, juce::String name, juce::StringArray ids) : state_(state), ids_(std::move(ids)) {
    setComponentID("section_lock_" + name);
    setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
    onClick = [this] { static_cast<OpenEdmKickProcessor&>(state_.processor).routing().toggleSection(ids_); timerCallback(); };
    startTimerHz(10);
    timerCallback();
}
void SectionLock::bind(juce::StringArray ids) { ids_ = std::move(ids); timerCallback(); }
void SectionLock::timerCallback() {
    const bool locked = static_cast<OpenEdmKickProcessor&>(state_.processor).routing().sectionLocked(ids_);
    setButtonText(locked ? "Unlock section" : "Lock section");
    setToggleState(locked, juce::dontSendNotification);
}

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : slider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      title_({}, title) {
    slider_.setComponentID(parameterId);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, kKnobValueHeight);
    slider_.setPopupDisplayEnabled(false, false, nullptr);
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider_.setColour(juce::Slider::rotarySliderOutlineColourId, theme::kTrack);
    title_.setFont(juce::FontOptions(kLabelFont, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title_);
    addAndMakeVisible(slider_);
    routing_ = std::make_unique<ParameterControl>(state, parameterId, *this, slider_);
    addAndMakeVisible(*routing_);
    bind(state, parameterId);
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    slider_.onValueChange = nullptr;
    slider_.onDragStart = nullptr;
    slider_.onDragEnd = nullptr;
    attachment_.reset();
    state_ = &state;
    parameterId_ = parameterId;
    auto* parameter = state.getParameter(parameterId);
    const auto& range = parameter->getNormalisableRange();
    slider_.setRange(range.start, range.end, range.interval);
    slider_.setSkewFactor(range.skew, range.symmetricSkew);
    slider_.textFromValueFunction = [parameter](double value) { return parameter->getText(parameter->convertTo0to1(static_cast<float>(value)), 64); };
    slider_.valueFromTextFunction = [parameter](const juce::String& text) { return parameter->convertFrom0to1(parameter->getValueForText(text)); };
    attachment_ = std::make_unique<juce::ParameterAttachment>(*parameter,
        [this](float value) { slider_.setValue(value, juce::dontSendNotification); });
    slider_.onDragStart = [this] { attachment_->beginGesture(); };
    slider_.onDragEnd = [this] { attachment_->endGesture(); };
    slider_.onValueChange = [this] {
        const float value = static_cast<float>(slider_.getValue());
        static_cast<OpenEdmKickProcessor&>(state_->processor).routing().manualValue(parameterId_, value);
        if (slider_.isMouseButtonDown()) attachment_->setValueAsPartOfGesture(value);
        if (!slider_.isMouseButtonDown()) attachment_->setValueAsCompleteGesture(value);
    };
    slider_.setComponentID(parameterId);
    routing_->bind(parameterId);
    attachment_->sendInitialUpdate();
    slider_.updateText();
}

void Knob::setTooltip(const juce::String& text) { slider_.setTooltip(text); title_.setTooltip(text); }

void Knob::resized() {
    auto area = getLocalBounds();
    routing_->setBounds(area.removeFromBottom(kRoutingHeight));
    title_.setBounds(area.removeFromTop(kLabelHeight));
    const int boxHeight = std::min(kKnobValueHeight, std::max(0, area.getHeight() / 4));
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::max(20, area.getWidth()), boxHeight);
    slider_.setBounds(area.withSizeKeepingCentre(std::min(112, area.getWidth()), std::min(96, area.getHeight())));
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : title_({}, title) {
    box_.setComponentID(parameterId);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId)))
        box_.addItemList(parameter->choices, 1);
    box_.setColour(juce::ComboBox::outlineColourId, accent);
    box_.setColour(juce::ComboBox::backgroundColourId, theme::kSteel);
    title_.setFont(juce::FontOptions(kLabelFont, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title_);
    addAndMakeVisible(box_);
    routing_ = std::make_unique<ParameterControl>(state, parameterId, *this, box_);
    addAndMakeVisible(*routing_);
    bind(state, parameterId);
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    state_ = &state;
    parameterId_ = parameterId;
    attachment_ = std::make_unique<juce::ParameterAttachment>(*state.getParameter(parameterId),
        [this](float value) { box_.setSelectedItemIndex(static_cast<int>(value), juce::dontSendNotification); });
    box_.onChange = [this] {
        const float value = static_cast<float>(box_.getSelectedItemIndex());
        static_cast<OpenEdmKickProcessor&>(state_->processor).routing().manualValue(parameterId_, value);
        attachment_->setValueAsCompleteGesture(value);
    };
    box_.setComponentID(parameterId);
    routing_->bind(parameterId);
    attachment_->sendInitialUpdate();
}
void Choice::setTooltip(const juce::String& text) { box_.setTooltip(text); }
void Choice::resized() {
    auto area = getLocalBounds();
    routing_->setBounds(area.removeFromBottom(kRoutingHeight));
    if (title_.getText().isNotEmpty()) title_.setBounds(area.removeFromTop(kLabelHeight));
    box_.setBounds(area.withHeight(std::min(36, area.getHeight())).withY(area.getY() + (area.getHeight() - std::min(36, area.getHeight())) / 2));
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent)
    : button_(text) {
    button_.setComponentID(parameterId);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    addAndMakeVisible(button_);
    routing_ = std::make_unique<ParameterControl>(state, parameterId, *this, button_);
    addAndMakeVisible(*routing_);
    attachment_ = std::make_unique<juce::ParameterAttachment>(*state.getParameter(parameterId),
        [this](float value) { button_.setToggleState(value >= 0.5f, juce::dontSendNotification); });
    button_.onClick = [this, &state, parameterId] {
        const float value = button_.getToggleState() ? 1.0f : 0.0f;
        static_cast<OpenEdmKickProcessor&>(state.processor).routing().manualValue(parameterId, value);
        attachment_->setValueAsCompleteGesture(value);
    };
    attachment_->sendInitialUpdate();
}
void Toggle::resized() { auto area = getLocalBounds(); routing_->setBounds(area.removeFromBottom(kRoutingHeight)); button_.setBounds(area.withHeight(std::min(36, area.getHeight())).withY(area.getCentreY() - std::min(36, area.getHeight()) / 2)); }
void Toggle::setTooltip(const juce::String& text) { button_.setTooltip(text); }

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components, int gap) {
    const int count = static_cast<int>(components.size());
    if (count <= 0) return;
    const int safeGap = std::max(0, gap);
    const int width = std::max(0, (area.getWidth() - safeGap * (count - 1)) / count);
    for (auto* component : components) {
        component->setBounds(area.removeFromLeft(width));
        area.removeFromLeft(safeGap);
    }
}

void drawCard(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent, bool inset) {
    const auto r = area.toFloat();
    g.setColour(inset ? theme::kInk.brighter(0.04f) : theme::kSteel);
    g.fillRoundedRectangle(r, 10.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(r.getX() + 12.0f, r.getY() + 8.0f, 3.0f, 30.0f, 1.5f);
    g.setColour(accent);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(title, area.getX() + 22, area.getY() + 7, area.getWidth() - 190, 34,
               juce::Justification::centredLeft);
}

void drawSmallLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour) {
    g.setColour(colour);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(text, area, juce::Justification::centredLeft);
}

} // namespace openedmkick::ui
