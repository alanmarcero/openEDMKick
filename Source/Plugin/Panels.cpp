#include "Panels.h"
#include "suite/EqNotes.h"

#include "Presets.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <ranges>

namespace openedmkick::ui {
namespace {
namespace core = openedmkick::core;
namespace params = openedmkick::params;
namespace theme = openedmkick::theme;

constexpr int kCardGap = 8;
constexpr int kCompactGap = 4;
constexpr int kMidiModeWidth = 164;
constexpr int kCardHeader = 43;
constexpr int kSectionLockWidth = 112;
constexpr int kSectionLockHeight = 40;
constexpr float kMeterFloorDb = -36.0f;

void setControlsVisible(std::initializer_list<juce::Component*> controls, bool visible) {
    std::ranges::for_each(controls, [visible](juce::Component* control) { control->setVisible(visible); });
}

void layoutWeightedRow(juce::Rectangle<int> area,
                       std::initializer_list<std::pair<juce::Component*, float>> items, int gap) {
    const float totalWeight = std::accumulate(items.begin(), items.end(), 0.0f,
        [](float acc, const auto& item) { return acc + (item.first->isVisible() ? item.second : 0.0f); });
    const int visibleCount = static_cast<int>(std::count_if(items.begin(), items.end(),
        [](const auto& item) { return item.first->isVisible(); }));
    if (visibleCount <= 0 || totalWeight <= 0.0f) return;
    const int safeGap = std::max(0, gap);
    const int available = std::max(0, area.getWidth() - safeGap * (visibleCount - 1));
    std::ranges::for_each(items, [&](const auto& item) {
        if (!item.first->isVisible()) return;
        const int w = juce::roundToInt(static_cast<float>(available) * (item.second / totalWeight));
        item.first->setBounds(area.removeFromLeft(w));
        area.removeFromLeft(safeGap);
    });
}

void placeRow(juce::Rectangle<int> card, int row, int rowCount,
              std::initializer_list<juce::Component*> controls) {
    const int innerTop = card.getY() + kCardHeader;
    const int innerHeight = std::max(0, card.getHeight() - kCardHeader - 4);
    const int rowHeight = innerHeight / std::max(1, rowCount);
    auto bounds = juce::Rectangle<int>(card.getX() + 8, innerTop + row * rowHeight,
                                        card.getWidth() - 16, rowHeight);
    layoutRow(bounds, controls, 3);
}

juce::String noteLabel(int note) {
    const auto name = core::noteName(note);
    return juce::String(name.data(), name.size()) + juce::String(core::noteOctave(note));
}


} // namespace

KickWaveformView::KickWaveformView(OpenEdmKickProcessor& processor) : processor_(processor) {
    setOpaque(false);
    std::ranges::for_each(processor.getParameters(), [this](juce::AudioProcessorParameter* parameter) {
        sources_.push_back(processor_.routing().effectiveRaw(dynamic_cast<juce::RangedAudioParameter*>(parameter)->paramID));
    });
    startTimerHz(15);
    timerCallback();
}

void KickWaveformView::timerCallback() {
    // Render the Drag hit again only when one of its inputs changed.
    std::vector<float> inputs;
    inputs.reserve(sources_.size() + 3);
    std::ranges::transform(sources_, std::back_inserter(inputs), [](const std::atomic<float>* source) { return source->load(); });
    inputs.push_back(static_cast<float>(processor_.currentMidiNote()));
    inputs.push_back(processor_.lastVelocity());
    if (inputs == renderedInputs_) return;
    renderedInputs_ = std::move(inputs);
    processor_.renderCurrentHit(hit_);
    repaint();
}

void KickWaveformView::paint(juce::Graphics& g) {
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 11.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 11.0f, 1.0f);
    const bool compact = getHeight() < 110;
    auto area = getLocalBounds().reduced(16, compact ? 6 : 12);
    auto readout = area.removeFromLeft(136);
    const int note = processor_.currentMidiNote();
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(compact ? 13.0f : 14.5f));
    g.drawText("MIDI note", readout.removeFromTop(compact ? 17 : 21), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(compact ? 32.0f : 40.0f, juce::Font::bold));
    g.drawText(noteLabel(note), readout.removeFromTop(compact ? 34 : 46), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(compact ? 14.0f : 17.0f));
    g.drawText(params::formatHz(core::noteFrequencyHz(note)), readout, juce::Justification::topLeft);

    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("One-shot waveform", area.removeFromTop(20), juce::Justification::centredLeft);
    const auto scale = area.removeFromBottom(20);
    const auto length = static_cast<size_t>(hit_.getNumSamples());
    const int durationMs = juce::roundToInt(1000.0 * static_cast<double>(length) / OpenEdmKickProcessor::hitSampleRate);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("0 ms", scale, juce::Justification::bottomLeft);
    g.drawText(juce::String(durationMs) + " ms", scale, juce::Justification::bottomRight);
    const auto plot = area.toFloat().reduced(1.0f, 2.0f);
    g.setColour(theme::kViolet.withAlpha(0.18f));
    g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());
    const int tickMs = durationMs > 500 ? 250 : (durationMs > 220 ? 100 : 50);
    for (int ms = tickMs; ms < durationMs; ms += tickMs) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(ms) / static_cast<float>(durationMs);
        g.setColour(theme::kViolet.withAlpha(0.18f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        if (x + 54 < plot.getRight()) {
            g.setColour(theme::kText);
            g.drawText(juce::String(ms), juce::roundToInt(x) - 20, scale.getY(), 40, scale.getHeight(),
                       juce::Justification::centredBottom);
        }
    }
    if (length < 2) return;

    juce::Path wave;
    const float mid = plot.getCentreY();
    const float amplitude = plot.getHeight() * 0.42f;
    const size_t count = std::min<size_t>(length, 1200);
    for (size_t i = 0; i < count; ++i) {
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(count - 1);
        const size_t sampleIndex = i * (length - 1) / (count - 1);
        const float y = mid - std::clamp(hit_.getSample(0, static_cast<int>(sampleIndex)), -1.0f, 1.0f) * amplitude;
        if (i == 0) wave.startNewSubPath(x, y);
        else wave.lineTo(x, y);
    }
    g.setColour(theme::kViolet);
    g.strokePath(wave, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

KickSynthPanel::KickSynthPanel(OpenEdmKickProcessor& processor)
    : sweepOctaves_(processor.parameters, params::kSweepOctaves, "Dive range", theme::kViolet),
      sweepMs_(processor.parameters, params::kSweepMs, "Dive time", theme::kViolet),
      sweepShape_(processor.parameters, params::kSweepShape, "Dive curve", theme::kViolet),
      keyTracking_(processor.parameters, params::kKeyTracking, "Key tracking", theme::kViolet),
      snapOctaves_(processor.parameters, params::kSnapOctaves, "Snap range", theme::kViolet),
      snapMs_(processor.parameters, params::kSnapMs, "Snap time", theme::kViolet),
      bodyTone_(processor.parameters, params::kBodyTone, "Harmonics", theme::kMint),
      knockHz_(processor.parameters, params::kKnockHz, "Knock freq", theme::kMint),
      knockDb_(processor.parameters, params::kKnockDb, "Knock", theme::kMint),
      toneHz_(processor.parameters, params::kToneHz, "Body tone", theme::kMint),
      subLevel_(processor.parameters, params::kSubLevel, "Sub level", theme::kMint),
      subOctave_(processor.parameters, params::kSubOctave, "Sub octave", theme::kMint),
      startPhase_(processor.parameters, params::kStartPhase, "Start phase", theme::kMint),
      clickType_(processor.parameters, params::kClickType, "Source", theme::kCoral),
      clickLevel_(processor.parameters, params::kClickLevel, "Level", theme::kCoral),
      clickHz_(processor.parameters, params::kClickHz, "Tone", theme::kCoral),
      clickMs_(processor.parameters, params::kClickMs, "Length", theme::kCoral),
      clickPunch_(processor.parameters, params::kClickPunch, "Punch", theme::kCoral),
      attack_(processor.parameters, params::kAttackMs, "Attack", theme::kMint),
      hold_(processor.parameters, params::kHoldMs, "Hold", theme::kMint),
      decay_(processor.parameters, params::kDecayMs, "Decay", theme::kMint),
      decayShape_(processor.parameters, params::kDecayShape, "Decay curve", theme::kMint),
      lowCut_(processor.parameters, params::kLowCutHz, "Low cut", theme::kMint),
      ampEnabled_(processor.parameters, params::kAmpEnabled, "Hold MIDI notes", theme::kViolet),
      ampAttack_(processor.parameters, params::kAmpAttack, "Attack", theme::kMint),
      ampDecay_(processor.parameters, params::kAmpDecay, "Decay", theme::kMint),
      ampSustain_(processor.parameters, params::kAmpSustain, "Sustain", theme::kMint),
      ampRelease_(processor.parameters, params::kAmpRelease, "Release", theme::kMint),
      eqLow_(processor.parameters, params::kEqLowFreq, params::kEqLowDb, "Low EQ", theme::kMint, theme::kViolet),
      eqMid_(processor.parameters, params::kEqMidFreq, params::kEqMidDb, "Mid EQ", theme::kMint, theme::kViolet),
      eqHigh_(processor.parameters, params::kEqHighFreq, params::kEqHighDb, "High EQ", theme::kMint, theme::kViolet),
      stereoWidth_(processor.parameters, params::kStereoWidth, "Top width", theme::kMint),
      output_(processor.parameters, params::kOutput, "Output", theme::kMint),
      ceiling_(processor.parameters, params::kCeiling, "Ceiling", theme::kMint),
      limiter_(processor.parameters, params::kLimiterEnabled, "Limiter", theme::kMint),
      meter_(processor),
      waveform_(processor),
      processor_(processor) {
    std::ranges::for_each(std::array<juce::Component*, 32>{
             &sweepOctaves_, &sweepMs_, &sweepShape_, &keyTracking_, &snapOctaves_, &snapMs_,
             &bodyTone_, &knockHz_, &knockDb_, &toneHz_, &subLevel_, &subOctave_, &startPhase_,
             &clickType_, &clickLevel_, &clickHz_, &clickMs_, &clickPunch_, &attack_, &hold_, &decay_, &decayShape_, &lowCut_,
             &eqLow_, &eqMid_, &eqHigh_, &stereoWidth_, &output_, &ceiling_, &limiter_, &meter_, &waveform_},
        [this](juce::Component* component) { addAndMakeVisible(component); });

    std::ranges::for_each(std::array<juce::Component*, 5>{&ampEnabled_, &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    const std::array<juce::StringArray, 6> groups{{
        {params::kSweepOctaves, params::kSweepMs, params::kSweepShape, params::kSnapOctaves, params::kSnapMs},
        {params::kBodyTone, params::kKnockHz, params::kKnockDb, params::kToneHz, params::kSubLevel, params::kSubOctave, params::kStartPhase},
        {params::kClickType, params::kClickLevel, params::kClickHz, params::kClickMs, params::kClickPunch},
        {params::kAttackMs, params::kHoldMs, params::kDecayMs, params::kDecayShape, params::kLowCutHz},
        {params::kAmpEnabled, params::kAmpAttack, params::kAmpDecay, params::kAmpSustain, params::kAmpRelease},
        {params::kEqLowFreq, params::kEqLowDb, params::kEqMidFreq, params::kEqMidDb, params::kEqHighFreq, params::kEqHighDb,
         params::kStereoWidth, params::kOutput, params::kCeiling, params::kLimiterEnabled}
    }};
    const std::array<juce::String, 6> names{"dive", "body", "transient", "tail", "adsr", "output"};
    const std::array<juce::String, 6> titles{"Pitch dive", "Body & sub", "Transient", "One-shot tail", "MIDI envelope", "Output"};
    std::ranges::for_each(std::views::iota(size_t{0}, groups.size()), [&](size_t i) {
        sectionLocks_.push_back(std::make_unique<SectionLock>(processor.parameters, names[i], groups[i], titles[i]));
        addAndMakeVisible(*sectionLocks_.back());
    });
    sweepOctaves_.setTooltip("How far above the played MIDI note the opening of the kick starts.");
    keyTracking_.setTooltip(::suite::eq::kKeyTrackingTip);
    sweepMs_.setTooltip("Time for the pitch dive to reach the incoming MIDI note.");
    bodyTone_.setTooltip("Add harmonics to give the kick a more forceful, audible body.");
    knockDb_.setTooltip("A resonant mid-bass knock for more impact on smaller speakers.");
    subLevel_.setTooltip("Blend a clean fundamental under the kick body.");
    clickLevel_.setTooltip("Add a short beater transient ahead of the body.");
    eqLow_.setTooltip(::suite::eq::kLowTip);
    eqMid_.setTooltip(::suite::eq::kMidTip);
    eqHigh_.setTooltip(::suite::eq::kHighTip);
    ampEnabled_.setTooltip("Hold incoming MIDI notes until note-off and shape them with attack, decay, sustain and release.");
    ampEnabled_.button().getToggleStateValue().addListener(this);
    setAdvancedMode(false);
    refreshActivity();
    startTimerHz(10);
}

void KickSynthPanel::refreshActivity() {
    auto& routing = processor_.routing();
    const auto val = [&routing](const char* id) { return routing.displayValue(id); };
    const auto on = [&val](const char* id) { return val(id) >= 0.5f; };
    const bool hasSweep = val(params::kSweepOctaves) > 0.001f;
    const juce::String sweepOff = "Raise Dive range above 0 oct to use this.";
    showActivity(sweepMs_, hasSweep, "Time for the pitch dive to reach the incoming MIDI note.", sweepOff);
    showActivity(sweepShape_, hasSweep, "Curve of the pitch dive.", sweepOff);
    showActivity(snapMs_, val(params::kSnapOctaves) > 0.001f, "Duration of the initial pitch snap.",
                 "Raise Snap range above 0 oct to use this.");
    showActivity(knockHz_, std::abs(val(params::kKnockDb)) > 0.01f, "Resonant mid-bass knock frequency.",
                 "Raise Knock above 0.0 dB to hear the knock frequency.");
    const bool hasClick = val(params::kClickLevel) > 0.001f;
    const juce::String clickOff = "Raise Level above 0% to hear the synthesized click.";
    showActivity(clickType_, hasClick, "Choose the synthesized transient character.", clickOff);
    showActivity(clickHz_, hasClick, "Center frequency of the synthesized transient.", clickOff);
    showActivity(clickMs_, hasClick, "Length of the synthesized transient.", clickOff);

    showActivity(ceiling_, on(params::kLimiterEnabled), "Maximum peak level before the limiter clamps.", "Turn on Limiter to use Ceiling.");
    const bool held = on(params::kAmpEnabled);
    std::ranges::for_each(std::array<Knob*, 5>{ &attack_, &hold_, &decay_, &decayShape_, &lowCut_ }, [held](Knob* knob) {
        showActivity(*knob, !held, "Shape the one-shot kick envelope and low cut.", "Turn off Hold MIDI notes to use the one-shot tail.");
    });
    std::ranges::for_each(std::array<Knob*, 4>{ &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_ }, [held](Knob* knob) {
        showActivity(*knob, held, "Shape the held MIDI note envelope.", "Turn on Hold MIDI notes to use the MIDI envelope.");
    });
}

KickSynthPanel::~KickSynthPanel() {
    ampEnabled_.button().getToggleStateValue().removeListener(this);
}

void KickSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setControlsVisible({ &sweepShape_, &snapOctaves_, &snapMs_, &knockHz_,
                         &toneHz_, &subOctave_, &startPhase_, &attack_, &decayShape_, &lowCut_ }, advanced);
    heldNotes_ = ampEnabled_.button().getToggleState();
    setControlsVisible({ &hold_, &decay_, sectionLocks_[3].get() }, advanced || !heldNotes_);
    setControlsVisible({ &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_, sectionLocks_[4].get() }, advanced || heldNotes_);
    refreshActivity();
    resized();
    repaint();
}

std::array<juce::Rectangle<int>, 7> KickSynthPanel::sections() const {
    auto area = getLocalBounds();
    const int gap = advancedMode_ ? kCardGap : kCompactGap;
    const auto scope = area.removeFromTop(advancedMode_ ? 112 : 98);
    area.removeFromTop(gap);
    const auto output = area.removeFromBottom(advancedMode_ ? 190 : 162);
    area.removeFromBottom(gap);
    juce::Rectangle<int> adsr;
    if (advancedMode_) {
        adsr = area.removeFromBottom(176);
        area.removeFromBottom(gap);
    }
    auto top = area.removeFromTop((area.getHeight() - gap) / 2);
    area.removeFromTop(gap);
    const int half = (area.getWidth() - kCardGap) / 2;
    const auto dive = top.removeFromLeft(half);
    // The transient row holds six controls, so it gets 60% of the width.
    const auto transient = area.removeFromLeft((area.getWidth() - kCardGap) * 3 / 5);
    const auto tail = area.withTrimmedLeft(kCardGap);
    if (!advancedMode_ && heldNotes_) adsr = tail;
    return { dive, top.withTrimmedLeft(kCardGap), transient, tail, adsr, output, scope };
}

void KickSynthPanel::paint(juce::Graphics& g) {
    const auto cards = sections();
    drawCard(g, cards[0], "Pitch dive", theme::kViolet);
    drawCard(g, cards[1], "Body & sub", theme::kMint);
    drawCard(g, cards[2], "Transient", theme::kCoral);
    if (advancedMode_ || !heldNotes_) drawCard(g, cards[3], "One-shot tail", theme::kMint);
    if (advancedMode_ || heldNotes_) drawCard(g, cards[4], "MIDI envelope", theme::kViolet);
    drawCard(g, cards[5], "Output", theme::kMint);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    auto mode = cards[6].withLeft(cards[6].getRight() - kMidiModeWidth);
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(mode.toFloat(), 9);
    drawSmallLabel(g, mode.reduced(12, 4).removeFromTop(18), "MIDI playback", theme::kText.withAlpha(0.85f));
}

void KickSynthPanel::resized() {
    const auto cards = sections();
    const int rows = advancedMode_ ? 2 : 1;
    std::ranges::for_each(std::views::iota(size_t{0}, sectionLocks_.size()), [&](size_t i) {
        sectionLocks_[i]->setBounds(cards[i].getRight() - kSectionLockWidth - 10, cards[i].getY() + 3,
                                    kSectionLockWidth, kSectionLockHeight);
    });
    auto scope = cards[6];
    auto mode = scope.removeFromRight(kMidiModeWidth);
    ampEnabled_.setBounds(mode.withTrimmedTop(18).reduced(8, 0));
    waveform_.setBounds(scope.withTrimmedRight(kCardGap));
    placeRow(cards[0], 0, rows, { &sweepOctaves_, &sweepMs_, &sweepShape_, &keyTracking_ });
    placeRow(cards[1], 0, rows, { &subLevel_, &knockDb_, &bodyTone_ });
    {
        const int innerTop = cards[2].getY() + kCardHeader;
        const int innerHeight = std::max(0, cards[2].getHeight() - kCardHeader - 4);
        const int rowHeight = innerHeight / std::max(1, rows);
        auto top = juce::Rectangle<int>(cards[2].getX() + 8, innerTop, cards[2].getWidth() - 16, rowHeight);
        layoutRow(top, { &clickType_, &clickLevel_, &clickHz_, &clickMs_, &clickPunch_ }, 3);
    }
    auto tailCard = cards[3];
    if (!advancedMode_)
        tailCard = tailCard.withSizeKeepingCentre(std::min(tailCard.getWidth(), 280), tailCard.getHeight());
    placeRow(tailCard, 0, rows, { &hold_, &decay_, &decayShape_ });
    if (advancedMode_ || heldNotes_)
        placeRow(cards[4], 0, 1, { &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_ });
    auto outputRow = cards[5].withTrimmedTop(kCardHeader).reduced(8, 2);
    layoutWeightedRow(outputRow, {
        { &eqLow_, 13.0f }, { &eqMid_, 13.0f }, { &eqHigh_, 13.0f },
        { &stereoWidth_, 9.5f }, { &output_, 9.5f }, { &ceiling_, 9.5f },
        { &limiter_, 9.5f }, { &meter_, 10.5f }
    }, 4);
    if (!advancedMode_) return;
    placeRow(cards[0], 1, 2, { &snapOctaves_, &snapMs_ });
    placeRow(cards[1], 1, 2, { &knockHz_, &toneHz_, &subOctave_, &startPhase_ });
    placeRow(cards[3], 1, 2, { &attack_, &lowCut_ });
}

PeakMeter::PeakMeter(OpenEdmKickProcessor& processor) : processor_(processor) {
    setTooltip("Output peak after the limiter, in dBFS. The number holds the highest peak for 1 s. "
               "The box lights above 0 dBFS. Click the meter to clear it.");
    startTimerHz(30);
}

void PeakMeter::timerCallback() {
    const float peakDb = juce::Decibels::gainToDecibels(processor_.displayPeak.exchange(0.0f), -100.0f);
    const double now = juce::Time::getMillisecondCounterHiRes();
    levelDb_ = std::max(peakDb, levelDb_ - 1.5f);
    if (peakDb >= heldDb_ || now - heldAtMs_ > 1000.0) {
        heldDb_ = peakDb;
        heldAtMs_ = now;
    }
    clipped_ = clipped_ || peakDb > 0.0f;
    repaint();
}

void PeakMeter::mouseDown(const juce::MouseEvent&) {
    clipped_ = false;
    repaint();
}

void PeakMeter::paint(juce::Graphics& g) {
    auto area = getLocalBounds().reduced(4, 6).withTrimmedBottom(6);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Output peak", area.removeFromTop(16), juce::Justification::centred);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    auto readout = juce::String("-inf dB");
    if (heldDb_ > -99.0f) readout = juce::String(heldDb_, 1) + " dB";
    g.drawText(readout, area.removeFromTop(20), juce::Justification::centred);
    area.removeFromTop(3);

    auto bar = area.removeFromTop(14);
    const auto clip = bar.removeFromRight(14).toFloat();
    bar.removeFromRight(4);
    const auto track = bar.toFloat();
    const auto xFor = [&track](float db) {
        return track.getX() + track.getWidth() * juce::jmap(std::clamp(db, kMeterFloorDb, 0.0f), kMeterFloorDb, 0.0f, 0.0f, 1.0f);
    };
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(track, 4.0f);
    g.setColour(theme::kMint);
    g.fillRoundedRectangle(track.withRight(xFor(levelDb_)), 4.0f);
    g.setColour(theme::kText);
    if (heldDb_ > kMeterFloorDb) g.fillRect(juce::Rectangle<float>(xFor(heldDb_) - 1.0f, track.getY(), 2.0f, track.getHeight()));
    if (processor_.routing().displayValue(params::kLimiterEnabled) >= 0.5f) {
        g.setColour(theme::kViolet);
        g.fillRect(juce::Rectangle<float>(xFor(processor_.routing().displayValue(params::kCeiling)) - 1.0f,
                                          track.getY() - 3.0f, 2.0f, track.getHeight() + 6.0f));
    }
    g.setColour(clipped_ ? theme::kCoral : theme::kInk);
    g.fillRoundedRectangle(clip, 3.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(clip.reduced(0.5f), 3.0f, 1.0f);

    g.setColour(theme::kText.withAlpha(0.75f));
    g.setFont(juce::FontOptions(12.0f));
    const auto ticks = area.removeFromTop(20).toFloat();
    std::ranges::for_each(std::array{ -24.0f, -12.0f, 0.0f }, [&](float db) {
        g.fillRect(xFor(db) - 0.5f, ticks.getY(), 1.0f, 3.0f);
        g.drawText(juce::String(juce::roundToInt(db)), juce::Rectangle<float>(xFor(db) - 15.0f, ticks.getY() + 4.0f, 30.0f, 15.0f),
                   juce::Justification::centred);
    });
}

EffectsPanel::EffectsPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      reverseEnabled_(processor.parameters, params::kReverseEnabled, "Reverse MIDI", theme::kCoral),
      highPassMidi_(processor.parameters, params::kHighPassMidi, "Sweep", theme::kCoral, params::kHighPassSyncEnabled),
      lowPassMidi_(processor.parameters, params::kLowPassMidi, "Sweep", theme::kViolet, params::kLowPassSyncEnabled),
      midiVolume_(processor.parameters, params::kMidiVolume, "MIDI volume", theme::kMint),
      midiExpression_(processor.parameters, params::kMidiExpression, "Expression", theme::kMint),
      midiBrightness_(processor.parameters, params::kMidiBrightness, "Brightness", theme::kMint),
      reverseMode_(processor.parameters, params::kReverseMode, "Reverse", theme::kCoral),
      reverseMix_(processor.parameters, params::kReverseMix, "Mix", theme::kCoral),
      clockSync_(processor.parameters, params::kClockSync, "Sync", theme::kMint),
      tempo_(processor.parameters, params::kTempoBpm, "Tempo", theme::kMint),
      timeSig_(processor.parameters, params::kTimeSignature, "Time", theme::kMint),
      distType_(processor.parameters, params::kDistType, "Type", theme::kCoral),
      distDrive_(processor.parameters, params::kDistDrive, "Drive", theme::kCoral),
      distTone_(processor.parameters, params::kDistTone, "Tone", theme::kCoral),
      distMix_(processor.parameters, params::kDistMix, "Mix", theme::kCoral),
      compThresh_(processor.parameters, params::kCompThresh, "Threshold", theme::kMint),
      compRatio_(processor.parameters, params::kCompRatio, "Ratio", theme::kMint),
      compAttack_(processor.parameters, params::kCompAttack, "Attack", theme::kMint),
      compRelease_(processor.parameters, params::kCompRelease, "Release", theme::kMint),
      compMakeup_(processor.parameters, params::kCompMakeup, "Makeup", theme::kMint),
      compMix_(processor.parameters, params::kCompMix, "Mix", theme::kMint),
      reverbSize_(processor.parameters, params::kReverbSize, "Size", theme::kViolet),
      reverbDecay_(processor.parameters, params::kReverbDecay, "Decay", theme::kViolet),
      reverbDamp_(processor.parameters, params::kReverbDamp, "Damp", theme::kViolet),
      reverbGate_(processor.parameters, params::kReverbGate, "Gate", theme::kViolet),
      reverbMix_(processor.parameters, params::kReverbMix, "Mix", theme::kViolet),
      highPass_(processor.parameters, params::kHighPass, "High pass", theme::kMint),
      lowPass_(processor.parameters, params::kLowPass, "Low pass", theme::kMint),
      highPassSync_(processor.parameters, params::kHighPassSyncEnabled, "High-pass sync", theme::kCoral),
      highPassSyncRate_(processor.parameters, params::kHighPassSyncRate, "High-pass rate", theme::kCoral),
      lowPassSync_(processor.parameters, params::kLowPassSyncEnabled, "Low-pass sync", theme::kViolet),
      lowPassSyncRate_(processor.parameters, params::kLowPassSyncRate, "Low-pass rate", theme::kViolet) {
    std::ranges::for_each(std::array<juce::Component*, 25>{
        &reverseMode_, &reverseMix_, &clockSync_, &tempo_, &timeSig_,
        &distType_, &distDrive_, &distTone_, &distMix_,
        &compThresh_, &compRatio_, &compAttack_, &compRelease_, &compMakeup_, &compMix_,
        &reverbSize_, &reverbDecay_, &reverbDamp_, &reverbGate_, &reverbMix_,
        &highPass_, &lowPass_, &highPassSyncRate_, &lowPassSyncRate_, &reverseEnabled_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    addChildComponent(highPassSync_);
    addChildComponent(lowPassSync_);
    std::ranges::for_each(std::array<juce::Component*, 5>{
        &highPassMidi_, &lowPassMidi_, &midiVolume_, &midiExpression_, &midiBrightness_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    const std::array<juce::StringArray, 4> groups{{
        {params::kReverseEnabled, params::kReverseMode, params::kReverseMix, params::kClockSync, params::kTempoBpm, params::kTimeSignature},
        {params::kDistType, params::kDistDrive, params::kDistTone, params::kDistMix},
        {params::kCompThresh, params::kCompRatio, params::kCompAttack, params::kCompRelease, params::kCompMakeup, params::kCompMix},
        {params::kReverbSize, params::kReverbDecay, params::kReverbDamp, params::kReverbGate, params::kReverbMix,
         params::kHighPass, params::kLowPass, params::kHighPassMidi, params::kLowPassMidi,
         params::kMidiVolume, params::kMidiExpression, params::kMidiBrightness}
    }};
    const std::array<juce::String, 4> titles{"Reverse", "Drive", "Compressor", "Reverb & filters"};
    std::ranges::for_each(std::views::iota(size_t{0}, groups.size()), [&](size_t i) {
        sectionLocks_.push_back(std::make_unique<SectionLock>(processor.parameters, "fx_" + juce::String(i), groups[i], titles[i]));
        addAndMakeVisible(*sectionLocks_.back());
    });
    setAdvancedMode(false);
    refreshActivity();
    startTimerHz(10);
}

void EffectsPanel::refreshActivity() {
    auto& routing = processor_.routing();
    const auto value = [&routing](const char* id) { return routing.displayValue(id); };
    const bool reverseMidi = value(params::kReverseEnabled) >= 0.5f;
    showActivity(reverseMode_, !reverseMidi, "Choose how a reverse hit lines up with the host.",
                 "Reverse MIDI is on. Turn it off to pick a mode.");
    showActivity(reverseMix_, reverseMidi || value(params::kReverseMode) >= 0.5f,
                 "Wet/dry mix of the reverse hit.", "Pick a reverse mode or turn on Reverse MIDI.");
    const bool driving = value(params::kDistMix) > 0.001f;
    showActivity(distType_, driving, "Tape, tube, hard clip, or wavefold.", "Raise Drive mix to hear the drive.");
    showActivity(distDrive_, driving, "How hard the drive pushes the kick.", "Raise Drive mix to hear the drive.");
    showActivity(distTone_, driving, "Darken or brighten the driven sound.", "Raise Drive mix to hear the drive.");
    const bool compressing = value(params::kCompMix) > 0.001f;
    showActivity(compThresh_, compressing, "Level where the compressor starts working.", "Raise Comp mix to hear the compressor.");
    showActivity(compRatio_, compressing, "How much the compressor holds the peaks down.", "Raise Comp mix to hear the compressor.");
    const bool verb = value(params::kReverbMix) > 0.001f;
    showActivity(reverbSize_, verb, "Room size of the reverb.", "Raise Reverb mix to hear the reverb.");
    showActivity(reverbDecay_, verb, "How long the reverb rings.", "Raise Reverb mix to hear the reverb.");
    showActivity(reverbDamp_, verb, "How quickly the high end of the reverb dies.", "Raise Reverb mix to hear the reverb.");
    showActivity(reverbGate_, verb, "Close the reverb after this many milliseconds. 0 leaves it open.", "Raise Reverb mix to hear the reverb.");
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setControlsVisible({ &distTone_, &compAttack_, &compRelease_, &compMakeup_,
                         &reverbDamp_, &reverbGate_, &highPass_, &lowPass_ }, advanced);
    highPassSyncRate_.setVisible(false);
    lowPassSyncRate_.setVisible(false);
    highPassMidi_.setVisible(false);
    lowPassMidi_.setVisible(false);
    midiVolume_.setVisible(false);
    midiExpression_.setVisible(false);
    midiBrightness_.setVisible(false);
    resized();
    repaint();
}

std::array<juce::Rectangle<int>, 4> EffectsPanel::sections() const {
    auto area = getLocalBounds();
    const int row = (area.getHeight() - 3 * kCardGap) / 4;
    const auto reverse = area.removeFromTop(row);
    area.removeFromTop(kCardGap);
    const auto drive = area.removeFromTop(row);
    area.removeFromTop(kCardGap);
    const auto comp = area.removeFromTop(row);
    return { reverse, drive, comp, area.withTrimmedTop(kCardGap) };
}

void EffectsPanel::paint(juce::Graphics& g) {
    const auto cards = sections();
    const std::array<juce::String, 4> titles{"Reverse", "Drive", "Compressor", "Reverb & filters"};
    const std::array<juce::Colour, 4> accents{theme::kViolet, theme::kCoral, theme::kMint, theme::kViolet};
    std::ranges::for_each(std::views::iota(size_t{0}, cards.size()), [&](size_t i) {
        drawCard(g, cards[i], titles[i], accents[i]);
    });
}

void EffectsPanel::resized() {
    auto cards = sections();
    std::ranges::for_each(std::views::iota(size_t{0}, cards.size()), [&](size_t i) {
        sectionLocks_[i]->setBounds(14, cards[i].getY() + 48, kSectionLockWidth, kSectionLockHeight);
        cards[i] = cards[i].withTrimmedLeft(154).reduced(8, 8);
    });
    layoutRow(cards[0], { &reverseEnabled_, &reverseMode_, &reverseMix_, &clockSync_, &tempo_, &timeSig_ });
    layoutRow(cards[1], { &distType_, &distDrive_, &distTone_, &distMix_ });
    layoutRow(cards[2], { &compThresh_, &compRatio_, &compMix_, &compAttack_, &compRelease_, &compMakeup_ });
    if (!advancedMode_) {
        layoutRow(cards[3], { &reverbSize_, &reverbDecay_, &reverbMix_ });
        return;
    }
    layoutRow(cards[3], { &reverbSize_, &reverbDecay_, &reverbDamp_, &reverbGate_, &reverbMix_, &highPass_, &lowPass_ });
}

} // namespace openedmkick::ui
