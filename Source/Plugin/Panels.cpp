#include "Panels.h"

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
    inputs.push_back(static_cast<float>(processor_.userClickSlot()));
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
    g.setFont(juce::FontOptions(compact ? 12.0f : 14.0f));
    g.drawText("MIDI note", readout.removeFromTop(compact ? 16 : 21), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(compact ? 32.0f : 40.0f, juce::Font::bold));
    g.drawText(noteLabel(note), readout.removeFromTop(compact ? 34 : 46), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(compact ? 14.0f : 17.0f));
    g.drawText(params::formatHz(core::noteFrequencyHz(note)), readout, juce::Justification::topLeft);

    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("One-shot waveform", area.removeFromTop(20), juce::Justification::centredLeft);
    const auto scale = area.removeFromBottom(19);
    const auto length = static_cast<size_t>(hit_.getNumSamples());
    const int durationMs = juce::roundToInt(1000.0 * static_cast<double>(length) / OpenEdmKickProcessor::hitSampleRate);
    g.setFont(juce::FontOptions(12.0f));
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
            g.drawText(juce::String(ms), juce::roundToInt(x) - 18, scale.getY(), 36, scale.getHeight(),
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
      limiter_(processor.parameters, params::kLimiter, "Limiter", theme::kMint),
      masterBypass_(processor.parameters, params::kMasterBypass, "Bypass", theme::kCoral),
      meter_(processor),
      waveform_(processor),
      processor_(processor),
      clickSampleOn_(processor.parameters, params::kClickSampleOn, "Sample", theme::kCoral),
      clickSampleSource_(processor.parameters, params::kClickSampleSource, "Click", theme::kCoral),
      clickSampleLevel_(processor.parameters, params::kClickSampleLevel, "Click level", theme::kCoral),
      clickSampleStart_(processor.parameters, params::kClickSampleStart, "Start", theme::kCoral),
      clickSamplePitch_(processor.parameters, params::kClickSamplePitch, "Pitch", theme::kCoral),
      clickSampleHp_(processor.parameters, params::kClickSampleHp, "High pass", theme::kCoral),
      loadClick_("Load") {
    std::ranges::for_each(std::array<juce::Component*, 32>{
             &sweepOctaves_, &sweepMs_, &sweepShape_, &snapOctaves_, &snapMs_,
             &bodyTone_, &knockHz_, &knockDb_, &toneHz_, &subLevel_, &subOctave_, &startPhase_,
             &clickType_, &clickLevel_, &clickHz_, &clickMs_, &clickPunch_, &attack_, &hold_, &decay_, &decayShape_, &lowCut_,
             &eqLow_, &eqMid_, &eqHigh_, &stereoWidth_, &output_, &ceiling_, &limiter_, &masterBypass_, &meter_, &waveform_},
        [this](juce::Component* component) { addAndMakeVisible(component); });

    std::ranges::for_each(std::array<juce::Component*, 5>{&ampEnabled_, &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    std::ranges::for_each(std::array<juce::Component*, 7>{
        &clickSampleOn_, &clickSampleSource_, &clickSampleLevel_, &clickSampleStart_, &clickSamplePitch_,
        &clickSampleHp_, &loadClick_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    loadClick_.setComponentID("loadClickSample");
    loadClick_.setTitle("Load click sample");
    loadClick_.setColour(juce::TextButton::buttonColourId, theme::kSteel);
    loadClick_.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>("Load a short click sample", juce::File{}, "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& picker) {
                const auto file = picker.getResult();
                if (file == juce::File{}) return;
                processor_.loadUserClickSample(file);
            });
    };
    const std::array<juce::StringArray, 6> groups{{
        {params::kSweepOctaves, params::kSweepMs, params::kSweepShape, params::kSnapOctaves, params::kSnapMs},
        {params::kBodyTone, params::kKnockHz, params::kKnockDb, params::kToneHz, params::kSubLevel, params::kSubOctave, params::kStartPhase},
        {params::kClickType, params::kClickLevel, params::kClickHz, params::kClickMs, params::kClickPunch,
         params::kClickSampleOn, params::kClickSampleSource, params::kClickSampleLevel, params::kClickSampleStart,
         params::kClickSamplePitch, params::kClickSampleHp},
        {params::kAttackMs, params::kHoldMs, params::kDecayMs, params::kDecayShape, params::kLowCutHz},
        {params::kAmpEnabled, params::kAmpAttack, params::kAmpDecay, params::kAmpSustain, params::kAmpRelease},
        {params::kEqLowFreq, params::kEqLowDb, params::kEqMidFreq, params::kEqMidDb, params::kEqHighFreq, params::kEqHighDb,
         params::kStereoWidth, params::kOutput, params::kCeiling, params::kLimiter, params::kMasterBypass}
    }};
    const std::array<juce::String, 6> names{"dive", "body", "transient", "tail", "adsr", "output"};
    const std::array<juce::String, 6> titles{"Pitch dive", "Body & sub", "Transient", "One-shot tail", "MIDI envelope", "Output"};
    std::ranges::for_each(std::views::iota(size_t{0}, groups.size()), [&](size_t i) {
        sectionLocks_.push_back(std::make_unique<SectionLock>(processor.parameters, names[i], groups[i], titles[i]));
        addAndMakeVisible(*sectionLocks_.back());
    });
    sweepOctaves_.setTooltip("How far above the played MIDI note the opening of the kick starts.");
    sweepMs_.setTooltip("Time for the pitch dive to reach the incoming MIDI note.");
    bodyTone_.setTooltip("Add harmonics to give the kick a more forceful, audible body.");
    knockDb_.setTooltip("A resonant mid-bass knock for more impact on smaller speakers.");
    subLevel_.setTooltip("Blend a clean fundamental under the kick body.");
    clickLevel_.setTooltip("Add a short beater transient ahead of the body.");
    clickSampleOn_.setTooltip("Mix a sample click with the transient. Off leaves existing sounds unchanged.");
    eqLow_.setTooltip("Inner cap sets Low EQ gain (-12 to +12 dB); outer collar snaps Low EQ to notes (E1 to E3).");
    eqMid_.setTooltip("Inner cap sets Mid EQ gain (-12 to +12 dB); outer collar snaps Mid EQ to notes (B4 to B6).");
    eqHigh_.setTooltip("Inner cap sets High EQ gain (-12 to +12 dB); outer collar snaps High EQ to notes (A#7 to A#9).");
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

    showActivity(ceiling_, on(params::kLimiter), "Maximum peak level before the limiter clamps.", "Turn on Limiter to use Ceiling.");
    const bool sample = on(params::kClickSampleOn);
    const juce::String sampleOff = "Turn on Sample to hear the sample click.";
    showActivity(clickSampleSource_, sample, "Play a built-in click or the WAV you loaded.", sampleOff);
    showActivity(clickSampleLevel_, sample, "Level of the sample click.", sampleOff);
    showActivity(clickSampleStart_, sample, "Skip into the sample before it plays.", sampleOff);
    showActivity(clickSamplePitch_, sample, "Playback pitch of the sample click, in semitones.", sampleOff);
    showActivity(clickSampleHp_, sample, "High-pass the sample click.", sampleOff);
    showActivity(loadClick_, sample, "Load a short WAV for the sample click. The audio is stored in the session.", sampleOff);
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
    setControlsVisible({ &clickSampleSource_, &clickSampleLevel_, &clickSampleStart_, &clickSamplePitch_,
                         &clickSampleHp_, &loadClick_ }, advanced);
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
    drawSmallLabel(g, mode.reduced(12, 4).removeFromTop(16), "MIDI playback", theme::kTextDim);
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
    placeRow(cards[0], 0, rows, { &sweepOctaves_, &sweepMs_, &sweepShape_ });
    placeRow(cards[1], 0, rows, { &subLevel_, &knockDb_, &bodyTone_ });
    {
        const int innerTop = cards[2].getY() + kCardHeader;
        const int innerHeight = std::max(0, cards[2].getHeight() - kCardHeader - 4);
        const int rowHeight = innerHeight / std::max(1, rows);
        auto top = juce::Rectangle<int>(cards[2].getX() + 8, innerTop, cards[2].getWidth() - 16, rowHeight);
        clickSampleOn_.setBounds(top.removeFromRight(std::min(96, std::max(40, top.getWidth() / 5))));
        top.removeFromRight(3);
        layoutRow(top, { &clickType_, &clickLevel_, &clickHz_, &clickMs_, &clickPunch_ }, 3);
    }
    if (advancedMode_)
        placeRow(cards[2], 1, 2, { &clickSampleSource_, &clickSampleLevel_, &clickSampleStart_,
                                   &clickSamplePitch_, &clickSampleHp_, &loadClick_ });
    placeRow(cards[3], 0, rows, { &hold_, &decay_, &decayShape_ });
    if (advancedMode_ || heldNotes_)
        placeRow(cards[4], 0, 1, { &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_ });
    auto outputRow = cards[5].withTrimmedTop(kCardHeader).reduced(8, 2);
    layoutWeightedRow(outputRow, {
        { &eqLow_, 13.0f }, { &eqMid_, 13.0f }, { &eqHigh_, 13.0f },
        { &stereoWidth_, 9.5f }, { &output_, 9.5f }, { &ceiling_, 9.5f },
        { &limiter_, 9.5f }, { &masterBypass_, 9.5f }, { &meter_, 10.5f }
    }, 4);
    if (!advancedMode_) return;
    placeRow(cards[0], 1, 2, { &snapOctaves_, &snapMs_ });
    placeRow(cards[1], 1, 2, { &knockHz_, &toneHz_, &subOctave_, &startPhase_ });
    placeRow(cards[3], 1, 2, { &attack_, &lowCut_ });
}

LayerPage::LayerPage(OpenEdmKickProcessor& processor, int layer)
    : layer_(std::clamp(layer, 0, core::kNumSoundLayers - 1)),
      enabled_(processor.parameters, params::layerId(layer_, params::kLayerEnabled), "Layer on", theme::kViolet),
      sound_(processor.parameters, params::layerId(layer_, params::kLayerSound), "Sound", theme::kCoral),
      level_(processor.parameters, params::layerId(layer_, params::kLayerGain), "Level", theme::kMint),
      pitch_(processor.parameters, params::layerId(layer_, params::kLayerTranspose), "Pitch", theme::kViolet),
      decay_(processor.parameters, params::layerId(layer_, params::kLayerDecay), "Decay", theme::kMint),
      highPass_(processor.parameters, params::layerId(layer_, params::kLayerHighPass), "High pass", theme::kCoral),
      lowPass_(processor.parameters, params::layerId(layer_, params::kLayerLowPass), "Low pass", theme::kCoral) {
    std::ranges::for_each(std::array<juce::Component*, 7>{
        &enabled_, &sound_, &level_, &pitch_, &decay_, &highPass_, &lowPass_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    layerKnobs_[0] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityCurve), "Velocity curve", theme::kMint);
    addAndMakeVisible(*layerKnobs_[0]);
    layerKnobs_[1] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityTimbre), "Velocity timbre", theme::kMint);
    addAndMakeVisible(*layerKnobs_[1]);
    layerKnobs_[2] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerKeyLow), "Key low", theme::kMint);
    addAndMakeVisible(*layerKnobs_[2]);
    layerKnobs_[3] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerKeyHigh), "Key high", theme::kMint);
    addAndMakeVisible(*layerKnobs_[3]);
    layerKnobs_[4] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityLow), "Velocity low", theme::kMint);
    addAndMakeVisible(*layerKnobs_[4]);
    layerKnobs_[5] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityHigh), "Velocity high", theme::kMint);
    addAndMakeVisible(*layerKnobs_[5]);
    layerKnobs_[6] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerToneScale), "Tone scale", theme::kMint);
    addAndMakeVisible(*layerKnobs_[6]);
    layerKnobs_[7] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerSweepScale), "Dive scale", theme::kMint);
    addAndMakeVisible(*layerKnobs_[7]);
    layerKnobs_[8] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerPan), "Pan", theme::kMint);
    addAndMakeVisible(*layerKnobs_[8]);
    layerToggles_[0] = std::make_unique<Toggle>(processor.parameters, params::layerId(layer_, params::kLayerSolo), "Solo", theme::kViolet);
    addAndMakeVisible(*layerToggles_[0]);
    layerToggles_[1] = std::make_unique<Toggle>(processor.parameters, params::layerId(layer_, params::kLayerPolarity), "Invert polarity", theme::kViolet);
    addAndMakeVisible(*layerToggles_[1]);
    juce::StringArray ids;
    std::ranges::for_each(std::array<const char*, 18>{params::kLayerEnabled, params::kLayerSound, params::kLayerGain,
        params::kLayerTranspose, params::kLayerDecay, params::kLayerHighPass, params::kLayerLowPass,
        params::kLayerVelocityCurve, params::kLayerVelocityTimbre, params::kLayerKeyLow, params::kLayerKeyHigh,
        params::kLayerVelocityLow, params::kLayerVelocityHigh, params::kLayerToneScale,
        params::kLayerSweepScale, params::kLayerSolo, params::kLayerPan, params::kLayerPolarity},
        [&](const char* key) { ids.add(params::layerId(layer_, key)); });
    const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer_));
    sectionLock_ = std::make_unique<SectionLock>(processor.parameters, "layer_" + juce::String(layer_), ids, "Layer " + letter);
    addAndMakeVisible(*sectionLock_);
    enabled_.setTooltip("Include this sound layer in every kick hit.");
}

void LayerPage::showEnabled(bool enabled, bool isKick, bool mutedBySolo) {
    const juce::String off = !enabled
        ? "This layer is off. Turn on Layer on to hear it."
        : "Another layer is soloed. Turn on Solo here or turn off Solo elsewhere to hear this layer.";
    const bool active = enabled && !mutedBySolo;
    showActivity(sound_, active, "Choose a synthesized kick, snare, clap, or hi-hat voice.", off);
    showActivity(level_, active, "Set this layer's mix level.", off);
    showActivity(pitch_, active, "Transpose the layer in semitones. Hats use this to tune their metal partials.", off);
    showActivity(decay_, active, "Set this layer's one-shot length. Kick layers scale their main kick decay by this value.", off);
    showActivity(highPass_, active, "Filter this layer before it is mixed with the other layers.", off);
    showActivity(lowPass_, active, "Filter this layer before it is mixed with the other layers.", off);
    showActivity(*layerToggles_[0], enabled, "Hear only enabled layers with Solo switched on.", "This layer is off. Turn on Layer on to hear it.");
    showActivity(*layerToggles_[1], active, "Reverse this layer's polarity before mixing it with the other layers.", off);
    static constexpr std::array<const char*, 9> kKnobTips{
        "Shape how MIDI velocity scales this layer's level.",
        "Scale timbre or brightness with MIDI velocity.",
        "Lowest MIDI note that triggers this layer.",
        "Highest MIDI note that triggers this layer.",
        "Lowest MIDI velocity that triggers this layer.",
        "Highest MIDI velocity that triggers this layer.",
        "Scale the layer's tone or harmonic character.",
        "Scale the kick pitch dive depth for this layer.",
        "Stereo pan position for this layer."
    };
    for (size_t i = 0; i < layerKnobs_.size(); ++i) {
        const bool knobActive = active && (i != 7 || isKick);
        const juce::String inactiveTip = (i == 7 && active && !isKick)
            ? "Dive scale only applies when Sound is set to Kick."
            : off;
        showActivity(*layerKnobs_[i], knobActive, kKnobTips[i], inactiveTip);
    }
}

void LayerPage::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    const int height = area.getHeight() / 4;
    const std::array<const char*, 4> titles{ "Layer mixer", "Pitch & filters", "Response", "MIDI zone" };
    const std::array<juce::Colour, 4> accents{ theme::kViolet, theme::kCoral, theme::kMint, theme::kMint };
    for (size_t i = 0; i < titles.size(); ++i) {
        auto strip = area.removeFromTop(height).withTrimmedBottom(5);
        drawCard(g, strip, titles[i], accents[i]);
    }
}

void LayerPage::resized() {
    sectionLock_->setBounds(14, 48, kSectionLockWidth, kSectionLockHeight);
    auto area = getLocalBounds().withTrimmedLeft(154).withTrimmedRight(10);
    const int rowHeight = area.getHeight() / 4;
    layoutRow(area.removeFromTop(rowHeight).reduced(0, 4), { &enabled_, &sound_, &level_, layerToggles_[0].get(), layerToggles_[1].get() }, 8);
    layoutRow(area.removeFromTop(rowHeight).reduced(0, 4), { &pitch_, &decay_, &highPass_, &lowPass_, layerKnobs_[8].get() }, 8);
    layoutRow(area.removeFromTop(rowHeight).reduced(0, 4), { layerKnobs_[0].get(), layerKnobs_[1].get(), layerKnobs_[6].get(), layerKnobs_[7].get() }, 8);
    layoutRow(area.reduced(0, 4), { layerKnobs_[2].get(), layerKnobs_[3].get(), layerKnobs_[4].get(), layerKnobs_[5].get() }, 8);
}

LayerPanel::LayerPanel(OpenEdmKickProcessor& processor) : processor_(processor) {
    std::ranges::for_each(std::views::iota(0, core::kNumSoundLayers), [&](int layer) {
        const auto index = static_cast<size_t>(layer);
        pages_[index] = std::make_unique<LayerPage>(processor, layer);
        addAndMakeVisible(*pages_[index]);
        auto& tab = tabs_[index];
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
        tab.setComponentID("layer_tab_" + letter.toLowerCase());
        tab.setClickingTogglesState(false);
        tab.setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
        tab.onClick = [this, layer] { selectLayer(layer); };
        addAndMakeVisible(tab);
    });
    selectLayer(0);
    refreshActivity();
    startTimerHz(10);
}

void LayerPanel::refreshActivity() {
    auto& routing = processor_.routing();
    bool anySoloed = false;
    for (int layer = 0; layer < core::kNumSoundLayers; ++layer) {
        const bool enabled = routing.displayValue(params::layerId(layer, params::kLayerEnabled)) >= 0.5f;
        const bool solo = routing.displayValue(params::layerId(layer, params::kLayerSolo)) >= 0.5f;
        if (enabled && solo) {
            anySoloed = true;
            break;
        }
    }
    std::ranges::for_each(std::views::iota(0, core::kNumSoundLayers), [&](int layer) {
        const auto index = static_cast<size_t>(layer);
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
        const auto soundId = params::layerId(layer, params::kLayerSound);
        const auto& sounds = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters.getParameter(soundId))->choices;
        const bool enabled = routing.displayValue(params::layerId(layer, params::kLayerEnabled)) >= 0.5f;
        const bool solo = routing.displayValue(params::layerId(layer, params::kLayerSolo)) >= 0.5f;
        const bool mutedBySolo = enabled && anySoloed && !solo;
        const int soundIndex = juce::roundToInt(routing.displayValue(soundId));
        const auto label = letter + " " + sounds[soundIndex];
        tabs_[index].setButtonText(label);
        tabs_[index].setTitle(label);
        const juce::String inactiveTip = !enabled
            ? "Layer " + letter + " is off. Turn on Layer on in its tab to hear it."
            : "Layer " + letter + " is muted because another layer is soloed.";
        showActivity(tabs_[index], enabled && !mutedBySolo, "Select layer " + letter + " to edit its sound, mix, and zone.", inactiveTip);
        pages_[index]->showEnabled(enabled, soundIndex == 0, mutedBySolo);
    });
}

void LayerPanel::selectLayer(int layer) {
    selectedLayer_ = std::clamp(layer, 0, core::kNumSoundLayers - 1);
    std::ranges::for_each(std::views::iota(0, core::kNumSoundLayers), [&](int index) {
        const auto selected = index == selectedLayer_;
        tabs_[static_cast<size_t>(index)].setToggleState(selected, juce::dontSendNotification);
        pages_[static_cast<size_t>(index)]->setVisible(selected);
    });
    resized();
    repaint();
}

void LayerPanel::resized() {
    auto area = getLocalBounds();
    auto tabs = area.removeFromTop(44);
    std::ranges::for_each(tabs_, [&](juce::TextButton& tab) {
        tab.setBounds(tabs.removeFromLeft(118).withHeight(40));
        tabs.removeFromLeft(8);
    });
    area.removeFromTop(8);
    pages_[static_cast<size_t>(selectedLayer_)]->setBounds(area);
}

PianoRollView::PianoRollView(OpenEdmKickProcessor& processor) : processor_(processor) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip("Click to add or remove a hit. Shift-click a hit to change its ratchets. "
               "Lanes of layers that are off are dim. Click a dim layer name to turn that layer on.");
    startTimerHz(12);
}

void PianoRollView::timerCallback() {
    const float phase = processor_.displayAutoPhase.load(std::memory_order_relaxed);
    const auto settings = processor_.currentAutoSlotSettings();
    const auto notes = processor_.currentPianoRollNotes();
    auto& routing = processor_.routing();
    std::array<std::pair<bool, int>, core::kNumSoundLayers> layers{};
    for (int i = 0; i < core::kNumSoundLayers; ++i) {
        layers[static_cast<size_t>(i)] = {
            routing.displayValue(params::layerId(i, params::kLayerEnabled)) >= 0.5f,
            juce::roundToInt(routing.displayValue(params::layerId(i, params::kLayerSound)))
        };
    }
    if (std::abs(phase - lastPhase_) <= 0.0001f && settings == lastSettings_ && notes == lastNotes_ && layers == lastLayers_)
        return;
    lastPhase_ = phase;
    lastSettings_ = settings;
    lastNotes_ = notes;
    lastLayers_ = layers;
    repaint();
}

juce::Rectangle<float> PianoRollView::gridArea() const {
    return getLocalBounds().toFloat().reduced(13.0f, 10.0f).withTrimmedTop(40.0f).withTrimmedLeft(66.0f).withTrimmedBottom(4.0f);
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
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Click to add or remove a hit. Shift-click a hit to change its ratchets.", getLocalBounds().reduced(14, 5).removeFromTop(17),
               juce::Justification::centredLeft);

    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(grid, 5.0f);
    for (int step = 0; step <= totalSteps; step += gridStep) {
        const float x = grid.getX() + grid.getWidth() * static_cast<float>(step) / static_cast<float>(totalSteps);
        const bool barLine = step % core::kStepsPerBar64 == 0;
        const bool beatLine = step % (core::kStepsPerBar64 / 4) == 0;
        g.setColour(barLine ? theme::kTextDim : theme::kEdge.withAlpha(beatLine ? 1.0f : 0.48f));
        g.drawVerticalLine(juce::roundToInt(x), grid.getY() + 3.0f, grid.getBottom() - 3.0f);
        if (barLine && step < totalSteps && (step % (core::kStepsPerBar64 * std::max(1, bars / 16)) == 0)) {
            g.setColour(theme::kTextDim.withAlpha(0.8f));
            g.setFont(juce::FontOptions(11.0f));
            const int bar = step / core::kStepsPerBar64 + 1;
            g.drawText("Bar " + juce::String(bar), static_cast<int>(x) + 4, static_cast<int>(grid.getY()) - 16, 46, 13,
                       juce::Justification::centredLeft);
        }
    }

    const float rowHeight = grid.getHeight() / 5.0f;
    auto& routing = processor_.routing();
    const auto playing = [&routing](int layer) {
        return layer < 0 || routing.displayValue(params::layerId(layer, params::kLayerEnabled)) >= 0.5f;
    };
    for (int lane = 0; lane < 5; ++lane) {
        const float y = grid.getY() + static_cast<float>(lane) * rowHeight;
        g.setColour(theme::kEdge);
        g.drawHorizontalLine(juce::roundToInt(y), grid.getX(), grid.getRight());
        auto label = juce::String("All");
        if (lane > 0) {
            const auto soundId = params::layerId(lane - 1, params::kLayerSound);
            const auto& sounds = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters.getParameter(soundId))->choices;
            label = juce::String::charToString(static_cast<juce::juce_wchar>('A' + lane - 1)) + " "
                  + sounds[juce::roundToInt(routing.displayValue(soundId))];
        }
        if (!playing(lane - 1)) {
            g.setColour(theme::kInk.withAlpha(0.5f));
            g.fillRect(juce::Rectangle<float>(grid.getX(), y, grid.getWidth(), rowHeight));
        }
        g.setColour(theme::kTextDim.withMultipliedAlpha(playing(lane - 1) ? 1.0f : 0.4f));
        g.drawText(label, juce::Rectangle<float>(grid.getX() - 64.0f, y, 62.0f, rowHeight), juce::Justification::centredLeft);
    }
    const auto notes = processor_.currentPianoRollNotes();
    for (const auto& note : notes) {
        if (note.step64 < 0 || note.step64 >= totalSteps) continue;
        const float x = grid.getX() + grid.getWidth() * static_cast<float>(note.step64) / static_cast<float>(totalSteps);
        const float w = std::max(4.0f, grid.getWidth() * static_cast<float>(std::max(1, note.length64)) / static_cast<float>(totalSteps));
        const float laneHeight = rowHeight * 0.72f;
        const auto hit = juce::Rectangle<float>(x + 1.0f, grid.getY() + static_cast<float>(std::clamp(note.layer + 1, 0, 4)) * rowHeight + (rowHeight - laneHeight) * 0.5f,
                                                 std::min(w - 1.0f, grid.getRight() - x), laneHeight);
        const auto accent = note.reverseCue ? theme::kCoral : theme::kViolet;
        g.setColour(accent.withAlpha((0.32f + note.velocity * 0.58f) * (playing(note.layer) ? 1.0f : 0.4f)));
        g.fillRoundedRectangle(hit, 4.0f);
        if (note.ratchets > 1) {
            g.setColour(theme::kText);
            g.drawText(juce::String(note.ratchets), hit, juce::Justification::centred);
        }
        g.setColour(theme::kText.withAlpha(0.52f));
        g.fillRoundedRectangle(hit.withHeight(2.0f).withY(hit.getY() + 3.0f), 1.0f);
    }

    const float phase = processor_.displayAutoPhase.load(std::memory_order_relaxed);
    const float head = grid.getX() + grid.getWidth() * std::clamp(phase, 0.0f, 1.0f);
    g.setColour(theme::kMint);
    g.drawVerticalLine(juce::roundToInt(head), grid.getY(), grid.getBottom());
}

void PianoRollView::mouseDown(const juce::MouseEvent& event) {
    const auto grid = gridArea();
    const int lane = std::clamp(static_cast<int>((event.position.y - grid.getY()) * 5 / grid.getHeight()), 0, 4);
    const bool onLabel = event.position.x < grid.getX() && event.position.y >= grid.getY() && event.position.y < grid.getBottom();
    if (onLabel && lane > 0) {
        auto* enabled = processor_.parameters.getParameter(params::layerId(lane - 1, params::kLayerEnabled));
        enabled->beginChangeGesture();
        enabled->setValueNotifyingHost(1.0f);
        enabled->endChangeGesture();
        return;
    }
    if (!grid.contains(event.position)) return;
    const auto settings = processor_.currentAutoSlotSettings();
    const int totalSteps = std::max(1, settings.bars) * core::kStepsPerBar64;
    const int gridStep = core::kGridStepSizes64[static_cast<size_t>(std::clamp(settings.gridResolution, 0, 4))];
    const float normalized = std::clamp((event.position.x - grid.getX()) / grid.getWidth(), 0.0f, 0.99999f);
    const int step = (static_cast<int>(normalized * static_cast<float>(totalSteps)) / gridStep) * gridStep;
    const int layer = lane - 1;
    if (event.mods.isShiftDown()) {
        processor_.cyclePianoRollRatchets(step, layer);
        return;
    }
    processor_.togglePianoRollNote(step, gridStep, layer);
}

AutoPianoRollPanel::AutoPianoRollPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      scene_(processor.parameters, params::kAutoSlot, "Scene", theme::kViolet),
      bypass_(processor.parameters, params::kAutoBypass, "Bypass pattern", theme::kCoral),
      enabled_(processor.parameters, params::kAutoEnabled, "Run pattern", theme::kViolet),
      clockSync_(processor.parameters, params::kClockSync, "Host sync", theme::kMint),
      timeSig_(processor.parameters, params::kTimeSignature, "Time signature", theme::kMint),
      bars_(processor.parameters, params::autoSlotId(0, params::kAutoBars), "Length", theme::kViolet),
      grid_(processor.parameters, params::autoSlotId(0, params::kAutoGrid), "Grid", theme::kViolet),
      fillPreset_(processor.parameters, params::autoSlotId(0, params::kAutoFillPreset), "Fill", theme::kViolet),
      tempo_(processor.parameters, params::kTempoBpm, "Tempo", theme::kMint),
      tailShorten_(processor.parameters, params::autoSlotId(0, params::kAutoTailShorten), "Tail trim", theme::kMint),
      crushSweep_(processor.parameters, params::autoSlotId(0, params::kAutoCrushSweep), "Crush rise", theme::kCoral),
      pianoRoll_(processor),
      slotWatcher_(*processor.parameters.getParameter(params::kAutoSlot),
                   [this](float value) { syncSelectedSlot(static_cast<int>(value)); }) {
    for (juce::Component* component : std::array<juce::Component*, 11>{
             &bypass_, &enabled_, &clockSync_, &timeSig_, &bars_, &grid_, &fillPreset_,
             &tempo_, &tailShorten_, &crushSweep_, &pianoRoll_}) addAndMakeVisible(component);
    addAndMakeVisible(scene_);
    clockLock_ = std::make_unique<SectionLock>(processor.parameters, "clock", juce::StringArray{params::kAutoEnabled, params::kAutoBypass, params::kClockSync, params::kTimeSignature, params::kTempoBpm}, "Playback clock");
    sceneLock_ = std::make_unique<SectionLock>(processor.parameters, "scene", juce::StringArray{}, "Pattern scene");
    addAndMakeVisible(*clockLock_);
    addAndMakeVisible(*sceneLock_);
    enabled_.setTooltip("Play this scene using the host transport or internal clock.");
    clockSync_.setTooltip("Follow the host transport, tempo and time signature.");
    fillPreset_.setTooltip("Select a shared-lane trance pattern.");
    slotWatcher_.sendInitialUpdate();
    startTimerHz(12);
    setAdvancedMode(false);
    refreshActivity();
}

void AutoPianoRollPanel::timerCallback() {
    syncSelectedSlot(processor_.currentAutoSlot());
    refreshActivity();
}

void AutoPianoRollPanel::refreshActivity() {
    auto& routing = processor_.routing();
    const bool hostClock = routing.displayValue(params::kClockSync) >= 0.5f
        && processor_.wrapperType != juce::AudioProcessor::wrapperType_Standalone;
    showActivity(tempo_, !hostClock, "Internal playback tempo in BPM.", "Host sync is on, so the host sets the tempo. Turn off Host sync to use this tempo.");
    showActivity(bypass_, routing.displayValue(params::kAutoEnabled) >= 0.5f,
                 "Mute the sequencer while keeping incoming MIDI playable.", "Turn on Run pattern to use Bypass pattern.");
}

void AutoPianoRollPanel::bindSlotControls() {
    if (sceneLock_ != nullptr) {
        juce::StringArray ids{params::kAutoSlot};
        for (const auto* key : params::kAutoSlotIds) ids.add(params::autoSlotId(selectedSlot_, key));
        sceneLock_->bind(ids);
    }
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
    auto controls = getLocalBounds().removeFromTop(advancedMode_ ? 360 : 290);
    drawCard(g, controls.removeFromTop(controls.getHeight() / 2 - 3), "Playback clock", theme::kMint);
    drawCard(g, controls.withTrimmedTop(3), "Pattern scene", theme::kViolet);
}

void AutoPianoRollPanel::resized() {
    auto area = getLocalBounds();
    auto controls = area.removeFromTop(advancedMode_ ? 360 : 290);
    auto clock = controls.removeFromTop(controls.getHeight() / 2 - 3);
    auto scene = controls.withTrimmedTop(3);
    clockLock_->setBounds(clock.getRight() - kSectionLockWidth - 10, clock.getY() + 6, kSectionLockWidth, kSectionLockHeight);
    sceneLock_->setBounds(scene.getRight() - kSectionLockWidth - 10, scene.getY() + 6, kSectionLockWidth, kSectionLockHeight);
    layoutRow(clock.withTrimmedTop(46).reduced(12, 2), {&enabled_, &bypass_, &clockSync_, &timeSig_, &tempo_}, 8);
    layoutRow(scene.withTrimmedTop(46).reduced(12, 2), {&scene_, &bars_, &grid_, &fillPreset_, &tailShorten_, &crushSweep_}, 8);
    pianoRoll_.setBounds(area.withTrimmedTop(kCardGap));
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
    auto area = getLocalBounds().reduced(4, 6);
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Output peak", area.removeFromTop(18), juce::Justification::centred);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    auto readout = juce::String("-inf dB");
    if (heldDb_ > -99.0f) readout = juce::String(heldDb_, 1) + " dB";
    g.drawText(readout, area.removeFromTop(22), juce::Justification::centred);
    area.removeFromTop(4);

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
    if (processor_.routing().displayValue(params::kLimiter) >= 0.5f) {
        g.setColour(theme::kViolet);
        g.fillRect(juce::Rectangle<float>(xFor(processor_.routing().displayValue(params::kCeiling)) - 1.0f,
                                          track.getY() - 3.0f, 2.0f, track.getHeight() + 6.0f));
    }
    g.setColour(clipped_ ? theme::kCoral : theme::kInk);
    g.fillRoundedRectangle(clip, 3.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(clip.reduced(0.5f), 3.0f, 1.0f);

    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(10.0f));
    const auto ticks = area.removeFromTop(16).toFloat();
    std::ranges::for_each(std::array{ -24.0f, -12.0f, 0.0f }, [&](float db) {
        g.fillRect(xFor(db) - 0.5f, ticks.getY(), 1.0f, 3.0f);
        g.drawText(juce::String(juce::roundToInt(db)), juce::Rectangle<float>(xFor(db) - 12.0f, ticks.getY() + 3.0f, 24.0f, 12.0f),
                   juce::Justification::centred);
    });
}

EffectsPanel::EffectsPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      reverseEnabled_(processor.parameters, params::kReverseEnabled, "Reverse MIDI", theme::kCoral),
      crushEnabled_(processor.parameters, params::kCrushEnabled, "Crush MIDI", theme::kMint),
      stutterEnabled_(processor.parameters, params::kStutterEnabled, "Stutter MIDI", theme::kViolet),
      gateEnabled_(processor.parameters, params::kGateEnabled, "Gate MIDI", theme::kViolet),
      highPassMidi_(processor.parameters, params::kHighPassMidi, "High-pass sweep", theme::kCoral, params::kHighPassSyncEnabled),
      lowPassMidi_(processor.parameters, params::kLowPassMidi, "Low-pass sweep", theme::kViolet, params::kLowPassSyncEnabled),
      midiVolume_(processor.parameters, params::kMidiVolume, "MIDI volume", theme::kMint),
      midiExpression_(processor.parameters, params::kMidiExpression, "Expression", theme::kMint),
      midiBrightness_(processor.parameters, params::kMidiBrightness, "Brightness", theme::kMint),
      reverseMode_(processor.parameters, params::kReverseMode, "Reverse", theme::kCoral),
      reverseMix_(processor.parameters, params::kReverseMix, "Reverse mix", theme::kCoral),
      crushRate_(processor.parameters, params::kCrushRate, "Sample rate", theme::kMint),
      crushBits_(processor.parameters, params::kCrushBits, "Bit depth", theme::kMint),
      crushMix_(processor.parameters, params::kCrushMix, "Crush mix", theme::kMint),
      stutterRate_(processor.parameters, params::kStutterRate, "Stutter rate", theme::kViolet),
      stutterMix_(processor.parameters, params::kStutterMix, "Stutter mix", theme::kViolet),
      gateRate_(processor.parameters, params::kGateRate, "Gate rate", theme::kViolet),
      gateMix_(processor.parameters, params::kGateMix, "Gate depth", theme::kViolet),
      pumpRate_(processor.parameters, params::kPumpRate, "Pump rate", theme::kViolet),
      pumpMix_(processor.parameters, params::kPumpMix, "Pump depth", theme::kViolet),
      highPass_(processor.parameters, params::kHighPass, "High pass", theme::kMint),
      lowPass_(processor.parameters, params::kLowPass, "Low pass", theme::kMint),
      highPassSync_(processor.parameters, params::kHighPassSyncEnabled, "High-pass sync", theme::kCoral),
      highPassSyncRate_(processor.parameters, params::kHighPassSyncRate, "High-pass rate", theme::kCoral),
      lowPassSync_(processor.parameters, params::kLowPassSyncEnabled, "Low-pass sync", theme::kViolet),
      lowPassSyncRate_(processor.parameters, params::kLowPassSyncRate, "Low-pass rate", theme::kViolet) {
    std::ranges::for_each(std::array<juce::Component*, 15>{
        &reverseMode_, &reverseMix_, &crushRate_, &crushBits_, &crushMix_,
        &stutterRate_, &stutterMix_, &gateRate_, &gateMix_, &pumpRate_, &pumpMix_,
        &highPass_, &lowPass_, &highPassSyncRate_, &lowPassSyncRate_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    // The sweep buttons also show and clear the sync switches, so these stay hidden.
    addChildComponent(highPassSync_);
    addChildComponent(lowPassSync_);
    std::ranges::for_each(std::array<juce::Component*, 9>{
        &highPassMidi_, &lowPassMidi_, &reverseEnabled_, &crushEnabled_, &stutterEnabled_, &gateEnabled_, &midiVolume_, &midiExpression_, &midiBrightness_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    const std::array<juce::StringArray, 4> groups{{
        {params::kReverseEnabled, params::kReverseMode, params::kReverseMix},
        {params::kCrushEnabled, params::kCrushRate, params::kCrushBits, params::kCrushMix},
        {params::kStutterEnabled, params::kStutterRate, params::kStutterMix, params::kGateEnabled, params::kGateRate, params::kGateMix, params::kPumpRate, params::kPumpMix},
        {params::kHighPassMidi, params::kLowPassMidi, params::kHighPass, params::kLowPass, params::kHighPassSyncEnabled, params::kHighPassSyncRate, params::kLowPassSyncEnabled, params::kLowPassSyncRate, params::kMidiVolume, params::kMidiExpression, params::kMidiBrightness}
    }};
    const std::array<juce::String, 4> titles{"Reverse", "Bitcrusher", "Rhythm", "Filters & MIDI"};
    std::ranges::for_each(std::views::iota(size_t{0}, groups.size()), [&](size_t i) {
        sectionLocks_.push_back(std::make_unique<SectionLock>(processor.parameters, "fx_" + juce::String(i), groups[i], titles[i]));
        addAndMakeVisible(*sectionLocks_.back());
    });
    highPassMidi_.setTooltip("Sweep the high-pass filter in time with the host. CC21 also turns the sweep on and off.");
    lowPassMidi_.setTooltip("Sweep the low-pass filter in time with the host. CC22 also turns the sweep on and off.");
    setAdvancedMode(false);
    refreshActivity();
    startTimerHz(10);
}

void EffectsPanel::refreshActivity() {
    auto& routing = processor_.routing();
    const auto value = [&routing](const char* id) { return routing.displayValue(id); };
    const bool reverseMidi = value(params::kReverseEnabled) >= 0.5f;
    showActivity(reverseMode_, !reverseMidi,
                 "Reverse on defaults to CC20 and ends at the next quarter-note boundary. Right-click to reassign.",
                 "Reverse MIDI is on, so each hit reverses to the end of its beat. Turn off Reverse MIDI to pick a mode.");
    showActivity(reverseMix_, reverseMidi || value(params::kReverseMode) >= 0.5f,
                 "Wet/dry mix of the reverse effect.",
                 "Pick a Reverse mode or turn on Reverse MIDI to use this mix.");

    // A MIDI switch plays its effect at full mix while the mix is 0%. Above 0% the switch has no effect.
    const auto showSwitched = [&value](Knob& mix, Toggle& midi, const char* mixId, const char* midiId, const juce::String& mixName) {
        const bool switchedOn = value(midiId) >= 0.5f;
        const bool mixed = value(mixId) > 0.0f;
        mix.showEffectiveText(switchedOn && !mixed ? "100% (on)" : "");
        showActivity(midi, !mixed,
                     "Play this effect at full mix while " + mixName + " is 0%.",
                     mixName + " is above 0%, so the effect plays already. Set " + mixName
                         + " to 0% to let this switch play it at full mix.");
        return switchedOn || mixed;
    };
    const bool patternCrush = value(params::kAutoEnabled) >= 0.5f && value(params::kAutoBypass) < 0.5f
        && processor_.currentAutoSlotSettings().crushSweep > 0.0f;
    const bool crushing = showSwitched(crushMix_, crushEnabled_, params::kCrushMix, params::kCrushEnabled, "Crush mix") || patternCrush;
    showActivity(crushRate_, crushing, "Downsample rate reduction for the bitcrusher.", "Raise Crush mix or turn on Crush MIDI to hear the crusher.");
    showActivity(crushBits_, crushing, "Bit depth quantization for the bitcrusher.", "Raise Crush mix or turn on Crush MIDI to hear the crusher.");
    const bool stuttering = showSwitched(stutterMix_, stutterEnabled_, params::kStutterMix, params::kStutterEnabled, "Stutter mix");
    showActivity(stutterRate_, stuttering, "Host-synced repeat rate for the stutter effect.", "Raise Stutter mix or turn on Stutter MIDI to hear the stutter.");
    const bool gating = showSwitched(gateMix_, gateEnabled_, params::kGateMix, params::kGateEnabled, "Gate depth");
    showActivity(gateRate_, gating, "Host-synced rhythmic gate rate.", "Raise Gate depth or turn on Gate MIDI to hear the gate.");
    showActivity(pumpRate_, value(params::kPumpMix) > 0.0f, "Host-synced sidechain-style ducking rate.", "Raise Pump depth to hear the pump.");

    // A sweep runs from the bypass end to the cutoff, or to the sweep limit while the cutoff is Off.
    const bool highSweep = value(params::kHighPassMidi) >= 0.5f || value(params::kHighPassSyncEnabled) >= 0.5f;
    const float highCutoff = value(params::kHighPass);
    const float highTop = highCutoff > core::ToneFilter::kHighPassBypassHz + 0.5f ? highCutoff : core::ToneFilter::kHighPassSweepTopHz;
    const auto highSweepText = "Sweep " + params::formatHz(highTop);
    highPass_.showEffectiveText(highSweep ? highSweepText : juce::String());
    showActivity(highPassSyncRate_, highSweep, "Host-synced duration of the high-pass sweep.", "Turn on High-pass sweep to use this rate.");
    const bool lowSweep = value(params::kLowPassMidi) >= 0.5f || value(params::kLowPassSyncEnabled) >= 0.5f;
    const float lowCutoff = value(params::kLowPass);
    const float lowFloor = lowCutoff < core::ToneFilter::kLowPassBypassHz - 1.0f ? lowCutoff : core::ToneFilter::kLowPassSweepFloorHz;
    const auto lowSweepText = "Sweep " + params::formatHz(lowFloor);
    lowPass_.showEffectiveText(lowSweep ? lowSweepText : juce::String());
    showActivity(lowPassSyncRate_, lowSweep, "Host-synced duration of the low-pass sweep.", "Turn on Low-pass sweep to use this rate.");
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setControlsVisible({ &crushRate_, &crushBits_, &stutterRate_, &gateRate_, &pumpRate_, &pumpMix_,
                         &highPass_, &lowPass_, &highPassSyncRate_, &lowPassSyncRate_,
                         &midiVolume_, &midiExpression_, &midiBrightness_ }, advanced);
    resized();
    repaint();
}

std::array<juce::Rectangle<int>, 4> EffectsPanel::sections() const {
    auto area = getLocalBounds();
    const int row = (area.getHeight() - 3 * kCardGap) / (advancedMode_ ? 8 : 4);
    const auto reverse = area.removeFromTop(row);
    area.removeFromTop(kCardGap);
    const auto texture = area.removeFromTop(row);
    area.removeFromTop(kCardGap);
    const auto rhythm = area.removeFromTop(advancedMode_ ? 3 * row : row);
    return { reverse, texture, rhythm, area.withTrimmedTop(kCardGap) };
}

void EffectsPanel::paint(juce::Graphics& g) {
    const auto cards = sections();
    const std::array<juce::String, 4> titles{"Reverse", "Bitcrusher", "Rhythm", "Filters & MIDI"};
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
    layoutRow(cards[0], { &reverseEnabled_, &reverseMode_, &reverseMix_ });
    layoutRow(cards[1], { &crushEnabled_, &crushMix_, &crushRate_, &crushBits_ });
    if (!advancedMode_) {
        layoutRow(cards[2], { &stutterEnabled_, &stutterMix_, &gateEnabled_, &gateMix_ });
        layoutRow(cards[3], { &highPassMidi_, &lowPassMidi_ });
        return;
    }
    const int rhythmRow = cards[2].getHeight() / 3;
    layoutRow(cards[2].removeFromTop(rhythmRow), { &stutterEnabled_, &stutterRate_, &stutterMix_ });
    layoutRow(cards[2].removeFromTop(rhythmRow), { &gateEnabled_, &gateRate_, &gateMix_ });
    layoutRow(cards[2], { &pumpRate_, &pumpMix_ });
    const int third = cards[3].getHeight() / 3;
    layoutRow(cards[3].removeFromTop(third), { &highPassMidi_, &highPass_, &highPassSyncRate_ });
    layoutRow(cards[3].removeFromTop(third), { &lowPassMidi_, &lowPass_, &lowPassSyncRate_ });
    layoutRow(cards[3], { &midiVolume_, &midiExpression_, &midiBrightness_ });
}

} // namespace openedmkick::ui
