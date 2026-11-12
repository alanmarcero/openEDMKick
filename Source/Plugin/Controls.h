#pragma once

#include "Parameters.h"
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace openedmkick::ui {

using Apvts = juce::AudioProcessorValueTreeState;

class Knob : public juce::Component {
public:
    Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::Slider& slider() noexcept { return slider_; }

private:
    juce::Slider slider_;
    juce::Label title_;
    std::unique_ptr<Apvts::SliderAttachment> attachment_;
};

class Choice : public juce::Component {
public:
    Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void bind(Apvts& state, const juce::String& parameterId);
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::ComboBox& box() noexcept { return box_; }

private:
    juce::ComboBox box_;
    juce::Label title_;
    std::unique_ptr<Apvts::ComboBoxAttachment> attachment_;
};

class Toggle : public juce::Component {
public:
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent);
    void resized() override;
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::TextButton& button() noexcept { return button_; }

private:
    juce::TextButton button_;
    Apvts::ButtonAttachment attachment_;
};

class SectionLock : public juce::Component, private juce::Timer {
public:
    SectionLock(const juce::String& title, juce::Colour accent,
                std::function<bool()> readState, std::function<void(bool)> writeState);
    void resized() override;

private:
    void timerCallback() override;
    void sync();
    juce::TextButton button_;
    juce::String title_;
    juce::Colour accent_;
    std::function<bool()> readState_;
    std::function<void(bool)> writeState_;
};

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components, int gap = 5);
void drawCard(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
              juce::Colour accent, bool inset = false);
void drawSmallLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour);

} // namespace openedmkick::ui
