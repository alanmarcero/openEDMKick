#include "Controls.h"
#include "Theme.h"
#include <ranges>

namespace openedmkick::ui {

namespace {

constexpr int kTitleHeight = 14;
constexpr int kValueHeight = 14;
constexpr int kControlHeight = 24;
constexpr int kCellGap = 4;
constexpr float kTitleFontSize = 10.5f;

} // namespace

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : slider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      title_({}, title.toUpperCase()),
      attachment_(std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_)) {
    slider_.setComponentID(parameterId);
    slider_.onDragStart = [this] { dragging_ = true; };
    slider_.onDragEnd = [this] {
        dragging_ = false;
        if (pendingState_ != nullptr) {
            auto* nextState = std::exchange(pendingState_, nullptr);
            bind(*nextState, std::exchange(pendingId_, {}));
        }
    };
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, kValueHeight);
    slider_.setPopupDisplayEnabled(false, false, nullptr);
    title_.setFont(juce::FontOptions(kTitleFontSize, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slider_);
    addAndMakeVisible(title_);
}

void Knob::bind(Apvts& state, const juce::String& parameterId) {
    if (dragging_) {
        pendingState_ = &state;
        pendingId_ = parameterId;
        return;
    }
    attachment_.reset();
    slider_.setComponentID(parameterId);
    attachment_ = std::make_unique<Apvts::SliderAttachment>(state, parameterId, slider_);
}

void Knob::setTooltip(const juce::String& text) {
    slider_.setTooltip(text);
    title_.setTooltip(text);
}

void Knob::resized() {
    auto bounds = getLocalBounds();
    title_.setBounds(bounds.removeFromTop(kTitleHeight));
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, bounds.getWidth(), kValueHeight);
    slider_.setBounds(bounds);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : title_({}, title.toUpperCase()) {
    box_.setComponentID(parameterId);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId))) {
        box_.addItemList(parameter->choices, 1);
    }
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
    box_.setColour(juce::ComboBox::outlineColourId, accent);
    title_.setFont(juce::FontOptions(kTitleFontSize, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kTextDim);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(box_);
    addChildComponent(title_);
    title_.setVisible(title.isNotEmpty());
}

void Choice::bind(Apvts& state, const juce::String& parameterId) {
    attachment_.reset();
    box_.setComponentID(parameterId);
    attachment_ = std::make_unique<Apvts::ComboBoxAttachment>(state, parameterId, box_);
}

void Choice::setTooltip(const juce::String& text) {
    box_.setTooltip(text);
}

void Choice::resized() {
    auto bounds = getLocalBounds();
    if (title_.isVisible()) {
        title_.setBounds(bounds.removeFromTop(kTitleHeight));
    }
    box_.setBounds(bounds.withSizeKeepingCentre(bounds.getWidth(), juce::jmin(kControlHeight, bounds.getHeight())));
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent)
    : button_(text),
      attachment_(state, parameterId, button_) {
    button_.setComponentID(parameterId);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    addAndMakeVisible(button_);
}

void Toggle::setTooltip(const juce::String& text) {
    button_.setTooltip(text);
}

void Toggle::resized() {
    const auto bounds = getLocalBounds();
    button_.setBounds(bounds.withSizeKeepingCentre(bounds.getWidth(), juce::jmin(kControlHeight, bounds.getHeight())));
}

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components) {
    const int count = static_cast<int>(components.size());
    if (count <= 0) {
        return;
    }
    const int cellWidth = (area.getWidth() - kCellGap * (count - 1)) / count;
    std::ranges::for_each(components, [&](juce::Component* component) {
        component->setBounds(area.removeFromLeft(cellWidth));
        area.removeFromLeft(kCellGap);
    });
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent) {
    const auto bounds = area.toFloat();
    g.setGradientFill(juce::ColourGradient(
        theme::kPanel.brighter(0.06f), bounds.getTopLeft(),
        theme::kPanel.darker(0.16f), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(bounds.getX() + 12.0f, bounds.getY() + 10.0f, 3.0f, 14.0f, 1.5f);
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.drawText(title.toUpperCase(), area.getX() + 22, area.getY() + 7, 320, 20, juce::Justification::centredLeft);
}

void drawCaption(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text) {
    g.setColour(theme::kTextDim.withAlpha(0.75f));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(text.toUpperCase(), area.removeFromTop(12), juce::Justification::centredLeft);
}

} // namespace openedmkick::ui
