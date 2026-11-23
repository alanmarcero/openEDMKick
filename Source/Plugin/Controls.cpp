#include "Controls.h"
#include "Theme.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace openedmkick::ui {

namespace {
constexpr int kLabelHeight = 19;
constexpr int kKnobValueHeight = 19;
constexpr int kRoutingHeight = 40;
constexpr float kLabelFont = 13.0f;
}

ParameterControl::ParameterControl(Apvts& state, juce::String id, juce::Component& owner, juce::Component& input, bool attachChildren)
    : state_(state), id_(std::move(id)), owner_(owner), input_(input), attachChildren_(attachChildren) {
    setComponentID("routing_" + id_);
    setWantsKeyboardFocus(true);
    updateTitle();
    setTooltip("Click the lock to keep this value when loading a preset. Right-click to assign a MIDI CC.");
    owner_.addMouseListener(this, false);
    if (attachChildren_) {
        std::ranges::for_each(owner_.getChildren(), [this](juce::Component* child) {
            if (child != this) child->addMouseListener(this, true);
        });
    }
    startTimerHz(25);
}
ParameterControl::~ParameterControl() {
    owner_.removeMouseListener(this);
    if (attachChildren_) {
        std::ranges::for_each(owner_.getChildren(), [this](juce::Component* child) {
            if (child != this) child->removeMouseListener(this);
        });
    }
}
void ParameterControl::updateTitle() {
    auto* parameter = state_.getParameter(id_);
    const auto name = parameter != nullptr ? parameter->getName(128) : input_.getTitle();
    setTitle("Lock " + (name.isNotEmpty() ? name : id_));
}
void ParameterControl::bind(juce::String id) { id_ = std::move(id); setComponentID("routing_" + id_); updateTitle(); refresh(); }
void ParameterControl::paint(juce::Graphics& g) {
    if (hasKeyboardFocus(true)) {
        g.setColour(theme::kViolet);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.5f);
    }
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    const bool locked = routing.isLocked(id_);
    const int cc = routing.assignment(id_);
    juce::Graphics::ScopedSaveState state(g);
    g.setOrigin((getWidth() - 40) / 2, cc < 0 ? 2 : -5);
    const auto ink = locked ? theme::kMint : theme::kTextDim;
    const float alpha = locked ? 1.0f : (isMouseOver() ? 0.85f : 0.28f);
    const float stroke = locked ? 1.8f : 1.35f;
    g.setColour(ink.withAlpha(alpha));
    juce::Path shackle;
    shackle.startNewSubPath(15, 20);
    shackle.lineTo(15, 16);
    shackle.cubicTo(15, 10, 24, 10, 24, 16);
    if (locked) shackle.lineTo(24, 20);
    g.strokePath(shackle, juce::PathStrokeType(stroke));
    g.drawRoundedRectangle(13, 19, 13, 10, 2, stroke);
    if (cc >= 0) {
        g.setColour(theme::kViolet);
        g.setFont(juce::FontOptions(11.5f));
        g.drawText("CC " + juce::String(cc), 0, 28, 40, 13, juce::Justification::centred);
    }
}
void ParameterControl::mouseDown(const juce::MouseEvent& event) {
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    if (!event.mods.isPopupMenu() && !(event.eventComponent == this && routing.assignment(id_) >= 0 && event.position.y >= 24)) {
        if (event.eventComponent == this) routing.setLocked(id_, !routing.isLocked(id_));
        repaint();
        return;
    }
    juce::PopupMenu menu;
    menu.addSubMenu("MIDI CC", routing.midiMenu(id_));
    menu.addItem(200, routing.isLocked(id_) ? "Unlock for presets" : "Lock for presets");
    menu.addItem(201, "Reset to default");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safe = juce::Component::SafePointer<ParameterControl>(this)](int selected) {
            if (safe == nullptr || selected == 0) return;
            auto& model = static_cast<OpenEdmKickProcessor&>(safe->state_.processor).routing();
            if (selected == 200) model.setLocked(safe->id_, !model.isLocked(safe->id_));
            if (selected >= 1 && selected <= 129) model.assignCc(safe->id_, selected - 2);
            if (selected == 130) model.assignCc(safe->id_, model.defaultAssignment(safe->id_));
            if (selected == 201) {
                auto* parameter = safe->state_.getParameter(safe->id_);
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->getDefaultValue());
                parameter->endChangeGesture();
            }
            safe->refresh();
        });
}
bool ParameterControl::keyPressed(const juce::KeyPress& key) {
    if (key != juce::KeyPress::spaceKey && key != juce::KeyPress::returnKey) return false;
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    routing.setLocked(id_, !routing.isLocked(id_));
    repaint();
    return true;
}
void ParameterControl::refresh() {
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    {
        float value = routing.displayValue(id_);
        if (partnerId_.isNotEmpty()) value = std::max(value, routing.displayValue(partnerId_));
        if (auto* slider = dynamic_cast<juce::Slider*>(&input_)) slider->setValue(value, juce::dontSendNotification);
        if (auto* choice = dynamic_cast<juce::ComboBox*>(&input_)) choice->setSelectedItemIndex(static_cast<int>(value), juce::dontSendNotification);
        if (auto* button = dynamic_cast<juce::Button*>(&input_)) button->setToggleState(value >= 0.5f, juce::dontSendNotification);
    }
    repaint();
}
SectionLock::SectionLock(Apvts& state, juce::String name, juce::StringArray ids, juce::String sectionTitle)
    : state_(state), ids_(std::move(ids)), sectionTitle_(std::move(sectionTitle)) {
    setComponentID("section_lock_" + name);
    setTooltip("Keep this section’s settings when choosing another factory sound.");
    setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
    onClick = [this] { static_cast<OpenEdmKickProcessor&>(state_.processor).routing().toggleSection(ids_); timerCallback(); };
    startTimerHz(10);
    timerCallback();
}
void SectionLock::bind(juce::StringArray ids) { ids_ = std::move(ids); timerCallback(); }
void SectionLock::timerCallback() {
    const bool locked = static_cast<OpenEdmKickProcessor&>(state_.processor).routing().sectionLocked(ids_);
    const auto text = locked ? "Unlock section" : "Lock section";
    setButtonText(text);
    const auto verb = locked ? "Unlock " : "Lock ";
    setTitle(sectionTitle_.isNotEmpty() ? verb + sectionTitle_ : text);
    setToggleState(locked, juce::dontSendNotification);
}

Knob::Knob(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : slider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      title_({}, title) {
    slider_.setName(title);
    slider_.setTitle(title);
    slider_.setComponentID(parameterId);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, kKnobValueHeight);
    slider_.setPopupDisplayEnabled(false, false, nullptr);
    slider_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider_.setColour(juce::Slider::rotarySliderOutlineColourId, theme::kTrack);
    title_.setFont(juce::FontOptions(kLabelFont, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kText);
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
    slider_.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    slider_.textFromValueFunction = [this, parameter](double value) {
        if (effectiveText_.isNotEmpty()) return effectiveText_;
        return parameter->getText(parameter->convertTo0to1(static_cast<float>(value)), 64);
    };
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

// Shows what plays when a switch overrides the knob, such as "100% (on)". Empty text shows the value.
void Knob::showEffectiveText(const juce::String& text) {
    if (text == effectiveText_) return;
    effectiveText_ = text;
    slider_.updateText();
}

void Knob::resized() {
    auto area = getLocalBounds();
    auto group = area.withSizeKeepingCentre(std::min(area.getWidth(), 156), area.getHeight());
    routing_->setBounds(group.removeFromRight(kRoutingHeight).removeFromBottom(kRoutingHeight));
    title_.setBounds(group.removeFromTop(kLabelHeight));
    const int boxHeight = std::min(kKnobValueHeight, std::max(0, group.getHeight() / 4));
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::min(78, group.getWidth()), boxHeight);
    slider_.setBounds(group.withSizeKeepingCentre(std::min(124, group.getWidth()), std::min(104, group.getHeight())));
}

bool DualEqKnob::GainSlider::hitTest(int x, int y) {
    const auto centre = getLocalBounds().toFloat().getCentre();
    const float radius = static_cast<float>(std::min(getWidth(), getHeight())) * 0.5f;
    return juce::Point<float>(static_cast<float>(x), static_cast<float>(y)).getDistanceFrom(centre) <= radius;
}

DualEqKnob::DualEqKnob(Apvts& state, const juce::String& freqId, const juce::String& dbId,
                       const juce::String& title, juce::Colour dbAccent, juce::Colour freqAccent)
    : state_(state), freqId_(freqId), dbId_(dbId), dbAccent_(dbAccent), freqAccent_(freqAccent),
      title_({}, title),
      freqReadout_(*this),
      dbSlider_(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox) {
    title_.setFont(juce::FontOptions(kLabelFont, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kText);
    title_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title_);

    dbReadout_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    dbReadout_.setBorderSize(juce::BorderSize<int>(1, 3, 1, 3));
    dbReadout_.setColour(juce::Label::textColourId, theme::kText);
    dbReadout_.setColour(juce::Label::backgroundColourId, theme::kInk.brighter(0.04f));
    dbReadout_.setJustificationType(juce::Justification::centred);
    dbReadout_.setEditable(false, true, false);
    addAndMakeVisible(dbReadout_);

    freqReadout_.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    freqReadout_.setBorderSize(juce::BorderSize<int>(1, 3, 1, 3));
    freqReadout_.setColour(juce::Label::textColourId, theme::kText);
    freqReadout_.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    freqReadout_.setJustificationType(juce::Justification::centred);
    freqReadout_.setEditable(false, true, false);
    freqReadout_.setWantsKeyboardFocus(true);
    freqReadout_.setName(title + " Freq");
    freqReadout_.setTitle(title + " Freq");
    addAndMakeVisible(freqReadout_);

    auto* dbParam = state.getParameter(dbId);
    const auto& dbRange = dbParam->getNormalisableRange();
    dbSlider_.setName(title + " Gain");
    dbSlider_.setTitle(title + " Gain");
    dbSlider_.setComponentID(dbId);
    dbSlider_.setRange(dbRange.start, dbRange.end, dbRange.interval);
    dbSlider_.setSkewFactor(dbRange.skew, dbRange.symmetricSkew);
    dbSlider_.setDoubleClickReturnValue(true, dbParam->convertFrom0to1(dbParam->getDefaultValue()));
    dbSlider_.setColour(juce::Slider::rotarySliderFillColourId, dbAccent_);
    dbSlider_.setColour(juce::Slider::rotarySliderOutlineColourId, theme::kTrack);
    addAndMakeVisible(dbSlider_);

    auto* freqParam = state.getParameter(freqId);
    const auto& freqRange = freqParam->getNormalisableRange();
    freqMirror_.setName(title + " Freq");
    freqMirror_.setTitle(title + " Freq");
    freqMirror_.setComponentID(freqId);
    freqMirror_.setRange(freqRange.start, freqRange.end, freqRange.interval);
    freqMirror_.setSkewFactor(freqRange.skew, freqRange.symmetricSkew);
    addChildComponent(freqMirror_);

    dbRouting_ = std::make_unique<ParameterControl>(state, dbId, dbSlider_, dbSlider_, true);
    dbReadout_.addMouseListener(dbRouting_.get(), false);
    addAndMakeVisible(*dbRouting_);

    freqRouting_ = std::make_unique<ParameterControl>(state, freqId, *this, freqMirror_, false);
    freqReadout_.addMouseListener(freqRouting_.get(), false);
    addAndMakeVisible(*freqRouting_);

    dbAttachment_ = std::make_unique<juce::ParameterAttachment>(*dbParam, [this](float value) {
        dbSlider_.setValue(value, juce::dontSendNotification);
        displayedDb_ = value;
        updateReadoutText();
    });
    freqAttachment_ = std::make_unique<juce::ParameterAttachment>(*freqParam, [this](float value) {
        freqHz_ = value;
        freqMirror_.setValue(value, juce::dontSendNotification);
        updateFreqReadoutText();
        repaint();
    });

    dbSlider_.onDragStart = [this] { dbAttachment_->beginGesture(); };
    dbSlider_.onDragEnd = [this] { dbAttachment_->endGesture(); };
    dbSlider_.onValueChange = [this] {
        const float value = static_cast<float>(dbSlider_.getValue());
        static_cast<OpenEdmKickProcessor&>(state_.processor).routing().manualValue(dbId_, value);
        displayedDb_ = value;
        updateReadoutText();
        if (dbSlider_.isMouseButtonDown()) {
            dbAttachment_->setValueAsPartOfGesture(value);
            return;
        }
        dbAttachment_->setValueAsCompleteGesture(value);
    };
    dbReadout_.onTextChange = [this, dbParam] {
        const float parsed = dbParam->convertFrom0to1(dbParam->getValueForText(dbReadout_.getText()));
        static_cast<OpenEdmKickProcessor&>(state_.processor).routing().manualValue(dbId_, parsed);
        dbAttachment_->setValueAsCompleteGesture(parsed);
        updateReadoutText();
    };
    freqReadout_.onTextChange = [this, freqParam] {
        const float parsed = freqParam->convertFrom0to1(freqParam->getValueForText(freqReadout_.getText()));
        freqHz_ = parsed;
        static_cast<OpenEdmKickProcessor&>(state_.processor).routing().manualValue(freqId_, parsed);
        freqAttachment_->setValueAsCompleteGesture(parsed);
        updateFreqReadoutText();
        repaint();
    };

    dbAttachment_->sendInitialUpdate();
    freqAttachment_->sendInitialUpdate();
    startTimerHz(20);
}

void DualEqKnob::timerCallback() {
    auto& routing = static_cast<OpenEdmKickProcessor&>(state_.processor).routing();
    const float routedDb = routing.displayValue(dbId_);
    const float routedFreq = routing.displayValue(freqId_);
    if (std::abs(routedDb - displayedDb_) > 0.01f) {
        displayedDb_ = routedDb;
        updateReadoutText();
    }
    if (std::abs(routedFreq - freqHz_) > 0.1f) {
        freqHz_ = routedFreq;
        updateFreqReadoutText();
        repaint();
    }
}

void DualEqKnob::updateReadoutText() {
    auto* dbParam = state_.getParameter(dbId_);
    if (dbParam == nullptr) return;
    dbReadout_.setText(dbParam->getText(dbParam->convertTo0to1(displayedDb_), 32), juce::dontSendNotification);
}

void DualEqKnob::updateFreqReadoutText() {
    auto* freqParam = state_.getParameter(freqId_);
    if (freqParam == nullptr) return;
    const float norm = freqParam->convertTo0to1(freqHz_);
    freqReadout_.setText(freqParam->getText(norm, 16), juce::dontSendNotification);
}

juce::Rectangle<float> DualEqKnob::dialBounds() const {
    const auto middle = getLocalBounds().withTrimmedTop(16).withTrimmedBottom(kRoutingHeight + 4);
    const float size = static_cast<float>(std::min(58, std::min(middle.getWidth() - 56, middle.getHeight())));
    return juce::Rectangle<float>(static_cast<float>(middle.getX()) + 2.0f,
                                  middle.toFloat().getCentreY() - size * 0.5f,
                                  size, size);
}

juce::Rectangle<int> DualEqKnob::dbPillBounds() const {
    const auto middle = getLocalBounds().withTrimmedTop(16).withTrimmedBottom(kRoutingHeight + 4);
    const int left = juce::roundToInt(dialBounds().getRight()) + 3;
    const int width = std::max(0, middle.getRight() - left - 1);
    const int cy = middle.getCentreY();
    return { left, cy - 22, width, 20 };
}

juce::Rectangle<int> DualEqKnob::freqPillBounds() const {
    const auto middle = getLocalBounds().withTrimmedTop(16).withTrimmedBottom(kRoutingHeight + 4);
    const int left = juce::roundToInt(dialBounds().getRight()) + 3;
    const int width = std::max(0, middle.getRight() - left - 1);
    const int cy = middle.getCentreY();
    return { left, cy + 1, width, 20 };
}

bool DualEqKnob::hitsOuterRing(juce::Point<float> pos) const {
    const auto dial = dialBounds();
    const float dist = pos.getDistanceFrom(dial.getCentre());
    const float outerR = dial.getWidth() * 0.5f + 3.0f;
    const float innerR = dial.getWidth() * 0.30f;
    return (dist >= innerR && dist <= outerR) || freqPillBounds().toFloat().contains(pos);
}

void DualEqKnob::paint(juce::Graphics& g) {
    const auto dial = dialBounds();
    const auto centre = dial.getCentre();
    const float outerR = dial.getWidth() * 0.5f - 2.0f;
    constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;

    auto* freqParam = state_.getParameter(freqId_);
    const float norm = freqParam != nullptr ? freqParam->convertTo0to1(freqHz_) : 0.5f;
    const float angle = startAngle + norm * (endAngle - startAngle);
    const auto ringColour = (hoveringFreq_ || draggingFreq_) ? freqAccent_.brighter(0.25f) : freqAccent_;

    juce::Path base;
    base.addCentredArc(centre.x, centre.y, outerR, outerR, 0.0f, startAngle, endAngle, true);
    g.setColour(theme::kTrack);
    g.strokePath(base, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, outerR, outerR, 0.0f, startAngle, angle, true);
    g.setColour(ringColour);
    g.strokePath(value, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Mark root, +1 octave, and +2 octave positions on the outer note ring.
    g.setColour(theme::kTextDim.withAlpha(0.55f));
    for (const float markNorm : { 0.0f, 0.5f, 1.0f }) {
        const float markAngle = startAngle + markNorm * (endAngle - startAngle);
        const float innerTick = outerR - 3.0f;
        const float outerTick = outerR + 3.0f;
        g.drawLine(centre.x + innerTick * std::sin(markAngle), centre.y - innerTick * std::cos(markAngle),
                   centre.x + outerTick * std::sin(markAngle), centre.y - outerTick * std::cos(markAngle), 1.1f);
    }

    const float dotX = centre.x + outerR * std::sin(angle);
    const float dotY = centre.y - outerR * std::cos(angle);
    g.setColour(theme::kText);
    g.fillEllipse(dotX - 2.3f, dotY - 2.3f, 4.6f, 4.6f);

    const auto freqPill = freqPillBounds().toFloat();
    g.setColour(theme::kInk.brighter(hoveringFreq_ || draggingFreq_ ? 0.1f : 0.04f));
    g.fillRoundedRectangle(freqPill, 4.0f);
    const bool freqFocused = freqReadout_.hasKeyboardFocus(true);
    g.setColour(freqFocused ? theme::kViolet : ringColour.withAlpha(hoveringFreq_ || draggingFreq_ ? 0.9f : 0.45f));
    g.drawRoundedRectangle(freqPill.reduced(0.5f), 4.0f, freqFocused ? 1.5f : 1.0f);

    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("dB", dbRouting_->getBounds().getX(), dbRouting_->getY() - 12, dbRouting_->getWidth(), 14, juce::Justification::centred);
    g.drawText("Note", freqRouting_->getBounds().getX(), freqRouting_->getY() - 12, freqRouting_->getWidth(), 14, juce::Justification::centred);
}

void DualEqKnob::resized() {
    auto area = getLocalBounds();
    auto bottom = area.removeFromBottom(kRoutingHeight);
    const int centerX = bottom.getCentreX();
    dbRouting_->setBounds(centerX - 42, bottom.getY(), 40, kRoutingHeight);
    freqRouting_->setBounds(centerX + 2, bottom.getY(), 40, kRoutingHeight);
    title_.setBounds(area.removeFromTop(16));
    const auto dial = dialBounds();
    const float innerSize = dial.getWidth() * 0.72f;
    dbSlider_.setBounds(juce::Rectangle<float>(innerSize, innerSize).withCentre(dial.getCentre()).toNearestInt());
    dbReadout_.setBounds(dbPillBounds());
    freqReadout_.setBounds(freqPillBounds());
}

void DualEqKnob::mouseMove(const juce::MouseEvent& event) {
    const bool over = hitsOuterRing(event.position);
    if (over != hoveringFreq_) {
        hoveringFreq_ = over;
        repaint();
    }
}

void DualEqKnob::mouseExit(const juce::MouseEvent&) {
    if (!hoveringFreq_) return;
    hoveringFreq_ = false;
    repaint();
}

void DualEqKnob::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isPopupMenu() || !hitsOuterRing(event.position)) return;
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr) return;
    draggingFreq_ = true;
    dragStartNorm_ = param->convertTo0to1(freqHz_);
    dragStartY_ = event.position.y;
    freqAttachment_->beginGesture();
    repaint();
}

void DualEqKnob::mouseDrag(const juce::MouseEvent& event) {
    if (!draggingFreq_) return;
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr) return;
    const float norm = std::clamp(dragStartNorm_ + (dragStartY_ - event.position.y) / 140.0f, 0.0f, 1.0f);
    const float nextHz = param->convertFrom0to1(norm);
    freqHz_ = nextHz;
    static_cast<OpenEdmKickProcessor&>(state_.processor).routing().manualValue(freqId_, nextHz);
    freqAttachment_->setValueAsPartOfGesture(nextHz);
    updateFreqReadoutText();
    repaint();
}

void DualEqKnob::mouseUp(const juce::MouseEvent&) {
    if (!draggingFreq_) return;
    draggingFreq_ = false;
    freqAttachment_->endGesture();
    repaint();
}

void DualEqKnob::mouseDoubleClick(const juce::MouseEvent& event) {
    if (!hitsOuterRing(event.position)) return;
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr) return;
    const float defaultHz = param->convertFrom0to1(param->getDefaultValue());
    freqHz_ = defaultHz;
    static_cast<OpenEdmKickProcessor&>(state_.processor).routing().manualValue(freqId_, defaultHz);
    freqAttachment_->setValueAsCompleteGesture(defaultHz);
    updateFreqReadoutText();
    repaint();
}

void DualEqKnob::stepFreqSemitones(int semitones) {
    auto* param = state_.getParameter(freqId_);
    if (param == nullptr || semitones == 0) return;
    constexpr float kSemitoneStep = 1.0f / 24.0f;
    const float nextNorm = std::clamp(param->convertTo0to1(freqHz_) + static_cast<float>(semitones) * kSemitoneStep, 0.0f, 1.0f);
    const float nextHz = param->convertFrom0to1(nextNorm);
    freqHz_ = nextHz;
    static_cast<OpenEdmKickProcessor&>(state_.processor).routing().manualValue(freqId_, nextHz);
    freqAttachment_->setValueAsCompleteGesture(nextHz);
    updateFreqReadoutText();
    repaint();
}

void DualEqKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    if (!hitsOuterRing(event.position) || wheel.deltaY == 0.0f) return;
    stepFreqSemitones(wheel.deltaY > 0.0f ? 1 : -1);
}

void DualEqKnob::setTooltip(const juce::String& text) {
    dbSlider_.setTooltip(text);
    dbReadout_.setTooltip(text);
    freqReadout_.setTooltip(text);
    title_.setTooltip(text);
}

bool DualEqKnob::FreqReadout::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey) {
        owner_.stepFreqSemitones(1);
        return true;
    }
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey) {
        owner_.stepFreqSemitones(-1);
        return true;
    }
    if (key == juce::KeyPress::returnKey) {
        showEditor();
        return true;
    }
    return juce::Label::keyPressed(key);
}

void DualEqKnob::FreqReadout::mouseMove(const juce::MouseEvent& event) { owner_.mouseMove(event.getEventRelativeTo(&owner_)); }
void DualEqKnob::FreqReadout::mouseExit(const juce::MouseEvent& event) { owner_.mouseExit(event.getEventRelativeTo(&owner_)); }
void DualEqKnob::FreqReadout::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();
    owner_.mouseDown(event.getEventRelativeTo(&owner_));
}
void DualEqKnob::FreqReadout::mouseDrag(const juce::MouseEvent& event) { owner_.mouseDrag(event.getEventRelativeTo(&owner_)); }
void DualEqKnob::FreqReadout::mouseUp(const juce::MouseEvent& event) { owner_.mouseUp(event.getEventRelativeTo(&owner_)); }
void DualEqKnob::FreqReadout::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) {
    owner_.mouseWheelMove(event.getEventRelativeTo(&owner_), wheel);
}

Choice::Choice(Apvts& state, const juce::String& parameterId, const juce::String& title, juce::Colour accent)
    : title_({}, title) {
    box_.setName(title);
    box_.setTitle(title);
    box_.setComponentID(parameterId);
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId)))
        box_.addItemList(parameter->choices, 1);
    box_.setColour(juce::ComboBox::outlineColourId, accent);
    box_.setColour(juce::ComboBox::backgroundColourId, theme::kSteel);
    title_.setFont(juce::FontOptions(kLabelFont, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, theme::kText);
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
    routing_->setBounds(area.removeFromBottom(kRoutingHeight).withSizeKeepingCentre(std::min(area.getWidth(), 82), kRoutingHeight));
    if (title_.getText().isNotEmpty()) title_.setBounds(area.removeFromTop(kLabelHeight));
    box_.setBounds(area.withHeight(std::min(40, area.getHeight())).withY(area.getY() + (area.getHeight() - std::min(40, area.getHeight())) / 2));
}

Toggle::Toggle(Apvts& state, const juce::String& parameterId, const juce::String& text, juce::Colour accent,
               const juce::String& partnerId)
    : button_(text) {
    button_.setTitle(text);
    button_.setComponentID(parameterId);
    button_.setClickingTogglesState(true);
    button_.setColour(juce::TextButton::buttonOnColourId, accent);
    addAndMakeVisible(button_);
    routing_ = std::make_unique<ParameterControl>(state, parameterId, *this, button_);
    routing_->mirrorPartner(partnerId);
    addAndMakeVisible(*routing_);
    attachment_ = std::make_unique<juce::ParameterAttachment>(*state.getParameter(parameterId),
        [this](float value) { button_.setToggleState(value >= 0.5f, juce::dontSendNotification); });
    button_.onClick = [this, &state, parameterId, partnerId] {
        const float value = button_.getToggleState() ? 1.0f : 0.0f;
        auto& routing = static_cast<OpenEdmKickProcessor&>(state.processor).routing();
        routing.manualValue(parameterId, value);
        auto* partner = state.getParameter(partnerId);
        if (partner == nullptr || value > 0.5f) {
            attachment_->setValueAsCompleteGesture(value);
            return;
        }
        // Switching a merged switch off turns off both parameters in one undo step.
        attachment_->beginGesture();
        attachment_->setValueAsPartOfGesture(value);
        routing.manualValue(partnerId, 0.0f);
        partner->beginChangeGesture();
        partner->setValueNotifyingHost(0.0f);
        partner->endChangeGesture();
        attachment_->endGesture();
    };
    attachment_->sendInitialUpdate();
}
void Toggle::resized() {
    auto area = getLocalBounds();
    routing_->setBounds(area.removeFromBottom(kRoutingHeight).withSizeKeepingCentre(std::min(area.getWidth(), 82), kRoutingHeight));
    button_.setBounds(area.withSizeKeepingCentre(area.getWidth(), std::min(40, area.getHeight())));
}
void Toggle::setTooltip(const juce::String& text) { button_.setTooltip(text); }

void layoutRow(juce::Rectangle<int> area, std::initializer_list<juce::Component*> components, int gap) {
    const int count = static_cast<int>(std::count_if(components.begin(), components.end(), [](const auto* control) { return control->isVisible(); }));
    if (count <= 0) return;
    const int safeGap = std::max(0, gap);
    const int width = std::max(0, (area.getWidth() - safeGap * (count - 1)) / count);
    for (auto* component : components) {
        if (!component->isVisible()) continue;
        component->setBounds(area.removeFromLeft(width));
        area.removeFromLeft(safeGap);
    }
}

void drawCard(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent, bool inset) {
    const auto r = area.toFloat();
    g.setColour(inset ? theme::kInk.brighter(0.04f) : theme::kSteel);
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(accent);
    g.fillEllipse(r.getX() + 15, r.getY() + 21, 7, 7);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText(title, area.getX() + 33, area.getY() + 5, area.getWidth() - 160, 38,
               juce::Justification::centredLeft);
}

void drawSmallLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, juce::Colour colour) {
    g.setColour(colour);
    g.setFont(juce::FontOptions(13.5f));
    g.drawText(text, area, juce::Justification::centredLeft);
}

} // namespace openedmkick::ui
