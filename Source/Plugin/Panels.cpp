#include "Panels.h"

#include "BinaryData.h"
#include "Presets.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace openedmkick::ui {
namespace {

namespace core = openedmkick::core;
namespace params = openedmkick::params;

constexpr std::array<int, 6> kPitchRows{12, 7, 5, 3, 1, 0};
constexpr std::array<const char*, 6> kPitchNames{"+12", "+7", "+5", "+3", "+1", "ROOT"};

bool isSharp(int note) noexcept {
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

int section(params::Section value) noexcept { return static_cast<int>(value); }

} // namespace

KickWaveformView::KickWaveformView(OpenEdmKickProcessor& processor) : processor_(processor) {
    startTimerHz(4);
    timerCallback();
}

void KickWaveformView::timerCallback() {
    const auto settings = processor_.currentSynthSettings();
    const bool locked = processor_.isKickLockedIn();
    const float highPass = processor_.parameters.getRawParameterValue(params::kHighPass)->load();
    const float lowPass = processor_.parameters.getRawParameterValue(params::kLowPass)->load();
    if (!hasCache_ || !(settings == cachedSettings_) || locked != cachedLocked_
        || std::abs(highPass - cachedHighPass_) > 0.1f || std::abs(lowPass - cachedLowPass_) > 0.1f) {
        samples_ = processor_.previewKickWaveform();
        cachedSettings_ = settings;
        cachedLocked_ = locked;
        cachedHighPass_ = highPass;
        cachedLowPass_ = lowPass;
        hasCache_ = true;
    }
    repaint();
}

void KickWaveformView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);
    drawSmallLabel(g, getLocalBounds().reduced(12).removeFromTop(16), "LIVE KICK SHAPE", theme::kCyan);

    const auto plot = bounds.reduced(12.0f, 26.0f);
    const float mid = plot.getCentreY();
    g.setColour(theme::kTrack.withAlpha(0.6f));
    g.drawHorizontalLine(juce::roundToInt(mid), plot.getX(), plot.getRight());
    for (int i = 1; i < 8; ++i) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / 8.0f;
        g.setColour(theme::kEdge.withAlpha(i % 2 == 0 ? 0.8f : 0.35f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
    }
    if (samples_.size() < 2) return;

    juce::Path wave;
    constexpr int points = 420;
    for (int i = 0; i < points; ++i) {
        const float phase = static_cast<float>(i) / static_cast<float>(points - 1);
        const auto index = std::min(samples_.size() - 1, static_cast<size_t>(phase * static_cast<float>(samples_.size() - 1)));
        const float value = std::clamp(samples_[index], -1.0f, 1.0f);
        const float x = plot.getX() + phase * plot.getWidth();
        const float y = mid - value * plot.getHeight() * 0.45f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(theme::kCyan.withAlpha(0.95f));
    g.strokePath(wave, juce::PathStrokeType(1.5f));
    const auto synth = processor_.currentSynthSettings();
    const float fundamental = core::fundamentalHzForNote(synth.note, synth.octave, synth.fineCents);
    const float start = std::max(fundamental + 20.0f, synth.startPitchHz);
    if (!processor_.isKickLockedIn() && start > fundamental + 5.0f) {
        juce::Path pitch;
        constexpr int pitchPoints = 100;
        const float tailMs = std::clamp(synth.tailLengthMs, 15.0f, 2000.0f);
        const float pitchRange = std::log(start / fundamental);
        std::ranges::for_each(std::views::iota(0, pitchPoints), [&](int i) {
            const float phase = static_cast<float>(i) / static_cast<float>(pitchPoints - 1);
            const float progress = phase * tailMs / std::max(1.0f, synth.pitchDecayMs);
            const float envelope = core::pitchSweepGain(progress, synth.pitchCurve);
            const float hz = fundamental + (start - fundamental) * envelope;
            const float normalized = std::clamp(std::log(std::max(fundamental, hz) / fundamental) / pitchRange, 0.0f, 1.0f);
            const float x = plot.getX() + phase * plot.getWidth();
            const float y = plot.getY() + 2.0f + (1.0f - normalized) * plot.getHeight() * 0.34f;
            if (i == 0) pitch.startNewSubPath(x, y); else pitch.lineTo(x, y);
        });
        g.setColour(theme::kAmber.withAlpha(0.95f));
        g.strokePath(pitch, juce::PathStrokeType(1.5f));
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("PITCH", getLocalBounds().reduced(12).removeFromTop(15), juce::Justification::centredRight);
    }
    const float frequency = core::fundamentalHzForNote(synth.note, synth.octave, synth.fineCents);
    const juce::String noteName(core::kNoteNames[static_cast<size_t>(std::clamp(synth.note, 0, 11))].data());
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.setColour(theme::kAmber);
    g.drawText(noteName + juce::String(synth.octave) + "  " + juce::String(frequency, 1) + " Hz",
               getLocalBounds().reduced(12).removeFromBottom(15), juce::Justification::centredRight);
}

KickSynthPanel::KickSynthPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      triggerMode_(processor.parameters, params::kMidiMode, "MIDI", theme::kCyan),
      octave_(processor.parameters, params::kOctave, "OCTAVE", theme::kCyan),
      fineTune_(processor.parameters, params::kFineTune, "FINE", theme::kCyan),
      startPitch_(processor.parameters, params::kStartPitch, "START", theme::kCyan),
      pitchDecay_(processor.parameters, params::kPitchDecay, "DECAY", theme::kCyan),
      pitchCurve_(processor.parameters, params::kPitchCurve, "CURVE", theme::kCyan),
      bodyTone_(processor.parameters, params::kBodyTone, "BODY TONE", theme::kAmber),
      attack_(processor.parameters, params::kAmpAttack, "ATTACK", theme::kAmber),
      decay_(processor.parameters, params::kAmpDecay, "DECAY", theme::kAmber),
      sustain_(processor.parameters, params::kAmpSustain, "SUSTAIN", theme::kAmber),
      release_(processor.parameters, params::kAmpRelease, "RELEASE", theme::kAmber),
      tailLength_(processor.parameters, params::kTailLength, "TAIL", theme::kAmber),
      tailCurve_(processor.parameters, params::kTailCurve, "TAIL CURVE", theme::kAmber),
      punch_(processor.parameters, params::kPunch, "PUNCH", theme::kAmber),
      subDrive_(processor.parameters, params::kSubDrive, "BODY DRIVE", theme::kAmber),
      lockIn_(processor.parameters, params::kLockIn, "LOCK WAV", theme::kCoral),
      clickLevel_(processor.parameters, params::kClickLevel, "LEVEL", theme::kCoral),
      clickFreq_(processor.parameters, params::kClickFreq, "FREQ", theme::kCoral),
      snareLevel_(processor.parameters, params::kSnareLevel, "SNARE", theme::kCyan),
      snareTone_(processor.parameters, params::kSnareTone, "TONE", theme::kCyan),
      snareDecay_(processor.parameters, params::kSnareDecay, "DECAY", theme::kCyan),
      clapLevel_(processor.parameters, params::kClapLevel, "CLAP", theme::kAmber),
      clapSpread_(processor.parameters, params::kClapSpread, "SPREAD", theme::kAmber),
      clapTone_(processor.parameters, params::kClapTone, "TONE", theme::kAmber),
      clapDecay_(processor.parameters, params::kClapDecay, "DECAY", theme::kAmber),
      hatLevel_(processor.parameters, params::kHatLevel, "HAT", theme::kCoral),
      hatTone_(processor.parameters, params::kHatTone, "TONE", theme::kCoral),
      hatDecay_(processor.parameters, params::kHatDecay, "DECAY", theme::kCoral),
      waveform_(processor),
      tuningLock_("Tuning", theme::kCyan, [&processor] { return processor.isSectionLocked(section(params::Section::tuning)); },
                  [&processor](bool on) { processor.setSectionLocked(section(params::Section::tuning), on); }),
      pitchLock_("Pitch", theme::kCyan, [&processor] { return processor.isSectionLocked(section(params::Section::pitch)); },
                 [&processor](bool on) { processor.setSectionLocked(section(params::Section::pitch), on); }),
      bodyLock_("Body", theme::kAmber, [&processor] { return processor.isSectionLocked(section(params::Section::bodyEnvelope)); },
                [&processor](bool on) { processor.setSectionLocked(section(params::Section::bodyEnvelope), on); }),
      transientLock_("Transient", theme::kCoral, [&processor] { return processor.isSectionLocked(section(params::Section::transient)); },
                     [&processor](bool on) { processor.setSectionLocked(section(params::Section::transient), on); }),
      layersLock_("Layers", theme::kCyan, [&processor] { return processor.isSectionLocked(section(params::Section::layers)); },
                  [&processor](bool on) { processor.setSectionLocked(section(params::Section::layers), on); }),
      noteWatcher_(*processor.parameters.getParameter(params::kNote), [this](float note) { updateNoteButtons(note); }) {
    for (int i = 0; i < core::kNumNotes; ++i) {
        auto& button = noteButtons_[static_cast<size_t>(i)];
        button.setButtonText(juce::String(core::kNoteNames[static_cast<size_t>(i)].data()));
        button.setComponentID("note_" + juce::String(i));
        button.setColour(juce::TextButton::buttonOnColourId, theme::kCyan);
        button.setColour(juce::TextButton::buttonColourId, isSharp(i) ? theme::kInk : theme::kText);
        button.setColour(juce::TextButton::textColourOffId, isSharp(i) ? theme::kText : theme::kInk);
        button.setColour(juce::TextButton::textColourOnId, theme::kInk);
        button.onClick = [this, i] { selectNote(i); };
        addAndMakeVisible(button);
    }
    auditionButton_.setComponentID("audition_trig");
    auditionButton_.setColour(juce::TextButton::buttonOnColourId, theme::kCyan);
    auditionButton_.setTooltip("Audition the tuned kick. This also works while Auto Mode is running.");
    auditionButton_.onClick = [&processor] { processor.triggerAudition(); };
    triggerMode_.setTooltip("Fixed plays the tuned root. Note Tracking follows incoming MIDI pitches.");
    lockIn_.setTooltip("Freeze the current synthesized hit as a lossless internal WAV.");
    for (juce::Component* child : std::array<juce::Component*, 29>{
             &auditionButton_, &triggerMode_, &octave_, &fineTune_, &startPitch_, &pitchDecay_, &pitchCurve_,
             &bodyTone_, &attack_, &decay_, &sustain_, &release_, &tailLength_, &tailCurve_, &punch_, &subDrive_,
             &lockIn_, &clickLevel_, &clickFreq_, &snareLevel_, &snareTone_, &snareDecay_, &clapLevel_, &clapSpread_,
             &clapTone_, &clapDecay_, &hatLevel_, &hatTone_, &hatDecay_}) addAndMakeVisible(child);
    addAndMakeVisible(waveform_);
    addAndMakeVisible(tuningLock_); addAndMakeVisible(pitchLock_); addAndMakeVisible(bodyLock_);
    addAndMakeVisible(transientLock_); addAndMakeVisible(layersLock_);
    noteWatcher_.sendInitialUpdate();
    setAdvancedMode(false);
}

void KickSynthPanel::selectNote(int index) {
    auto* parameter = processor_.parameters.getParameter(params::kNote);
    if (parameter != nullptr) parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index)));
    processor_.triggerAudition();
}

void KickSynthPanel::updateNoteButtons(float value) {
    const int active = std::clamp(static_cast<int>(std::round(value)), 0, core::kNumNotes - 1);
    for (int i = 0; i < core::kNumNotes; ++i)
        noteButtons_[static_cast<size_t>(i)].setToggleState(i == active, juce::dontSendNotification);
}

void KickSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    fineTune_.setVisible(advanced); pitchCurve_.setVisible(advanced); tailCurve_.setVisible(advanced);
    clickFreq_.setVisible(advanced); snareLevel_.setVisible(advanced); snareTone_.setVisible(advanced);
    snareDecay_.setVisible(advanced); clapSpread_.setVisible(advanced);
    clapTone_.setVisible(advanced); clapDecay_.setVisible(advanced);
    hatTone_.setVisible(advanced); hatDecay_.setVisible(advanced);
    snareLevel_.setVisible(true); clapLevel_.setVisible(true); hatLevel_.setVisible(true);
    resized(); repaint();
}

void KickSynthPanel::paint(juce::Graphics& g) {
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 13.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 13.0f, 1.0f);
    drawCard(g, tuningArea_, "Key Tuning", theme::kCyan);
    drawCard(g, pitchArea_, "Pitch Envelope", theme::kCyan);
    drawCard(g, bodyArea_, "Body & ADSR", theme::kAmber);
    drawCard(g, transientArea_, "Attack Transient", theme::kCoral);
    drawCard(g, layersArea_, advancedMode_ ? "Layered Texture" : "Layered Texture · Advanced", theme::kCyan);
    if (advancedMode_) {
        drawSmallLabel(g, layersArea_.reduced(14).withY(layersArea_.getY() + 25).withHeight(14),
                       "SNARE · NOISE CLAP · METALLIC HAT", theme::kTextDim);
    } else {
        drawSmallLabel(g, layersArea_.reduced(14).withY(layersArea_.getY() + 25).withHeight(14),
                       "BLEND LAYERS · FULL CONTROLS ADDS TONE AND DECAY SHAPING", theme::kTextDim);
    }
}

void KickSynthPanel::resized() {
    auto area = getLocalBounds().reduced(8);
    tuningArea_ = area.removeFromTop(136); area.removeFromTop(7);
    waveArea_ = area.removeFromTop(128); waveform_.setBounds(waveArea_); area.removeFromTop(7);
    pitchArea_ = area.removeFromTop(98); area.removeFromTop(7);
    bodyArea_ = area.removeFromTop(108); area.removeFromTop(7);
    transientArea_ = area.removeFromTop(100); area.removeFromTop(7);
    layersArea_ = area;

    auto tuning = tuningArea_;
    tuningLock_.setBounds(tuning.removeFromRight(64).withY(tuning.getY() + 6).withHeight(22));
    auto tune = tuning.reduced(12, 25);
    auto keys = tune.removeFromTop(25);
    const int keyWidth = keys.getWidth() / core::kNumNotes;
    for (int i = 0; i < core::kNumNotes; ++i) {
        auto key = keys.removeFromLeft(keyWidth);
        key.removeFromRight(i == core::kNumNotes - 1 ? 0 : 2);
        noteButtons_[static_cast<size_t>(i)].setBounds(key.withHeight(25));
    }
    auto tuneOptions = tune.withTrimmedTop(2);
    auditionButton_.setBounds(tuneOptions.removeFromRight(82).withHeight(28)); tuneOptions.removeFromRight(6);
    triggerMode_.setBounds(tuneOptions.removeFromRight(112).withHeight(32)); tuneOptions.removeFromRight(6);
    octave_.setBounds(tuneOptions.removeFromRight(90).withHeight(32)); tuneOptions.removeFromRight(6);
    fineTune_.setBounds(tuneOptions.withWidth(100).withHeight(55));

    auto pitch = pitchArea_;
    pitchLock_.setBounds(pitch.removeFromRight(64).withY(pitch.getY() + 6).withHeight(22));
    if (advancedMode_) layoutRow(pitch.reduced(12, 26), { &startPitch_, &pitchDecay_, &pitchCurve_ });
    else layoutRow(pitch.reduced(12, 26), { &startPitch_, &pitchDecay_ });

    auto body = bodyArea_;
    bodyLock_.setBounds(body.removeFromRight(64).withY(body.getY() + 6).withHeight(22));
    if (advancedMode_)
        layoutRow(body.reduced(10, 25), { &bodyTone_, &attack_, &decay_, &sustain_, &release_, &tailLength_, &tailCurve_, &punch_, &subDrive_, &lockIn_ }, 2);
    else
        layoutRow(body.reduced(10, 25), { &bodyTone_, &attack_, &decay_, &sustain_, &release_, &tailLength_, &punch_, &subDrive_, &lockIn_ }, 2);

    auto transient = transientArea_;
    transientLock_.setBounds(transient.removeFromRight(64).withY(transient.getY() + 6).withHeight(22));
    if (advancedMode_) layoutRow(transient.reduced(12, 25), { &clickLevel_, &clickFreq_ });
    else layoutRow(transient.reduced(12, 25), { &clickLevel_ });

    auto layers = layersArea_;
    layersLock_.setBounds(layers.removeFromRight(64).withY(layers.getY() + 6).withHeight(22));
    const int innerY = layers.getY() + (advancedMode_ ? 40 : 30);
    if (advancedMode_) {
        auto row = juce::Rectangle<int>(layers.getX() + 12, innerY, layers.getWidth() - 24, 76);
        auto snare = row.removeFromLeft(row.getWidth() * 3 / 10);
        auto clap = row.removeFromLeft(row.getWidth() * 4 / 7);
        auto hat = row;
        layoutRow(snare, { &snareLevel_, &snareTone_, &snareDecay_ }, 2);
        layoutRow(clap, { &clapLevel_, &clapSpread_, &clapTone_, &clapDecay_ }, 2);
        layoutRow(hat, { &hatLevel_, &hatTone_, &hatDecay_ }, 2);
    } else {
        const auto row = juce::Rectangle<int>(layers.getX() + 12, innerY, layers.getWidth() - 24, 58);
        layoutRow(row, { &snareLevel_, &clapLevel_, &hatLevel_ }, 10);
    }
}

MuleStagePanel::MuleStagePanel(OpenEdmKickProcessor& processor) : processor_(processor) {
    mascot_ = juce::ImageCache::getFromMemory(BinaryData::kickingmule_png, BinaryData::kickingmule_pngSize);
    seenTriggers_ = processor_.displayTriggerCount.load();
    startTimerHz(24);
}

void MuleStagePanel::timerCallback() {
    syncKickAnimation();
    repaint();
}

void MuleStagePanel::syncKickAnimation() {
    const auto triggerCount = processor_.displayTriggerCount.load();
    if (triggerCount != seenTriggers_) {
        seenTriggers_ = triggerCount;
        kickStartedMs_ = juce::Time::getMillisecondCounterHiRes();
    }
}

void MuleStagePanel::paint(juce::Graphics& g) {
    syncKickAnimation();
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kSteel); g.fillRoundedRectangle(bounds, 13.0f);
    g.setColour(theme::kEdge); g.drawRoundedRectangle(bounds.reduced(0.5f), 13.0f, 1.0f);
    drawSmallLabel(g, getLocalBounds().reduced(14).removeFromTop(16), "KICKING MULE · LIVE STAGE", theme::kAmber);

    auto stage = bounds.reduced(12.0f, 34.0f).withTrimmedBottom(100.0f);
    g.setColour(theme::kInk); g.fillRoundedRectangle(stage, 12.0f);
    const float peak = std::clamp(processor_.displayPeak.load(), 0.0f, 1.0f);
    const float beat = std::clamp(processor_.displayBeatPhase.load(), 0.0f, 1.0f);
    const double kickAgeMs = juce::Time::getMillisecondCounterHiRes() - kickStartedMs_;
    const float kickPulse = kickStartedMs_ < 0.0 ? 0.0f : std::exp(-static_cast<float>(std::max(0.0, kickAgeMs)) / 95.0f);
    const float pulse = std::max({ peak, kickPulse, 0.16f * std::pow(1.0f - beat, 3.0f) });
    const float radius = std::min(stage.getWidth(), stage.getHeight()) * (0.28f + pulse * 0.04f);
    const juce::Point<float> centre(stage.getCentreX(), stage.getCentreY() + 4.0f);
    g.setColour(theme::kCyan.withAlpha(0.08f + pulse * 0.28f));
    g.fillEllipse(centre.x - radius * 1.5f, centre.y - radius * 1.5f, radius * 3.0f, radius * 3.0f);
    g.setColour(theme::kEdge); g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 3.0f);
    g.setColour(theme::kCyan.withAlpha(0.75f)); g.drawEllipse(centre.x - radius * 0.86f, centre.y - radius * 0.86f,
                                                              radius * 1.72f, radius * 1.72f, 1.5f);
    if (mascot_.isValid()) {
        const int baseSize = juce::jmin(juce::roundToInt(stage.getWidth() * 0.8f), juce::roundToInt(stage.getHeight() * 0.84f));
        const int size = juce::roundToInt(static_cast<float>(baseSize) * (1.0f + 0.08f * kickPulse));
        const int hop = juce::roundToInt(stage.getHeight() * 0.05f * kickPulse);
        g.drawImageWithin(mascot_, juce::roundToInt(centre.x - size * 0.42f), juce::roundToInt(centre.y - size * 0.5f - hop),
                          juce::roundToInt(size * 0.92f), size, juce::RectanglePlacement::centred);
    }

    auto footer = getLocalBounds().reduced(16).removeFromBottom(84);
    const float bpm = processor_.displayBpm.load();
    const bool locked = processor_.isKickLockedIn();
    g.setColour(theme::kText); g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText(juce::String(bpm, 1) + " BPM", footer.removeFromTop(24), juce::Justification::centredLeft);
    auto meter = footer.removeFromTop(18).reduced(0, 4);
    g.setColour(theme::kInk); g.fillRoundedRectangle(meter.toFloat(), 4.0f);
    auto fill = meter.withWidth(juce::roundToInt(meter.getWidth() * std::sqrt(peak)));
    g.setColour(theme::kCoral); g.fillRoundedRectangle(fill.toFloat(), 4.0f);
    g.setColour(locked ? theme::kAmber : theme::kCyan);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(locked ? "FROZEN WAV" : "LIVE SYNTHESIS", footer, juce::Justification::centredLeft);
}

PianoRollView::PianoRollView(OpenEdmKickProcessor& processor) : processor_(processor) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(18);
}

juce::Rectangle<float> PianoRollView::gridArea() const {
    return getLocalBounds().toFloat().reduced(14.0f, 28.0f).withTrimmedLeft(54.0f).withTrimmedBottom(18.0f);
}

void PianoRollView::mouseDown(const juce::MouseEvent& event) {
    const auto grid = gridArea();
    if (!grid.contains(event.position)) return;
    const auto settings = processor_.currentAutoSlotSettings();
    const int totalSteps = std::max(1, settings.bars) * core::kStepsPerBar64;
    const int gridStep = core::kGridStepSizes64[static_cast<size_t>(std::clamp(settings.gridResolution, 0, 4))];
    const float x = std::clamp((event.position.x - grid.getX()) / grid.getWidth(), 0.0f, 0.99999f);
    const float y = std::clamp((event.position.y - grid.getY()) / grid.getHeight(), 0.0f, 0.99999f);
    const int step = static_cast<int>(x * totalSteps) / gridStep * gridStep;
    const int row = std::clamp(static_cast<int>(y * kPitchRows.size()), 0, static_cast<int>(kPitchRows.size()) - 1);
    processor_.togglePianoRollNote(step, gridStep, kPitchRows[static_cast<size_t>(row)]);
    repaint();
}

void PianoRollView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kInk); g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(theme::kEdge); g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);
    drawSmallLabel(g, getLocalBounds().reduced(12).removeFromTop(16), "PIANO ROLL · 1/64 GRID", theme::kCyan);
    const auto grid = gridArea();
    const auto settings = processor_.currentAutoSlotSettings();
    const int bars = std::clamp(settings.bars, 1, 64);
    const int totalSteps = bars * core::kStepsPerBar64;
    const float rowHeight = grid.getHeight() / static_cast<float>(kPitchRows.size());
    for (int row = 0; row < static_cast<int>(kPitchRows.size()); ++row) {
        const float y = grid.getY() + row * rowHeight;
        g.setColour(row % 2 == 0 ? theme::kSteel : theme::kInk.brighter(0.03f));
        g.fillRect(grid.getX(), y, grid.getWidth(), rowHeight - 1.0f);
        g.setColour(row == static_cast<int>(kPitchRows.size()) - 1 ? theme::kCyan : theme::kTextDim);
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(kPitchNames[static_cast<size_t>(row)], juce::Rectangle<float>(bounds.getX() + 11.0f, y, 42.0f, rowHeight),
                   juce::Justification::centredLeft);
    }
    const int lines = std::min(128, totalSteps);
    for (int i = 0; i <= lines; ++i) {
        const float x = grid.getX() + grid.getWidth() * static_cast<float>(i) / static_cast<float>(lines);
        const bool bar = i % 16 == 0;
        const bool beat = i % 4 == 0;
        g.setColour(theme::kEdge.withAlpha(bar ? 0.95f : (beat ? 0.52f : 0.18f)));
        g.drawVerticalLine(juce::roundToInt(x), juce::roundToInt(grid.getY()), juce::roundToInt(grid.getBottom()));
    }

    const auto notes = processor_.currentPianoRollNotes();
    for (const auto& note : notes) {
        if (note.step64 < 0 || note.step64 >= totalSteps) continue;
        const auto closest = std::ranges::min_element(kPitchRows, [&](int a, int b) {
            return std::abs(a - note.semitoneOffset) < std::abs(b - note.semitoneOffset);
        });
        const int row = static_cast<int>(std::distance(kPitchRows.begin(), closest));
        const float x = grid.getX() + grid.getWidth() * static_cast<float>(note.step64) / totalSteps;
        const float w = std::max(3.0f, grid.getWidth() * static_cast<float>(std::max(1, note.length64)) / totalSteps - 1.0f);
        const float y = grid.getY() + row * rowHeight + 2.0f;
        const auto colour = note.reverseCue ? theme::kCoral : theme::kCyan;
        g.setColour(colour.withAlpha(0.38f + 0.48f * std::clamp(note.velocity, 0.2f, 1.0f)));
        g.fillRoundedRectangle(x, y, w, rowHeight - 4.0f, 3.0f);
        g.setColour(theme::kAmber);
        g.fillRect(x + 2.0f, y + 2.0f, std::max(2.0f, (w - 4.0f) * (1.0f - 0.62f * settings.tailShorten)), 2.0f);
    }

    const float phase = std::clamp(processor_.displayAutoPhase.load(), 0.0f, 0.9999f);
    const float head = grid.getX() + phase * grid.getWidth();
    g.setColour(theme::kAmber); g.drawVerticalLine(juce::roundToInt(head), juce::roundToInt(grid.getY() - 2), juce::roundToInt(grid.getBottom() + 2));
    const bool enabled = processor_.parameters.getRawParameterValue(params::kAutoEnabled)->load() > 0.5f;
    if (!enabled) {
        g.setColour(theme::kInk.withAlpha(0.82f)); g.fillRoundedRectangle(grid.getX(), grid.getBottom() - 20.0f, grid.getWidth(), 18.0f, 4.0f);
        g.setColour(theme::kAmber); g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("AUTO MODE IS OFF · EDIT THE ROLL, THEN ENABLE PLAYBACK", juce::Rectangle<float>(grid.getX() + 8.0f,
                   grid.getBottom() - 20.0f, grid.getWidth() - 16.0f, 18.0f), juce::Justification::centredLeft);
    }
}

AutoPianoRollPanel::AutoPianoRollPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      bypass_(processor.parameters, params::kAutoBypass, "BYPASS", theme::kCoral),
      enabled_(processor.parameters, params::kAutoEnabled, "AUTO MODE", theme::kCyan),
      clockSync_(processor.parameters, params::kClockSync, "HOST SYNC", theme::kAmber),
      timeSig_(processor.parameters, params::kTimeSignature, "TIME SIGNATURE", theme::kAmber),
      bars_(processor.parameters, params::kAutoBars, "LENGTH", theme::kCyan),
      grid_(processor.parameters, params::kAutoGrid, "GRID", theme::kCyan),
      fillPreset_(processor.parameters, params::kAutoFillPreset, "FILL", theme::kCyan),
      tempo_(processor.parameters, params::kTempoBpm, "TEMPO", theme::kAmber),
      tailShorten_(processor.parameters, params::kAutoTailShorten, "TAIL SHORTEN", theme::kAmber),
      pitchRise_(processor.parameters, params::kAutoPitchRise, "PITCH RISE", theme::kCyan),
      crushSweep_(processor.parameters, params::kAutoCrushSweep, "CRUSH RAMP", theme::kCoral),
      pianoRoll_(processor),
      autoLock_("Auto Mode", theme::kCyan,
                [&processor] { return processor.isSectionLocked(section(params::Section::autoMode)); },
                [&processor](bool on) { processor.setSectionLocked(section(params::Section::autoMode), on); }),
      slotWatcher_(*processor.parameters.getParameter(params::kAutoSlot), [this](float value) { selectSlot(static_cast<int>(value)); }) {
    for (int i = 0; i < core::kNumAutoSlots; ++i) {
        auto& button = slotButtons_[static_cast<size_t>(i)];
        button.setButtonText(juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)));
        button.setComponentID("auto_slot_" + juce::String(i));
        button.setColour(juce::TextButton::buttonOnColourId, theme::kCyan);
        button.onClick = [this, i] { slotWatcher_.setValueAsCompleteGesture(static_cast<float>(i)); };
        addAndMakeVisible(button);
    }
    quickPresetBox_.setTextWhenNothingSelected("Load a trance fill…");
    const auto& fills = presets::fillBuildPresets();
    for (int i = 0; i < static_cast<int>(fills.size()); ++i) quickPresetBox_.addItem(fills[static_cast<size_t>(i)].name, i + 1);
    quickPresetBox_.onChange = [this] {
        const int index = quickPresetBox_.getSelectedId() - 1;
        const auto& available = presets::fillBuildPresets();
        if (index >= 0 && index < static_cast<int>(available.size()))
            presets::applyToCurrentAutoSlot(available[static_cast<size_t>(index)], processor_.parameters, processor_.sectionLockMask());
    };
    for (juce::Component* component : std::array<juce::Component*, 12>{
             &bypass_, &enabled_, &clockSync_, &timeSig_, &bars_, &grid_, &fillPreset_, &quickPresetBox_,
             &tempo_, &tailShorten_, &pitchRise_, &crushSweep_}) addAndMakeVisible(component);
    addAndMakeVisible(pianoRoll_); addAndMakeVisible(autoLock_);
    enabled_.setTooltip("Run the active A/B/C/D step pattern from host sync or the internal tempo.");
    clockSync_.setTooltip("Use host tempo and playhead position. Turn off to use the internal tempo and time signature.");
    tailShorten_.setTooltip("Shortens kick tails on fast rolls to keep consecutive hits clear.");
    slotWatcher_.sendInitialUpdate();
    setAdvancedMode(false);
}

void AutoPianoRollPanel::selectSlot(int slot) {
    const int safe = std::clamp(slot, 0, core::kNumAutoSlots - 1);
    for (int i = 0; i < core::kNumAutoSlots; ++i)
        slotButtons_[static_cast<size_t>(i)].setToggleState(i == safe, juce::dontSendNotification);
    bars_.bind(processor_.parameters, params::autoSlotId(safe, params::kAutoBars));
    fillPreset_.bind(processor_.parameters, params::autoSlotId(safe, params::kAutoFillPreset));
    grid_.bind(processor_.parameters, params::autoSlotId(safe, params::kAutoGrid));
    tailShorten_.bind(processor_.parameters, params::autoSlotId(safe, params::kAutoTailShorten));
    pitchRise_.bind(processor_.parameters, params::autoSlotId(safe, params::kAutoPitchRise));
    crushSweep_.bind(processor_.parameters, params::autoSlotId(safe, params::kAutoCrushSweep));
    repaint();
}

void AutoPianoRollPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    timeSig_.setVisible(advanced);
    pitchRise_.setVisible(advanced);
    crushSweep_.setVisible(advanced);
    resized(); repaint();
}

void AutoPianoRollPanel::paint(juce::Graphics& g) {
    g.setColour(theme::kInk); g.fillRoundedRectangle(getLocalBounds().toFloat(), 13.0f);
    g.setColour(theme::kEdge); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 13.0f, 1.0f);
    drawSmallLabel(g, getLocalBounds().reduced(14).removeFromTop(16), "TRANCE SEQUENCER · FOUR INDEPENDENT SCENES", theme::kCyan);
}

void AutoPianoRollPanel::resized() {
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(50); autoLock_.setBounds(top.removeFromRight(72).withHeight(24).withY(top.getY() + 2));
    auto line = area.removeFromTop(42); line.removeFromTop(2);
    enabled_.setBounds(line.removeFromLeft(112).reduced(0, 5)); line.removeFromLeft(6);
    bypass_.setBounds(line.removeFromLeft(86).reduced(0, 5)); line.removeFromLeft(6);
    clockSync_.setBounds(line.removeFromLeft(112).reduced(0, 5)); line.removeFromLeft(10);
    if (advancedMode_) { timeSig_.setBounds(line.removeFromLeft(132)); line.removeFromLeft(10); }
    tempo_.setBounds(line.removeFromLeft(108).reduced(0, 1));

    auto controls = area.removeFromTop(56);
    auto slots = controls.removeFromLeft(176).reduced(0, 12);
    layoutRow(slots, { &slotButtons_[0], &slotButtons_[1], &slotButtons_[2], &slotButtons_[3] }, 5);
    controls.removeFromLeft(10);
    bars_.setBounds(controls.removeFromLeft(132).reduced(0, 4)); controls.removeFromLeft(8);
    fillPreset_.setBounds(controls.removeFromLeft(250).reduced(0, 4)); controls.removeFromLeft(8);
    grid_.setBounds(controls.removeFromLeft(130).reduced(0, 4)); controls.removeFromLeft(8);
    tailShorten_.setBounds(controls.removeFromLeft(118)); controls.removeFromLeft(8);
    quickPresetBox_.setBounds(controls.withHeight(28).withY(controls.getY() + 14));
    if (advancedMode_) {
        auto advanced = area.removeFromTop(60);
        pitchRise_.setBounds(advanced.removeFromLeft(110)); advanced.removeFromLeft(12);
        crushSweep_.setBounds(advanced.removeFromLeft(110));
    }
    area.removeFromTop(6);
    pianoRoll_.setBounds(area);
}

PeakMeter::PeakMeter(OpenEdmKickProcessor& processor) : processor_(processor) { startTimerHz(20); }

void PeakMeter::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kInk); g.fillRoundedRectangle(bounds, 5.0f);
    const float peak = std::clamp(processor_.displayPeak.load(), 0.0f, 1.0f);
    auto fill = bounds.reduced(2.0f).withWidth(std::max(1.0f, (bounds.getWidth() - 4.0f) * peak));
    g.setColour(peak > 0.9f ? theme::kCoral : theme::kCyan); g.fillRoundedRectangle(fill, 4.0f);
    g.setColour(theme::kText); g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("OUT  " + juce::String(juce::Decibels::gainToDecibels(std::max(peak, 0.0001f)), 1) + " dB",
               getLocalBounds().reduced(7, 0), juce::Justification::centredRight);
}

EffectsPanel::EffectsPanel(OpenEdmKickProcessor& processor)
    : reverseMode_(processor.parameters, params::kReverseMode, "SYNC MODE", theme::kCoral),
      reverseMix_(processor.parameters, params::kReverseMix, "REVERSE MIX", theme::kCoral),
      drive_(processor.parameters, params::kDrive, "DRIVE", theme::kAmber),
      crushRate_(processor.parameters, params::kCrushRate, "SAMPLE RATE", theme::kAmber),
      crushBits_(processor.parameters, params::kCrushBits, "BITS", theme::kAmber),
      crushMix_(processor.parameters, params::kCrushMix, "CRUSH MIX", theme::kAmber),
      stutterRate_(processor.parameters, params::kStutterRate, "REPEAT RATE", theme::kCyan),
      stutterMix_(processor.parameters, params::kStutterMix, "REPEAT MIX", theme::kCyan),
      gateRate_(processor.parameters, params::kGateRate, "GATE RATE", theme::kCyan),
      gateMix_(processor.parameters, params::kGateMix, "GATE DEPTH", theme::kCyan),
      pumpRate_(processor.parameters, params::kPumpRate, "FILTER RATE", theme::kCyan),
      pumpMix_(processor.parameters, params::kPumpMix, "FILTER DEPTH", theme::kCyan),
      highPass_(processor.parameters, params::kHighPass, "HIGH PASS", theme::kAmber),
      lowPass_(processor.parameters, params::kLowPass, "LOW PASS", theme::kAmber),
      output_(processor.parameters, params::kOutput, "OUTPUT", theme::kAmber),
      ceiling_(processor.parameters, params::kCeiling, "CEILING", theme::kAmber),
      limiter_(processor.parameters, params::kLimiter, "LIMITER", theme::kAmber),
      masterBypass_(processor.parameters, params::kMasterBypass, "MASTER BYPASS", theme::kCoral),
      meter_(processor),
      reverseLock_("Reverse", theme::kCoral, [&processor] { return processor.isSectionLocked(section(params::Section::reverse)); },
                   [&processor](bool on) { processor.setSectionLocked(section(params::Section::reverse), on); }),
      textureLock_("Texture", theme::kAmber, [&processor] { return processor.isSectionLocked(section(params::Section::texture)); },
                   [&processor](bool on) { processor.setSectionLocked(section(params::Section::texture), on); }),
      rhythmLock_("Rhythm", theme::kCyan, [&processor] { return processor.isSectionLocked(section(params::Section::rhythm)); },
                  [&processor](bool on) { processor.setSectionLocked(section(params::Section::rhythm), on); }),
      toneLock_("Tone", theme::kAmber, [&processor] { return processor.isSectionLocked(section(params::Section::toneOutput)); },
                [&processor](bool on) { processor.setSectionLocked(section(params::Section::toneOutput), on); }) {
    for (juce::Component* component : std::array<juce::Component*, 19>{
             &reverseMode_, &reverseMix_, &drive_, &crushRate_, &crushBits_, &crushMix_, &stutterRate_, &stutterMix_,
             &gateRate_, &gateMix_, &pumpRate_, &pumpMix_, &highPass_, &lowPass_, &output_, &ceiling_, &limiter_,
             &masterBypass_, &meter_}) addAndMakeVisible(component);
    addAndMakeVisible(reverseLock_); addAndMakeVisible(textureLock_); addAndMakeVisible(rhythmLock_); addAndMakeVisible(toneLock_);
    reverseMode_.setTooltip("Reverse playback can finish on beat, half beat, quarter beat, or the last beat of each bar.");
    setAdvancedMode(false);
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    crushRate_.setVisible(advanced); crushBits_.setVisible(advanced); pumpRate_.setVisible(advanced); pumpMix_.setVisible(advanced);
    resized(); repaint();
}

void EffectsPanel::paint(juce::Graphics& g) {
    g.setColour(theme::kInk); g.fillRoundedRectangle(getLocalBounds().toFloat(), 13.0f);
    g.setColour(theme::kEdge); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 13.0f, 1.0f);
    drawSmallLabel(g, getLocalBounds().reduced(14).removeFromTop(16), "MASTERING & MOTION", theme::kAmber);
    const auto area = getLocalBounds().reduced(8);
    drawCard(g, {area.getX(), area.getY() + 50, (area.getWidth() - 8) / 2, (area.getHeight() - 60) / 2}, "Reverse", theme::kCoral);
    drawCard(g, {area.getX() + (area.getWidth() + 8) / 2, area.getY() + 50, (area.getWidth() - 8) / 2, (area.getHeight() - 60) / 2}, "Texture", theme::kAmber);
    drawCard(g, {area.getX(), area.getY() + 50 + (area.getHeight() - 60) / 2 + 10, (area.getWidth() - 8) / 2, (area.getHeight() - 60) / 2}, "Rhythm", theme::kCyan);
    drawCard(g, {area.getX() + (area.getWidth() + 8) / 2, area.getY() + 50 + (area.getHeight() - 60) / 2 + 10,
                 (area.getWidth() - 8) / 2, (area.getHeight() - 60) / 2}, "Tone & Output", theme::kAmber);
}

void EffectsPanel::resized() {
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(50);
    const int gap = 8;
    const int cardW = (area.getWidth() - gap) / 2;
    const int cardH = (area.getHeight() - gap) / 2;
    auto reverse = juce::Rectangle<int>(area.getX(), area.getY(), cardW, cardH).reduced(12);
    auto texture = juce::Rectangle<int>(area.getX() + cardW + gap, area.getY(), cardW, cardH).reduced(12);
    auto rhythm = juce::Rectangle<int>(area.getX(), area.getY() + cardH + gap, cardW, cardH).reduced(12);
    auto tone = juce::Rectangle<int>(area.getX() + cardW + gap, area.getY() + cardH + gap, cardW, cardH).reduced(12);

    reverseLock_.setBounds(reverse.removeFromTop(24).removeFromRight(68)); reverse.removeFromTop(12);
    reverseMode_.setBounds(reverse.removeFromTop(52)); reverse.removeFromTop(6);
    const int mixSize = std::min(148, reverse.getWidth() - 20);
    reverseMix_.setBounds(juce::Rectangle<int>(reverse.getCentreX() - mixSize / 2,
                                                reverse.getY() + 8, mixSize, 148));

    textureLock_.setBounds(texture.removeFromTop(24).removeFromRight(68)); texture.removeFromTop(8);
    if (advancedMode_) {
        layoutRow(texture.removeFromTop(std::max(80, texture.getHeight() / 2)), { &drive_, &crushRate_, &crushBits_ }, 4);
        layoutRow(texture, { &crushMix_ }, 4);
    } else layoutRow(texture, { &drive_, &crushMix_ }, 8);

    rhythmLock_.setBounds(rhythm.removeFromTop(24).removeFromRight(68)); rhythm.removeFromTop(8);
    if (advancedMode_) {
        layoutRow(rhythm, { &stutterRate_, &stutterMix_, &gateRate_, &gateMix_, &pumpRate_, &pumpMix_ }, 3);
    } else {
        layoutRow(rhythm.withHeight(rhythm.getHeight() / 2), { &stutterRate_, &stutterMix_, &gateRate_, &gateMix_ }, 4);
        auto rest = rhythm.withTrimmedTop(rhythm.getHeight() / 2);
        pumpRate_.setBounds(0, 0, 0, 0); pumpMix_.setBounds(0, 0, 0, 0);
        (void) rest;
    }

    toneLock_.setBounds(tone.removeFromTop(24).removeFromRight(68)); tone.removeFromTop(8);
    layoutRow(tone.removeFromTop(std::max(80, tone.getHeight() / 2)), { &highPass_, &lowPass_, &output_ }, 4);
    auto lower = tone;
    const int ceilingSize = std::min(148, lower.getWidth() / 3);
    ceiling_.setBounds(juce::Rectangle<int>(lower.getX(), lower.getY(), ceilingSize, std::min(150, lower.getHeight())));
    auto toggles = lower.withTrimmedLeft(ceilingSize + 12).reduced(0, 4);
    limiter_.setBounds(toggles.removeFromTop(32));
    toggles.removeFromTop(5);
    masterBypass_.setBounds(toggles.removeFromTop(32));
    toggles.removeFromTop(8);
    meter_.setBounds(toggles.removeFromTop(22));
}

} // namespace openedmkick::ui
