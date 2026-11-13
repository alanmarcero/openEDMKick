#include "EdmLookAndFeel.h"
#include "Theme.h"

#include <algorithm>

namespace openedmkick::ui {
namespace {
constexpr float kCorner = 7.0f;
constexpr float kControlFont = 11.5f;
}

EdmLookAndFeel::EdmLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, theme::kInk);
    setColour(juce::Label::textColourId, theme::kText);
    setColour(juce::Slider::textBoxTextColourId, theme::kText);
    setColour(juce::Slider::textBoxHighlightColourId, theme::kViolet.withAlpha(0.3f));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, theme::kInk.brighter(0.04f));
    setColour(juce::ComboBox::backgroundColourId, theme::kSteel);
    setColour(juce::ComboBox::textColourId, theme::kText);
    setColour(juce::ComboBox::arrowColourId, theme::kViolet);
    setColour(juce::PopupMenu::backgroundColourId, theme::kSteel);
    setColour(juce::PopupMenu::textColourId, theme::kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::kViolet);
    setColour(juce::PopupMenu::highlightedTextColourId, theme::kInk);
    setColour(juce::TextButton::buttonColourId, theme::kSteel);
    setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    setColour(juce::TextButton::textColourOnId, theme::kInk);
    setColour(juce::TooltipWindow::backgroundColourId, theme::kSteel.brighter(0.1f));
    setColour(juce::TooltipWindow::textColourId, theme::kText);
}

void EdmLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                      float startAngle, float endAngle, juce::Slider& slider) {
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = std::min(area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float arcRadius = std::max(4.0f, radius - 4.0f);
    const float angle = startAngle + position * (endAngle - startAngle);
    const float zeroAngle = (startAngle + endAngle) * 0.5f;
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float origin = bipolar ? zeroAngle : startAngle;
    const auto stroke = juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path base;
    base.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(theme::kTrack);
    g.strokePath(base, stroke);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, origin, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(value, stroke);

    const float bodyRadius = std::max(2.0f, arcRadius - 5.5f);
    const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillEllipse(body.translated(0.0f, 2.0f));
    g.setGradientFill(juce::ColourGradient(theme::kSteel.brighter(0.2f), body.getX(), body.getY(),
                                           theme::kInk, body.getRight(), body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(theme::kEdge);
    g.drawEllipse(body, 1.0f);
    const auto tip = centre.getPointOnCircumference(bodyRadius * 0.78f, angle);
    const auto root = centre.getPointOnCircumference(bodyRadius * 0.15f, angle);
    g.setColour(theme::kText);
    g.drawLine({root, tip}, 1.7f);
}

void EdmLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f,
                                               static_cast<float>(height) - 1.0f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, kCorner);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId).withAlpha(box.isMouseOver(true) ? 0.9f : 0.55f));
    g.drawRoundedRectangle(bounds, kCorner, 1.0f);
    const auto arrow = bounds.withLeft(bounds.getRight() - 19.0f)
        .reduced(5.0f, static_cast<float>(height) * 0.36f);
    juce::Path triangle;
    triangle.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(triangle);
}

void EdmLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(8, 0, box.getWidth() - 25, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

void EdmLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                          const juce::Colour&, bool highlighted, bool down) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto accent = button.findColour(juce::TextButton::buttonOnColourId);
    const bool active = button.getToggleState();
    const auto fill = active ? accent : theme::kSteel;
    g.setColour(down ? fill.brighter(0.12f) : (highlighted ? fill.brighter(0.07f) : fill));
    g.fillRoundedRectangle(bounds, kCorner);
    g.setColour(accent.withAlpha(active ? 0.95f : (highlighted ? 0.85f : 0.45f)));
    g.drawRoundedRectangle(bounds, kCorner, 1.0f);
}

void EdmLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    const bool active = button.getToggleState();
    g.setColour(button.findColour(active ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    const float size = button.getHeight() < 23 ? 9.0f : kControlFont;
    g.setFont(juce::FontOptions(size, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 0), juce::Justification::centred, 1);
}

juce::Font EdmLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::FontOptions(kControlFont); }
juce::Font EdmLookAndFeel::getPopupMenuFont() { return juce::FontOptions(13.0f); }

juce::Label* EdmLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::FontOptions(9.5f, juce::Font::bold));
    label->setJustificationType(juce::Justification::centred);
    return label;
}

} // namespace openedmkick::ui
