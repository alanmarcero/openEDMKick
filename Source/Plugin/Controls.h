#pragma once

#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace openedmkick::ui {

using Apvts = juce::AudioProcessorValueTreeState;

class ParameterControl final : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer {
public:
    ParameterControl(Apvts&, juce::String id, juce::Component& owner, juce::Component& input, bool attachChildren = true);
    ~ParameterControl() override;
    void bind(juce::String id);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void refresh();
    // A merged switch shows on while either parameter is on.
    void mirrorPartner(juce::String id) { partnerId_ = std::move(id); }
private:
    void timerCallback() override { refresh(); }
    Apvts& state_;
    juce::String id_;
    juce::String partnerId_;
    juce::Component& owner_;
    juce::Component& input_;
    bool attachChildren_ = true;
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
    void showEffectiveText(const juce::String& text);
    void refreshMidiMirror() { routing_->refresh(); }
    [[nodiscard]] juce::Slider& slider() noexcept { return slider_; }

private:
    RoutedSlider slider_;
    juce::String effectiveText_;
    std::unique_ptr<ParameterControl> routing_;
    juce::Label title_;
    Apvts* state_ = nullptr;
    juce::String parameterId_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

/** Dual concentric rotary control: inner cap adjusts dB gain, outer collar sweeps Hz frequency. */
class DualEqKnob final : public juce::Component, private juce::Timer {
public:
    DualEqKnob(Apvts& state, const juce::String& freqId, const juce::String& dbId,
               const juce::String& title, juce::Colour dbAccent, juce::Colour freqAccent);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::Slider& slider() noexcept { return dbSlider_; }

private:
    class GainSlider final : public RoutedSlider {
    public:
        using RoutedSlider::RoutedSlider;
        bool hitTest(int x, int y) override;
    };

    void timerCallback() override;
    [[nodiscard]] juce::Rectangle<float> dialBounds() const;
    [[nodiscard]] juce::Rectangle<int> dbPillBounds() const;
    [[nodiscard]] juce::Rectangle<int> freqPillBounds() const;
    [[nodiscard]] bool hitsOuterRing(juce::Point<float> pos) const;
    void updateReadoutText();

    Apvts& state_;
    juce::String freqId_;
    juce::String dbId_;
    juce::Colour dbAccent_;
    juce::Colour freqAccent_;
    juce::Label title_;
    juce::Label dbReadout_;
    GainSlider dbSlider_;
    juce::Slider freqMirror_;
    std::unique_ptr<ParameterControl> dbRouting_;
    std::unique_ptr<ParameterControl> freqRouting_;
    std::unique_ptr<juce::ParameterAttachment> dbAttachment_;
    std::unique_ptr<juce::ParameterAttachment> freqAttachment_;
    float freqHz_ = 1000.0f;
    float displayedDb_ = 0.0f;
    float dragStartNorm_ = 0.5f;
    float dragStartY_ = 0.0f;
    bool draggingFreq_ = false;
    bool hoveringFreq_ = false;
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
    Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent,
           const juce::String& partnerId = {});
    void resized() override;
    void setTooltip(const juce::String& text);
    [[nodiscard]] juce::TextButton& button() noexcept { return button_; }

private:
    RoutedButton button_;
    std::unique_ptr<ParameterControl> routing_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
};

// Dims a control while its setting has no effect. Its tooltip then says what turns it on.
template <typename Control>
void showActivity(Control& control, bool active, const juce::String& activeTip, const juce::String& inactiveTip) {
    control.setAlpha(active ? 1.0f : 0.4f);
    control.setTooltip(active ? activeTip : inactiveTip);
}

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components, int gap = 5);
void drawCard(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
              juce::Colour accent, bool inset = false);
void drawSmallLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour);

} // namespace openedmkick::ui
