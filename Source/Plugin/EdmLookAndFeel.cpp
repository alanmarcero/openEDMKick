#include "EdmLookAndFeel.h"
#include "Theme.h"

namespace openedmkick::ui {

namespace {

constexpr float kArcThickness = 3.5f;
constexpr float kCornerRadius = 4.5f;
constexpr float kControlFontSize = 12.0f;

} // namespace

EdmLookAndFeel::EdmLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, theme::kBackground);
    setColour(juce::Label::textColourId, theme::kText);
    setColour(juce::Slider::textBoxTextColourId, theme::kTextDim);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, theme::kTrack);
    setColour(juce::ComboBox::backgroundColourId, theme::kKnobBody);
    setColour(juce::ComboBox::textColourId, theme::kText);
    setColour(juce::ComboBox::arrowColourId, theme::kTextDim);
    setColour(juce::PopupMenu::backgroundColourId, theme::kPanel);
    setColour(juce::PopupMenu::textColourId, theme::kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::kTrack);
    setColour(juce::PopupMenu::highlightedTextColourId, theme::kText);
    setColour(juce::TextButton::buttonColourId, theme::kKnobBody);
    setColour(juce::TextButton::textColourOffId, theme::kTextDim);
    setColour(juce::TextButton::textColourOnId, theme::kBackground);
}

void EdmLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPos,
    float startAngle,
    float endAngle,
    juce::Slider& slider) {
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.5f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float arcRadius = radius - kArcThickness;
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    const bool isBipolar = juce::approximatelyEqual(slider.getMinimum(), -slider.getMaximum());
    const float originAngle = isBipolar ? (startAngle + endAngle) / 2.0f : startAngle;
    const auto stroke = juce::PathStrokeType(kArcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(theme::kTrack);
    g.strokePath(track, stroke);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, originAngle, valueAngle, true);
    g.setColour(accent);
    g.strokePath(value, stroke);

    const float bodyRadius = arcRadius - kArcThickness - 1.5f;
    const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(body.translated(0.0f, 2.0f));
    g.setGradientFill(juce::ColourGradient(
        theme::kKnobBody.brighter(0.65f), centre.x, body.getY(),
        theme::kKnobBody.darker(0.4f), centre.x, body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(theme::kTextDim.withAlpha(0.6f));
    g.drawEllipse(body, 1.0f);

    const auto tip = centre.getPointOnCircumference(bodyRadius - 2.5f, valueAngle);
    const auto tail = centre.getPointOnCircumference(bodyRadius * 0.35f, valueAngle);
    g.setColour(theme::kText);
    g.drawLine({ tail, tip }, 2.0f);
}

void EdmLookAndFeel::drawComboBox(
    juce::Graphics& g,
    int width,
    int height,
    bool,
    int,
    int,
    int,
    int,
    juce::ComboBox& box) {
    const auto bounds = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId).withAlpha(box.isMouseOver(true) ? 0.9f : 0.45f));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.0f);

    const auto arrowArea = bounds.withLeft(bounds.getRight() - 18.0f).reduced(5.0f, bounds.getHeight() * 0.38f);
    juce::Path arrow;
    arrow.addTriangle(arrowArea.getX(), arrowArea.getY(), arrowArea.getRight(), arrowArea.getY(),
                      arrowArea.getCentreX(), arrowArea.getBottom());
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void EdmLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

void EdmLookAndFeel::drawButtonBackground(
    juce::Graphics& g,
    juce::Button& button,
    const juce::Colour&,
    bool isHighlighted,
    bool) {
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto accent = button.findColour(juce::TextButton::buttonOnColourId);
    const bool isOn = button.getToggleState();
    g.setColour(isOn ? accent : theme::kKnobBody);
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(accent.withAlpha(isHighlighted || isOn ? 0.9f : 0.45f));
    g.drawRoundedRectangle(bounds, kCornerRadius, 1.0f);
}

void EdmLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    const bool isOn = button.getToggleState();
    g.setColour(button.findColour(isOn ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    float fontSize = kControlFontSize;
    if (button.getHeight() > 0 && button.getHeight() < 22) {
        fontSize = juce::jmax(8.0f, static_cast<float>(button.getHeight()) * 0.46f);
    }
    g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(1, 0), juce::Justification::centred);
}

juce::Font EdmLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return juce::FontOptions(kControlFontSize);
}

juce::Font EdmLookAndFeel::getPopupMenuFont() {
    return juce::FontOptions(13.5f);
}

juce::Label* EdmLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::FontOptions(11.0f));
    return label;
}

} // namespace openedmkick::ui
