#include "EdmLookAndFeel.h"
#include "Theme.h"

#include <algorithm>

namespace openedmkick::ui {
namespace {
constexpr float kCorner = 7.0f;
constexpr float kControlFont = 15.0f;
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
    setColour(juce::ScrollBar::thumbColourId, theme::kEdge.brighter(0.25f));
    setColour(juce::TextEditor::backgroundColourId, theme::kInk);
    setColour(juce::TextEditor::textColourId, theme::kText);
    setColour(juce::TextEditor::focusedOutlineColourId, theme::kViolet);
    setColour(juce::PopupMenu::backgroundColourId, theme::kSteel);
    setColour(juce::PopupMenu::textColourId, theme::kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::kViolet);
    setColour(juce::PopupMenu::highlightedTextColourId, theme::kInk);
    setColour(juce::TextButton::buttonColourId, theme::kSteel);
    setColour(juce::TextButton::textColourOffId, theme::kText);
    setColour(juce::TextButton::textColourOnId, theme::kInk);
    setColour(juce::TooltipWindow::backgroundColourId, theme::kSteel.brighter(0.1f));
    setColour(juce::TooltipWindow::textColourId, theme::kText);
}

void EdmLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                      float startAngle, float endAngle, juce::Slider& slider) {
    const float alpha = slider.isEnabled() ? 1.0f : 0.4f;
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
    g.setColour(theme::kTrack.withMultipliedAlpha(alpha));
    g.strokePath(base, stroke);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, origin, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId).withMultipliedAlpha(alpha));
    g.strokePath(value, stroke);

    const float bodyRadius = std::max(2.0f, arcRadius - 5.5f);
    const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.28f * alpha));
    g.fillEllipse(body.translated(0.0f, 2.0f));
    g.setGradientFill(juce::ColourGradient(theme::kSteel.brighter(0.2f).withMultipliedAlpha(alpha), body.getX(), body.getY(),
                                           theme::kInk.withMultipliedAlpha(alpha), body.getRight(), body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(theme::kEdge.withMultipliedAlpha(alpha));
    g.drawEllipse(body, 1.0f);
    if (slider.isEnabled() && slider.hasKeyboardFocus(true)) {
        g.setColour(theme::kViolet);
        g.drawEllipse(area.reduced(1), 1.5f);
    }
    const auto tip = centre.getPointOnCircumference(bodyRadius * 0.78f, angle);
    const auto root = centre.getPointOnCircumference(bodyRadius * 0.15f, angle);
    g.setColour(theme::kText.withMultipliedAlpha(alpha));
    g.drawLine({root, tip}, 1.7f);
}

void EdmLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) {
    const float alpha = box.isEnabled() ? 1.0f : 0.4f;
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f,
                                               static_cast<float>(height) - 1.0f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(bounds, kCorner);
    const bool hot = box.isEnabled() && (box.isMouseOver(true) || box.hasKeyboardFocus(true));
    auto outline = box.findColour(juce::ComboBox::outlineColourId).withAlpha(hot ? 0.9f : 0.55f);
    if (box.isEnabled() && box.hasKeyboardFocus(true)) outline = theme::kViolet;
    g.setColour(outline.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(bounds, kCorner, 1.0f);
    const auto arrow = bounds.withLeft(bounds.getRight() - 19.0f)
        .reduced(5.0f, static_cast<float>(height) * 0.36f);
    juce::Path triangle;
    triangle.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withMultipliedAlpha(alpha));
    g.fillPath(triangle);
}

void EdmLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(8, 0, box.getWidth() - 25, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

void EdmLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                          const juce::Colour&, bool highlighted, bool down) {
    const float alpha = button.isEnabled() ? 1.0f : 0.4f;
    const bool showHot = button.isEnabled() && highlighted;
    const bool showDown = button.isEnabled() && down;
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto accent = button.findColour(juce::TextButton::buttonOnColourId);
    const bool active = button.getToggleState();
    if (button.getComponentID().startsWith("page_")) {
        if (showHot || showDown) {
            g.setColour(theme::kSteel.withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(bounds, kCorner);
        }
        if (active || (button.isEnabled() && button.hasKeyboardFocus(true))) {
            g.setColour(theme::kViolet.withMultipliedAlpha(alpha));
            g.fillRoundedRectangle(bounds.withTrimmedLeft(8).withTrimmedRight(8).withHeight(3)
                                       .withY(bounds.getBottom() - 3), 1.5f);
        }
        return;
    }
    const auto fill = active ? accent : button.findColour(juce::TextButton::buttonColourId);
    const auto drawnFill = showDown ? fill.brighter(0.12f) : (showHot ? fill.brighter(0.07f) : fill);
    g.setColour(drawnFill.withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(bounds, kCorner);
    auto edge = active || showHot ? accent : theme::kEdge;
    if (button.isEnabled() && button.hasKeyboardFocus(true)) edge = theme::kViolet;
    g.setColour(edge.withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(bounds, kCorner, (button.isEnabled() && button.hasKeyboardFocus(true)) ? 1.8f : 1.0f);
}

void EdmLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    const float alpha = button.isEnabled() ? 1.0f : 0.4f;
    const bool active = button.getToggleState();
    auto colour = button.findColour(active ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    if (active && button.getComponentID().startsWith("page_")) colour = theme::kViolet;
    g.setColour(colour.withMultipliedAlpha(alpha));
    const float size = button.getHeight() < 23 ? 9.0f : kControlFont;
    g.setFont(juce::FontOptions(size, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 0), juce::Justification::centred, 1);
}

juce::Font EdmLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::FontOptions(kControlFont); }
juce::Font EdmLookAndFeel::getPopupMenuFont() { return juce::FontOptions(13.0f); }

juce::Label* EdmLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::FontOptions(12.5f, juce::Font::bold));
    label->setJustificationType(juce::Justification::centred);
    return label;
}

} // namespace openedmkick::ui
