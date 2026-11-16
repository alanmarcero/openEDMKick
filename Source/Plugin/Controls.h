#pragma once

#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace openedmkick::ui {

using Apvts = juce::AudioProcessorValueTreeState;

class ParameterControl final : public juce::Component, private juce::Timer {
public:
    ParameterControl(Apvts&, juce::String id, juce::Component& owner, juce::Component& input);
    ~ParameterControl() override;
    void bind(juce::String id);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void refresh();
private:
    void timerCallback() override { refresh(); }
    Apvts& state_;
    juce::String id_;
    juce::Component& owner_;
    juce::Component& input_;
};

class SectionLock final : public juce::TextButton, private juce::Timer {
public:
    SectionLock(Apvts&, juce::String name, juce::StringArray ids);
    const juce::StringArray& parameterIds() const noexcept { return ids_; }
    void bind(juce::StringArray ids);
private:
    void timerCallback() override;
    Apvts& state_;
    juce::StringArray ids_;
};

class RoutedSlider : public juce::Slider {
public:
    using juce::Slider::Slider;
    void mouseDown(const juce::MouseEvent& event) override { if (!event.mods.isPopupMenu()) juce::Slider::mouseDown(event); }
};
class RoutedChoice : public juce::ComboBox {
public:
    void mouseDown(const juce::MouseEvent& event) override { if (!event.mods.isPopupMenu()) juce::ComboBox::mouseDown(event); }
};
class RoutedButton : public juce::TextButton {
public:
    using juce::TextButton::TextButton;
    void mouseDown(const juce::MouseEvent& event) override { if (!event.mods.isPopupMenu()) juce::TextButton::mouseDown(event); }
};

class Knob : public juce::Component {
public:
    Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);
    void refreshMidiMirror() { routing_->refresh(); }
    [[nodiscard]] juce::Slider& slider() noexcept { return slider_; }

private:
    RoutedSlider slider_;
    std::unique_ptr<ParameterControl> routing_;
    juce::Label title_;
    Apvts* state_ = nullptr;
    juce::String parameterId_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

class Choice : public juce::Component {
public:
    Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::ComboBox& box() noexcept { return box_; }

private:
    RoutedChoice box_;
    std::unique_ptr<ParameterControl> routing_;
    juce::Label title_;
    Apvts* state_ = nullptr;
    juce::String parameterId_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

class Toggle : public juce::Component {
public:
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent);
    void resized() override;
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::TextButton& button() noexcept { return button_; }

private:
    RoutedButton button_;
    std::unique_ptr<ParameterControl> routing_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components, int gap = 5);
void drawCard(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
              juce::Colour accent, bool inset = false);
void drawSmallLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour);

} // namespace openedmkick::ui
