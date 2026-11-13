#include "Panels.h"

#include "Presets.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <ranges>

namespace openedmkick::ui {
namespace {
namespace core = openedmkick::core;
namespace params = openedmkick::params;
namespace theme = openedmkick::theme;

constexpr int kCardGap = 10;
constexpr int kCardHeader = 32;

void setControlsVisible(std::initializer_list<juce::Component*> controls, bool visible) {
    for (auto* control : controls) control->setVisible(visible);
}

void placeRow(juce::Rectangle<int> card, int row, int rowCount,
              std::initializer_list<juce::Component*> controls) {
    const int innerTop = card.getY() + kCardHeader + 5;
    const int innerHeight = std::max(0, card.getHeight() - kCardHeader - 10);
    const int rowHeight = innerHeight / std::max(1, rowCount);
    auto bounds = juce::Rectangle<int>(card.getX() + 8, innerTop + row * rowHeight,
                                        card.getWidth() - 16, rowHeight);
    layoutRow(bounds, controls, 3);
}

juce::String noteLabel(int note) {
    const auto name = core::noteName(note);
    return juce::String(name.data(), name.size()) + juce::String(core::noteOctave(note));
}

void styleSlotButton(juce::TextButton& button, int slot) {
    button.setButtonText(juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)));
    button.setComponentID("autoSlot" + juce::String(slot));
    button.setClickingTogglesState(false);
    button.setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
    button.setTooltip("Edit pattern scene " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + slot)) + ".");
}
} // namespace

KickWaveformView::KickWaveformView(OpenEdmKickProcessor& processor) : processor_(processor) {
    setOpaque(false);
    startTimerHz(3);
    timerCallback();
}

void KickWaveformView::timerCallback() {
    samples_ = processor_.previewKickWaveform();
    repaint();
}

void KickWaveformView::paint(juce::Graphics& g) {
    auto area = getLocalBounds().toFloat();
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(area, 12.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(area.reduced(0.5f), 12.0f, 1.0f);

    const int note = processor_.currentMidiNote();
    const float hz = core::noteFrequencyHz(note);
    auto label = getLocalBounds().reduced(16, 10);
    auto title = label.removeFromTop(24);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("MIDI TRACKING", title.removeFromLeft(150), juce::Justification::centredLeft);
    g.setColour(theme::kBrass);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(noteLabel(note) + "   " + juce::String(hz, hz < 100.0f ? 1 : 0) + " Hz",
               title.removeFromLeft(180), juce::Justification::centredLeft);
    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(9.5f));
    g.drawText("Incoming note sets the kick root and octave", title, juce::Justification::centredRight);

    auto plot = getLocalBounds().reduced(16, 10).withTrimmedTop(30).toFloat();
    g.setColour(theme::kInk.withAlpha(0.76f));
    g.fillRoundedRectangle(plot, 7.0f);
    g.setColour(theme::kEdge.withAlpha(0.85f));
    g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()), plot.getX() + 8.0f, plot.getRight() - 8.0f);
    for (int division = 1; division < 8; ++division) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(division) / 8.0f;
        g.setColour(theme::kEdge.withAlpha(0.32f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY() + 4.0f, plot.getBottom() - 4.0f);
    }
    if (samples_.size() < 2) return;

    juce::Path wave;
    const float mid = plot.getCentreY();
    const float amplitude = plot.getHeight() * 0.42f;
    const size_t count = std::min<size_t>(samples_.size(), 1200);
    for (size_t i = 0; i < count; ++i) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(count - 1);
        const size_t sampleIndex = i * (samples_.size() - 1) / (count - 1);
        const float y = mid - samples_[sampleIndex] * amplitude;
        if (i == 0) wave.startNewSubPath(x, y);
        else wave.lineTo(x, y);
    }
    g.setColour(theme::kViolet.withAlpha(0.22f));
    g.strokePath(wave, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(theme::kViolet);
    g.strokePath(wave, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

KickSynthPanel::KickSynthPanel(OpenEdmKickProcessor& processor)
    : sweepOctaves_(processor.parameters, params::kSweepOctaves, "Dive range", theme::kViolet),
      sweepMs_(processor.parameters, params::kSweepMs, "Dive time", theme::kViolet),
      sweepShape_(processor.parameters, params::kSweepShape, "Dive curve", theme::kViolet),
      snapOctaves_(processor.parameters, params::kSnapOctaves, "Snap range", theme::kViolet),
      snapMs_(processor.parameters, params::kSnapMs, "Snap time", theme::kViolet),
      bodyTone_(processor.parameters, params::kBodyTone, "Harmonics", theme::kBrass),
      bodyDrive_(processor.parameters, params::kBodyDrive, "Body drive", theme::kBrass),
      saturation_(processor.parameters, params::kSaturation, "Saturation", theme::kBrass),
      knockHz_(processor.parameters, params::kKnockHz, "Knock freq", theme::kBrass),
      knockDb_(processor.parameters, params::kKnockDb, "Knock", theme::kBrass),
      grit_(processor.parameters, params::kGrit, "Tail grit", theme::kBrass),
      toneHz_(processor.parameters, params::kToneHz, "Body tone", theme::kBrass),
      subLevel_(processor.parameters, params::kSubLevel, "Sub", theme::kBrass),
      startPhase_(processor.parameters, params::kStartPhase, "Start phase", theme::kBrass),
      clickType_(processor.parameters, params::kClickType, "Transient", theme::kCoral),
      clickLevel_(processor.parameters, params::kClickLevel, "Level", theme::kCoral),
      clickHz_(processor.parameters, params::kClickHz, "Tone", theme::kCoral),
      clickMs_(processor.parameters, params::kClickMs, "Length", theme::kCoral),
      attack_(processor.parameters, params::kAttackMs, "Attack", theme::kBrass),
      hold_(processor.parameters, params::kHoldMs, "Hold", theme::kBrass),
      decay_(processor.parameters, params::kDecayMs, "Decay", theme::kBrass),
      decayShape_(processor.parameters, params::kDecayShape, "Decay curve", theme::kBrass),
      lowCut_(processor.parameters, params::kLowCutHz, "Low cut", theme::kBrass),
      clip_(processor.parameters, params::kClip, "Clip", theme::kBrass),
      waveform_(processor) {
    for (juce::Component* component : std::array<juce::Component*, 25>{
             &sweepOctaves_, &sweepMs_, &sweepShape_, &snapOctaves_, &snapMs_,
             &bodyTone_, &bodyDrive_, &saturation_, &knockHz_, &knockDb_, &grit_, &toneHz_, &subLevel_, &startPhase_,
             &clickType_, &clickLevel_, &clickHz_, &clickMs_, &attack_, &hold_, &decay_, &decayShape_, &lowCut_, &clip_, &waveform_})
        addAndMakeVisible(component);

    sweepOctaves_.setTooltip("How far above the played MIDI note the opening of the kick starts.");
    sweepMs_.setTooltip("Time for the pitch dive to reach the incoming MIDI note.");
    bodyTone_.setTooltip("Add harmonics to give the kick a more forceful, audible body.");
    knockDb_.setTooltip("A resonant mid-bass knock for more impact on smaller speakers.");
    subLevel_.setTooltip("Blend a clean fundamental under the driven body.");
    clickLevel_.setTooltip("Add a short beater transient ahead of the body.");
    setAdvancedMode(false);
}

void KickSynthPanel::setVisibleControls(std::initializer_list<juce::Component*> controls, bool visible) {
    setControlsVisible(controls, visible);
}

void KickSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setVisibleControls({ &sweepShape_, &snapMs_, &saturation_, &knockHz_, &grit_, &toneHz_, &startPhase_,
                         &clickType_, &clickMs_, &attack_, &decayShape_, &lowCut_ }, advanced);
    setVisibleControls({ &sweepOctaves_, &sweepMs_, &snapOctaves_, &bodyTone_, &bodyDrive_, &knockDb_, &subLevel_,
                         &clickLevel_, &clickHz_, &hold_, &decay_, &clip_, &waveform_ }, true);
    resized();
    repaint();
}

void KickSynthPanel::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    drawSmallLabel(g, area.removeFromTop(20), "Sculpt a tuned impact", theme::kTextDim);
    const int waveHeight = std::clamp(getHeight() / 6, 130, 160);
    area.removeFromTop(2);
    drawCard(g, area.removeFromTop(waveHeight), "Played note", theme::kViolet);
    area.removeFromTop(kCardGap);
    const int cardHeight = (area.getHeight() - kCardGap) / 2;
    auto top = area.removeFromTop(cardHeight);
    auto bottom = area;
    const int cardWidth = (top.getWidth() - kCardGap) / 2;
    drawCard(g, top.removeFromLeft(cardWidth), "Dive", theme::kViolet);
    top.removeFromLeft(kCardGap);
    drawCard(g, top, "Body", theme::kBrass);
    drawCard(g, bottom.removeFromLeft(cardWidth), "Transient", theme::kCoral);
    bottom.removeFromLeft(kCardGap);
    drawCard(g, bottom, "Tail", theme::kBrass);

    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(9.5f));
    g.drawText("The pitch lands on the exact note and octave from MIDI.",
               getLocalBounds().reduced(3).removeFromBottom(15), juce::Justification::centredRight);
}

void KickSynthPanel::resized() {
    auto area = getLocalBounds().withTrimmedTop(22).withTrimmedBottom(18);
    const int waveHeight = std::clamp(getHeight() / 6, 130, 160);
    waveform_.setBounds(area.removeFromTop(waveHeight).reduced(1));
    area.removeFromTop(kCardGap);
    const int cardHeight = (area.getHeight() - kCardGap) / 2;
    auto top = area.removeFromTop(cardHeight);
    auto bottom = area;
    const int cardWidth = (top.getWidth() - kCardGap) / 2;
    const auto dive = top.removeFromLeft(cardWidth); top.removeFromLeft(kCardGap);
    const auto body = top;
    const auto transient = bottom.removeFromLeft(cardWidth); bottom.removeFromLeft(kCardGap);
    const auto tail = bottom;

    placeRow(dive, 0, 2, { &sweepOctaves_, &sweepMs_, &snapOctaves_ });
    placeRow(dive, 1, 2, { &sweepShape_, &snapMs_ });
    placeRow(body, 0, 3, { &bodyTone_, &bodyDrive_, &knockDb_ });
    placeRow(body, 1, 3, { &saturation_, &knockHz_, &subLevel_ });
    placeRow(body, 2, 3, { &grit_, &toneHz_, &startPhase_ });
    placeRow(transient, 0, 2, { &clickLevel_, &clickHz_ });
    placeRow(transient, 1, 2, { &clickType_, &clickMs_ });
    placeRow(tail, 0, 2, { &hold_, &decay_, &clip_ });
    placeRow(tail, 1, 2, { &attack_, &decayShape_, &lowCut_ });
}

PianoRollView::PianoRollView(OpenEdmKickProcessor& processor) : processor_(processor) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(12);
}

juce::Rectangle<float> PianoRollView::gridArea() const {
    return getLocalBounds().toFloat().reduced(13.0f, 10.0f).withTrimmedTop(23.0f).withTrimmedBottom(4.0f);
}

void PianoRollView::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kInk.brighter(0.035f));
    g.fillRoundedRectangle(bounds, 9.0f);
    const auto grid = gridArea();
    const auto settings = processor_.currentAutoSlotSettings();
    const int bars = std::max(1, settings.bars);
    const int totalSteps = bars * core::kStepsPerBar64;
    const int gridStep = core::kGridStepSizes64[static_cast<size_t>(std::clamp(settings.gridResolution, 0, 4))];

    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("KICK LANE · CLICK TO ADD OR REMOVE A HIT", getLocalBounds().reduced(14, 5).removeFromTop(17),
               juce::Justification::centredLeft);

    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(grid, 5.0f);
    for (int step = 0; step <= totalSteps; step += gridStep) {
        const float x = grid.getX() + grid.getWidth() * static_cast<float>(step) / static_cast<float>(totalSteps);
        const bool barLine = step % core::kStepsPerBar64 == 0;
        g.setColour(barLine ? theme::kEdge : theme::kEdge.withAlpha(0.28f));
        g.drawVerticalLine(juce::roundToInt(x), grid.getY() + 3.0f, grid.getBottom() - 3.0f);
        if (barLine && step < totalSteps) {
            g.setColour(theme::kTextDim.withAlpha(0.8f));
            g.setFont(juce::FontOptions(8.5f));
            const int bar = step / core::kStepsPerBar64 + 1;
            g.drawText("BAR " + juce::String(bar), static_cast<int>(x) + 4, static_cast<int>(grid.getY()) + 3, 46, 13,
                       juce::Justification::centredLeft);
        }
    }

    const auto notes = processor_.currentPianoRollNotes();
    for (const auto& note : notes) {
        if (note.step64 < 0 || note.step64 >= totalSteps) continue;
        const float x = grid.getX() + grid.getWidth() * static_cast<float>(note.step64) / static_cast<float>(totalSteps);
        const float w = std::max(4.0f, grid.getWidth() * static_cast<float>(std::max(1, note.length64)) / static_cast<float>(totalSteps));
        const float laneHeight = grid.getHeight() * 0.72f;
        const auto hit = juce::Rectangle<float>(x + 1.0f, grid.getY() + (grid.getHeight() - laneHeight) * 0.5f,
                                                 std::min(w - 1.0f, grid.getRight() - x), laneHeight);
        const auto accent = note.reverseCue ? theme::kCoral : theme::kViolet;
        g.setColour(accent.withAlpha(0.32f + note.velocity * 0.58f));
        g.fillRoundedRectangle(hit, 4.0f);
        g.setColour(theme::kText.withAlpha(0.52f));
        g.fillRoundedRectangle(hit.withHeight(2.0f).withY(hit.getY() + 3.0f), 1.0f);
    }

    const float phase = processor_.displayAutoPhase.load(std::memory_order_relaxed);
    const float head = grid.getX() + grid.getWidth() * std::clamp(phase, 0.0f, 1.0f);
    g.setColour(theme::kBrass);
    g.drawVerticalLine(juce::roundToInt(head), grid.getY(), grid.getBottom());
}

void PianoRollView::mouseDown(const juce::MouseEvent& event) {
    const auto grid = gridArea();
    if (!grid.contains(event.position)) return;
    const auto settings = processor_.currentAutoSlotSettings();
    const int totalSteps = std::max(1, settings.bars) * core::kStepsPerBar64;
    const int gridStep = core::kGridStepSizes64[static_cast<size_t>(std::clamp(settings.gridResolution, 0, 4))];
    const float normalized = std::clamp((event.position.x - grid.getX()) / grid.getWidth(), 0.0f, 0.99999f);
    const int step = (static_cast<int>(normalized * static_cast<float>(totalSteps)) / gridStep) * gridStep;
    processor_.togglePianoRollNote(step, gridStep);
}

AutoPianoRollPanel::AutoPianoRollPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      bypass_(processor.parameters, params::kAutoBypass, "Bypass", theme::kCoral),
      enabled_(processor.parameters, params::kAutoEnabled, "Auto mode", theme::kViolet),
      clockSync_(processor.parameters, params::kClockSync, "Host sync", theme::kBrass),
      timeSig_(processor.parameters, params::kTimeSignature, "Meter", theme::kBrass),
      bars_(processor.parameters, params::autoSlotId(0, params::kAutoBars), "Length", theme::kViolet),
      grid_(processor.parameters, params::autoSlotId(0, params::kAutoGrid), "Grid", theme::kViolet),
      fillPreset_(processor.parameters, params::autoSlotId(0, params::kAutoFillPreset), "Fill", theme::kViolet),
      tempo_(processor.parameters, params::kTempoBpm, "Tempo", theme::kBrass),
      tailShorten_(processor.parameters, params::autoSlotId(0, params::kAutoTailShorten), "Tail trim", theme::kBrass),
      crushSweep_(processor.parameters, params::autoSlotId(0, params::kAutoCrushSweep), "Crush rise", theme::kCoral),
      pianoRoll_(processor),
      slotWatcher_(*processor.parameters.getParameter(params::kAutoSlot),
                   [this](float value) { syncSelectedSlot(static_cast<int>(value)); }) {
    for (juce::Component* component : std::array<juce::Component*, 11>{
             &bypass_, &enabled_, &clockSync_, &timeSig_, &bars_, &grid_, &fillPreset_,
             &tempo_, &tailShorten_, &crushSweep_, &pianoRoll_}) addAndMakeVisible(component);
    for (int slot = 0; slot < core::kNumAutoSlots; ++slot) {
        auto& button = slotButtons_[static_cast<size_t>(slot)];
        styleSlotButton(button, slot);
        button.onClick = [this, slot] { selectSlot(slot); };
        addAndMakeVisible(button);
    }
    fillPreset_.setTooltip("Select a one-lane trance kick sequence.");
    slotWatcher_.sendInitialUpdate();
    setAdvancedMode(false);
}

void AutoPianoRollPanel::bindSlotControls() {
    bars_.bind(processor_.parameters, params::autoSlotId(selectedSlot_, params::kAutoBars));
    grid_.bind(processor_.parameters, params::autoSlotId(selectedSlot_, params::kAutoGrid));
    fillPreset_.bind(processor_.parameters, params::autoSlotId(selectedSlot_, params::kAutoFillPreset));
    tailShorten_.bind(processor_.parameters, params::autoSlotId(selectedSlot_, params::kAutoTailShorten));
    crushSweep_.bind(processor_.parameters, params::autoSlotId(selectedSlot_, params::kAutoCrushSweep));
}

void AutoPianoRollPanel::selectSlot(int slot) {
    const int safeSlot = std::clamp(slot, 0, core::kNumAutoSlots - 1);
    slotWatcher_.setValueAsCompleteGesture(static_cast<float>(safeSlot));
    syncSelectedSlot(safeSlot);
}

void AutoPianoRollPanel::syncSelectedSlot(int slot) {
    const int safeSlot = std::clamp(slot, 0, core::kNumAutoSlots - 1);
    if (safeSlot == selectedSlot_) return;
    selectedSlot_ = safeSlot;
    bindSlotControls();
    for (int i = 0; i < core::kNumAutoSlots; ++i)
        slotButtons_[static_cast<size_t>(i)].setToggleState(i == selectedSlot_, juce::dontSendNotification);
    repaint();
}

void AutoPianoRollPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    timeSig_.setVisible(advanced);
    tempo_.setVisible(advanced);
    tailShorten_.setVisible(advanced);
    crushSweep_.setVisible(advanced);
    resized();
    repaint();
}

void AutoPianoRollPanel::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    drawSmallLabel(g, area.removeFromTop(21), "Build a pattern in one tuned kick lane", theme::kTextDim);
    const int top = 35;
    drawCard(g, { 0, top, getWidth(), 155 }, "Sequence", theme::kViolet);
    drawCard(g, { 0, top + 166, getWidth(), getHeight() - top - 166 }, "Step editor", theme::kBrass);
}

void AutoPianoRollPanel::resized() {
    auto area = getLocalBounds().withTrimmedTop(39);
    auto controls = area.removeFromTop(155).reduced(12, 38);
    for (auto& button : slotButtons_) {
        button.setBounds(controls.removeFromLeft(44).withHeight(32));
        controls.removeFromLeft(5);
    }
    controls.removeFromTop(39);
    const int toggleWidth = 112;
    enabled_.setBounds(controls.removeFromLeft(toggleWidth).withHeight(34)); controls.removeFromLeft(7);
    bypass_.setBounds(controls.removeFromLeft(94).withHeight(34)); controls.removeFromLeft(7);
    clockSync_.setBounds(controls.removeFromLeft(112).withHeight(34)); controls.removeFromLeft(10);
    const int fieldWidth = std::max(90, controls.getWidth() / (advancedMode_ ? 5 : 3));
    if (advancedMode_) {
        timeSig_.setBounds(controls.removeFromLeft(fieldWidth).withHeight(44)); controls.removeFromLeft(8);
        tempo_.setBounds(controls.removeFromLeft(fieldWidth).withHeight(52)); controls.removeFromLeft(8);
    }
    bars_.setBounds(controls.removeFromLeft(fieldWidth).withHeight(44)); controls.removeFromLeft(8);
    grid_.setBounds(controls.removeFromLeft(fieldWidth).withHeight(44)); controls.removeFromLeft(8);
    fillPreset_.setBounds(controls.withHeight(44));

    area.removeFromTop(11);
    pianoRoll_.setBounds(area);
    const int footerHeight = advancedMode_ ? 74 : 0;
    if (advancedMode_) {
        auto footer = area.removeFromBottom(footerHeight).reduced(14, 3);
        tailShorten_.setBounds(footer.removeFromLeft(160)); footer.removeFromLeft(10);
        crushSweep_.setBounds(footer.removeFromLeft(160));
        pianoRoll_.setBounds(area);
    }
}

PeakMeter::PeakMeter(OpenEdmKickProcessor& processor) : processor_(processor) { startTimerHz(15); }
void PeakMeter::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(bounds, 4.0f);
    const float peak = processor_.displayPeak.load(std::memory_order_relaxed);
    auto fill = bounds.withWidth(bounds.getWidth() * std::clamp(peak, 0.0f, 1.0f));
    g.setColour(peak > 0.9f ? theme::kCoral : theme::kViolet);
    g.fillRoundedRectangle(fill, 4.0f);
}

EffectsPanel::EffectsPanel(OpenEdmKickProcessor& processor)
    : reverseMode_(processor.parameters, params::kReverseMode, "Reverse", theme::kCoral),
      reverseMix_(processor.parameters, params::kReverseMix, "Reverse mix", theme::kCoral),
      drive_(processor.parameters, params::kDrive, "Drive", theme::kBrass),
      crushRate_(processor.parameters, params::kCrushRate, "Sample rate", theme::kBrass),
      crushBits_(processor.parameters, params::kCrushBits, "Bit depth", theme::kBrass),
      crushMix_(processor.parameters, params::kCrushMix, "Crush mix", theme::kBrass),
      stutterRate_(processor.parameters, params::kStutterRate, "Repeat rate", theme::kViolet),
      stutterMix_(processor.parameters, params::kStutterMix, "Repeat mix", theme::kViolet),
      gateRate_(processor.parameters, params::kGateRate, "Gate rate", theme::kViolet),
      gateMix_(processor.parameters, params::kGateMix, "Gate depth", theme::kViolet),
      pumpRate_(processor.parameters, params::kPumpRate, "Filter rate", theme::kViolet),
      pumpMix_(processor.parameters, params::kPumpMix, "Filter depth", theme::kViolet),
      highPass_(processor.parameters, params::kHighPass, "High pass", theme::kBrass),
      lowPass_(processor.parameters, params::kLowPass, "Low pass", theme::kBrass),
      output_(processor.parameters, params::kOutput, "Output", theme::kBrass),
      ceiling_(processor.parameters, params::kCeiling, "Ceiling", theme::kBrass),
      limiter_(processor.parameters, params::kLimiter, "Limiter", theme::kBrass),
      masterBypass_(processor.parameters, params::kMasterBypass, "Bypass all", theme::kCoral),
      meter_(processor) {
    for (juce::Component* component : std::array<juce::Component*, 19>{
             &reverseMode_, &reverseMix_, &drive_, &crushRate_, &crushBits_, &crushMix_,
             &stutterRate_, &stutterMix_, &gateRate_, &gateMix_, &pumpRate_, &pumpMix_,
             &highPass_, &lowPass_, &output_, &ceiling_, &limiter_, &masterBypass_, &meter_})
        addAndMakeVisible(component);
    setAdvancedMode(false);
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setControlsVisible({ &crushRate_, &crushBits_, &crushMix_, &stutterRate_, &stutterMix_, &gateRate_, &gateMix_,
                 &pumpRate_, &pumpMix_, &highPass_, &lowPass_, &ceiling_ }, advanced);
    resized();
    repaint();
}

void EffectsPanel::paint(juce::Graphics& g) {
    drawSmallLabel(g, getLocalBounds().removeFromTop(21), "Motion and finish", theme::kTextDim);
    auto area = getLocalBounds().withTrimmedTop(28);
    const int gap = 10;
    const int cardWidth = (area.getWidth() - gap) / 2;
    const int cardHeight = (area.getHeight() - gap) / 2;
    drawCard(g, { area.getX(), area.getY(), cardWidth, cardHeight }, "Reverse and drive", theme::kCoral);
    drawCard(g, { area.getX() + cardWidth + gap, area.getY(), cardWidth, cardHeight }, "Texture", theme::kBrass);
    drawCard(g, { area.getX(), area.getY() + cardHeight + gap, cardWidth, cardHeight }, "Rhythm", theme::kViolet);
    drawCard(g, { area.getX() + cardWidth + gap, area.getY() + cardHeight + gap, cardWidth, cardHeight }, "Tone and output", theme::kBrass);
}

void EffectsPanel::resized() {
    auto area = getLocalBounds().withTrimmedTop(28);
    const int gap = 10;
    const int cardWidth = (area.getWidth() - gap) / 2;
    const int cardHeight = (area.getHeight() - gap) / 2;
    const auto reverse = juce::Rectangle<int>(area.getX(), area.getY(), cardWidth, cardHeight);
    const auto texture = juce::Rectangle<int>(area.getX() + cardWidth + gap, area.getY(), cardWidth, cardHeight);
    const auto rhythm = juce::Rectangle<int>(area.getX(), area.getY() + cardHeight + gap, cardWidth, cardHeight);
    const auto tone = juce::Rectangle<int>(area.getX() + cardWidth + gap, area.getY() + cardHeight + gap, cardWidth, cardHeight);

    placeRow(reverse, 0, 2, { &reverseMode_, &reverseMix_, &drive_ });
    placeRow(reverse, 1, 2, { &masterBypass_, &meter_ });
    placeRow(texture, 0, 2, { &crushRate_, &crushBits_ });
    placeRow(texture, 1, 2, { &crushMix_ });
    placeRow(rhythm, 0, 3, { &stutterRate_, &stutterMix_ });
    placeRow(rhythm, 1, 3, { &gateRate_, &gateMix_ });
    placeRow(rhythm, 2, 3, { &pumpRate_, &pumpMix_ });
    placeRow(tone, 0, 3, { &highPass_, &lowPass_ });
    placeRow(tone, 1, 3, { &output_, &ceiling_ });
    placeRow(tone, 2, 3, { &limiter_ });

    if (!advancedMode_) {
        reverseMode_.setVisible(true); reverseMix_.setVisible(true); drive_.setVisible(true);
        masterBypass_.setVisible(true); meter_.setVisible(true); output_.setVisible(true); limiter_.setVisible(true);
    }
}

} // namespace openedmkick::ui
