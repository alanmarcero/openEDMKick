#include "Controls.h"
#include "Theme.h"

#include <algorithm>

namespace openedmkick::ui {

namespace {
constexpr int kLabelHeight = 14;
constexpr int kKnobValueHeight = 15;
constexpr float kLabelFont = 9.5f;
}

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : slider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      title_({}, title.toUpperCase()),
      attachment_(std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_)) {
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
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    slider_.setComponentID(parameterId);
    attachment_ = std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_);
}

void Knob::setTooltip(const juce::String& text) { slider_.setTooltip(text); title_.setTooltip(text); }

void Knob::resized() {
    auto area = getLocalBounds();
    title_.setBounds(area.removeFromTop(kLabelHeight));
    const int boxHeight = std::min(kKnobValueHeight, std::max(0, area.getHeight() / 4));
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::max(20, area.getWidth()), boxHeight);
    slider_.setBounds(area);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : title_({}, title.toUpperCase()) {
    box_.setComponentID(parameterId);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId)))
        box_.addItemList(parameter->choices, 1);
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
    box_.setColour(juce::ComboBox::outlineColourId, accent);
    box_.setColour(juce::ComboBox::backgroundColourId, theme::kSteel);
    title_.setFont(juce::FontOptions(kLabelFont, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title_);
    addAndMakeVisible(box_);
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    box_.setComponentID(parameterId);
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
}
void Choice::setTooltip(const juce::String& text) { box_.setTooltip(text); }
void Choice::resized() {
    auto area = getLocalBounds();
    if (title_.getText().isNotEmpty()) title_.setBounds(area.removeFromTop(kLabelHeight));
    box_.setBounds(area.withHeight(std::min(28, area.getHeight())).withY(area.getY() + (area.getHeight() - std::min(28, area.getHeight())) / 2));
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent)
    : button_(text), attachment_(state, parameterId, button_) {
    button_.setComponentID(parameterId);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    addAndMakeVisible(button_);
}
void Toggle::resized() { button_.setBounds(getLocalBounds()); }
void Toggle::setTooltip(const juce::String& text) { button_.setTooltip(text); }

SectionLock::SectionLock(const juce::String& title, juce::Colour accent,
                         std::function<bool()> readState, std::function<void(bool)> writeState)
    : title_(title), accent_(accent), readState_(std::move(readState)), writeState_(std::move(writeState)) {
    button_.setComponentID("lock_" + title.toLowerCase().replace(" ", "_"));
    button_.setTooltip("Keep this section unchanged when selecting another preset.");
    button_.onClick = [this] { if (writeState_) writeState_(!readState_()); sync(); };
    addAndMakeVisible(button_);
    sync();
    startTimerHz(5);
}

void SectionLock::timerCallback() { sync(); }
void SectionLock::sync() {
    const bool locked = readState_ && readState_();
    button_.setButtonText(locked ? "LOCKED" : "LOCK");
    button_.setToggleState(locked, juce::dontSendNotification);
    button_.setColour(juce::TextButton::buttonOnColourId, locked ? accent_ : theme::kSteel);
    button_.setColour(juce::TextButton::textColourOffId, locked ? theme::kInk : theme::kTextDim);
    button_.setColour(juce::TextButton::textColourOnId, theme::kInk);
}
void SectionLock::resized() { button_.setBounds(getLocalBounds()); }

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
    g.fillRoundedRectangle(r.getX() + 12.0f, r.getY() + 9.0f, 3.0f, 13.0f, 1.5f);
    g.setColour(accent);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(title.toUpperCase(), area.getX() + 22, area.getY() + 7, area.getWidth() - 32, 18,
               juce::Justification::centredLeft);
}

void drawSmallLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour) {
    g.setColour(colour);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(text.toUpperCase(), area, juce::Justification::centredLeft);
}

} // namespace openedmkick::ui
