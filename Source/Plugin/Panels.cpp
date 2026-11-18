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
constexpr int kMidiModeWidth = 164;
constexpr int kCardHeader = 48;
constexpr int kSectionLockWidth = 112;
constexpr int kSectionLockHeight = 40;

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
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 11.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 11.0f, 1.0f);
    auto area = getLocalBounds().reduced(18, 12);
    auto readout = area.removeFromLeft(136);
    const int note = processor_.currentMidiNote();
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("MIDI note", readout.removeFromTop(21), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(40.0f, juce::Font::bold));
    g.drawText(noteLabel(note), readout.removeFromTop(46), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(17.0f));
    g.drawText(params::formatHz(core::noteFrequencyHz(note)), readout, juce::Justification::topLeft);

    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("One-shot waveform", area.removeFromTop(20), juce::Justification::centredLeft);
    const auto scale = area.removeFromBottom(19);
    const int durationMs = juce::roundToInt(1000.0 * static_cast<double>(samples_.size()) / processor_.previewSampleRate);
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
      bodyDrive_(processor.parameters, params::kBodyDrive, "Body drive", theme::kMint),
      saturation_(processor.parameters, params::kSaturation, "Saturation", theme::kMint),
      knockHz_(processor.parameters, params::kKnockHz, "Knock freq", theme::kMint),
      knockDb_(processor.parameters, params::kKnockDb, "Knock", theme::kMint),
      grit_(processor.parameters, params::kGrit, "Tail grit", theme::kMint),
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
      clip_(processor.parameters, params::kClip, "Clip", theme::kMint),
      ampEnabled_(processor.parameters, params::kAmpEnabled, "Hold MIDI notes", theme::kViolet),
      ampAttack_(processor.parameters, params::kAmpAttack, "Attack", theme::kMint),
      ampDecay_(processor.parameters, params::kAmpDecay, "Decay", theme::kMint),
      ampSustain_(processor.parameters, params::kAmpSustain, "Sustain", theme::kMint),
      ampRelease_(processor.parameters, params::kAmpRelease, "Release", theme::kMint),
      waveform_(processor) {
    for (juce::Component* component : std::array<juce::Component*, 27>{
             &sweepOctaves_, &sweepMs_, &sweepShape_, &snapOctaves_, &snapMs_,
             &bodyTone_, &bodyDrive_, &saturation_, &knockHz_, &knockDb_, &grit_, &toneHz_, &subLevel_, &subOctave_, &startPhase_,
             &clickType_, &clickLevel_, &clickHz_, &clickMs_, &clickPunch_, &attack_, &hold_, &decay_, &decayShape_, &lowCut_, &clip_, &waveform_})
        addAndMakeVisible(component);

    for (auto* component : std::array<juce::Component*, 5>{&ampEnabled_, &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_}) addAndMakeVisible(component);
    const std::array<juce::StringArray, 5> groups{{
        {params::kSweepOctaves, params::kSweepMs, params::kSweepShape, params::kSnapOctaves, params::kSnapMs},
        {params::kBodyTone, params::kBodyDrive, params::kSaturation, params::kKnockHz, params::kKnockDb, params::kGrit, params::kToneHz, params::kSubLevel, params::kSubOctave, params::kStartPhase},
        {params::kClickType, params::kClickLevel, params::kClickHz, params::kClickMs, params::kClickPunch},
        {params::kAttackMs, params::kHoldMs, params::kDecayMs, params::kDecayShape, params::kLowCutHz, params::kClip},
        {params::kAmpEnabled, params::kAmpAttack, params::kAmpDecay, params::kAmpSustain, params::kAmpRelease}
    }};
    const std::array<juce::String, 5> names{"dive", "body", "transient", "tail", "adsr"};
    for (size_t i = 0; i < groups.size(); ++i) {
        sectionLocks_.push_back(std::make_unique<SectionLock>(processor.parameters, names[i], groups[i]));
        addAndMakeVisible(*sectionLocks_.back());
    }
    sweepOctaves_.setTooltip("How far above the played MIDI note the opening of the kick starts.");
    sweepMs_.setTooltip("Time for the pitch dive to reach the incoming MIDI note.");
    bodyTone_.setTooltip("Add harmonics to give the kick a more forceful, audible body.");
    knockDb_.setTooltip("A resonant mid-bass knock for more impact on smaller speakers.");
    subLevel_.setTooltip("Blend a clean fundamental under the driven body.");
    clickLevel_.setTooltip("Add a short beater transient ahead of the body.");
    ampEnabled_.setTooltip("Hold incoming MIDI notes until note-off and shape them with attack, decay, sustain and release.");
    ampEnabled_.button().getToggleStateValue().addListener(this);
    setAdvancedMode(false);
}

KickSynthPanel::~KickSynthPanel() {
    ampEnabled_.button().getToggleStateValue().removeListener(this);
}

void KickSynthPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setControlsVisible({ &sweepShape_, &snapOctaves_, &snapMs_, &bodyDrive_, &saturation_, &knockHz_,
                         &grit_, &toneHz_, &subOctave_, &startPhase_, &clickMs_, &clickPunch_, &attack_, &decayShape_, &lowCut_, &clip_ }, advanced);
    heldNotes_ = ampEnabled_.button().getToggleState();
    setControlsVisible({ &hold_, &decay_, sectionLocks_[3].get() }, advanced || !heldNotes_);
    setControlsVisible({ &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_, sectionLocks_[4].get() }, advanced || heldNotes_);
    resized();
    repaint();
}

std::array<juce::Rectangle<int>, 6> KickSynthPanel::sections() const {
    auto area = getLocalBounds();
    const auto scope = area.removeFromTop(advancedMode_ ? 122 : 150);
    area.removeFromTop(kCardGap);
    juce::Rectangle<int> adsr;
    if (advancedMode_) {
        adsr = area.removeFromBottom(164);
        area.removeFromBottom(kCardGap);
    }
    auto top = area.removeFromTop((area.getHeight() - kCardGap) / 2);
    area.removeFromTop(kCardGap);
    const int half = (area.getWidth() - kCardGap) / 2;
    const auto dive = top.removeFromLeft(half);
    const auto transient = area.removeFromLeft(half);
    const auto tail = area.withTrimmedLeft(kCardGap);
    if (!advancedMode_ && heldNotes_) adsr = tail;
    return { dive, top.withTrimmedLeft(kCardGap), transient, tail, adsr, scope };
}

void KickSynthPanel::paint(juce::Graphics& g) {
    const auto cards = sections();
    drawCard(g, cards[0], "Pitch dive", theme::kViolet);
    drawCard(g, cards[1], "Body & sub", theme::kMint);
    drawCard(g, cards[2], "Transient", theme::kCoral);
    if (advancedMode_ || !heldNotes_) drawCard(g, cards[3], "One-shot tail", theme::kMint);
    if (advancedMode_ || heldNotes_) drawCard(g, cards[4], "MIDI envelope", theme::kViolet);
    auto mode = cards[5].withLeft(cards[5].getRight() - kMidiModeWidth);
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(mode.toFloat(), 9);
    drawSmallLabel(g, mode.reduced(14, 8).removeFromTop(20), "MIDI playback", theme::kTextDim);
}

void KickSynthPanel::resized() {
    const auto cards = sections();
    const int rows = advancedMode_ ? 2 : 1;
    for (size_t i = 0; i < sectionLocks_.size(); ++i)
        sectionLocks_[i]->setBounds(cards[i].getRight() - kSectionLockWidth - 10, cards[i].getY() + 6,
                                    kSectionLockWidth, kSectionLockHeight);
    auto scope = cards[5];
    auto mode = scope.removeFromRight(kMidiModeWidth);
    ampEnabled_.setBounds(mode.withTrimmedTop(32).reduced(10, 3));
    waveform_.setBounds(scope.withTrimmedRight(kCardGap));
    placeRow(cards[0], 0, rows, { &sweepOctaves_, &sweepMs_, &sweepShape_ });
    placeRow(cards[1], 0, rows, { &subLevel_, &knockDb_, &bodyTone_, &bodyDrive_ });
    placeRow(cards[2], 0, 1, { &clickType_, &clickLevel_, &clickHz_, &clickMs_, &clickPunch_ });
    placeRow(cards[3], 0, rows, { &hold_, &decay_, &decayShape_ });
    if (advancedMode_ || heldNotes_)
        placeRow(cards[4], 0, 1, { &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_ });
    if (!advancedMode_) return;
    placeRow(cards[0], 1, 2, { &snapOctaves_, &snapMs_ });
    placeRow(cards[1], 1, 2, { &saturation_, &knockHz_, &grit_, &toneHz_, &subOctave_, &startPhase_ });
    placeRow(cards[3], 1, 2, { &attack_, &lowCut_, &clip_ });
}

LayerPage::LayerPage(OpenEdmKickProcessor& processor, int layer)
    : layer_(std::clamp(layer, 0, core::kNumSoundLayers - 1)),
      enabled_(processor.parameters, params::layerId(layer_, params::kLayerEnabled), "Enable layer", theme::kViolet),
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
    layerKnobs_[6] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerDriveOffset), "Drive offset", theme::kMint);
    addAndMakeVisible(*layerKnobs_[6]);
    layerKnobs_[7] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerToneScale), "Tone scale", theme::kMint);
    addAndMakeVisible(*layerKnobs_[7]);
    layerKnobs_[8] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerSweepScale), "Sweep scale", theme::kMint);
    addAndMakeVisible(*layerKnobs_[8]);
    layerKnobs_[9] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerPan), "Pan", theme::kMint);
    addAndMakeVisible(*layerKnobs_[9]);
    layerToggles_[0] = std::make_unique<Toggle>(processor.parameters, params::layerId(layer_, params::kLayerSolo), "Solo", theme::kViolet);
    addAndMakeVisible(*layerToggles_[0]);
    layerToggles_[1] = std::make_unique<Toggle>(processor.parameters, params::layerId(layer_, params::kLayerPolarity), "Invert polarity", theme::kViolet);
    addAndMakeVisible(*layerToggles_[1]);
    juce::StringArray ids;
    for (const auto* key : std::array<const char*, 19>{params::kLayerEnabled, params::kLayerSound, params::kLayerGain,
        params::kLayerTranspose, params::kLayerDecay, params::kLayerHighPass, params::kLayerLowPass,
        params::kLayerVelocityCurve, params::kLayerVelocityTimbre, params::kLayerKeyLow, params::kLayerKeyHigh,
        params::kLayerVelocityLow, params::kLayerVelocityHigh, params::kLayerDriveOffset, params::kLayerToneScale,
        params::kLayerSweepScale, params::kLayerSolo, params::kLayerPan, params::kLayerPolarity}) ids.add(params::layerId(layer_, key));
    sectionLock_ = std::make_unique<SectionLock>(processor.parameters, "layer_" + juce::String(layer_), ids);
    addAndMakeVisible(*sectionLock_);
    enabled_.setTooltip("Include this sound layer in every kick hit.");
    sound_.setTooltip("Choose a synthesized kick, snare, clap, or hi-hat voice.");
    layerToggles_[0]->setTooltip("Hear only enabled layers with Solo switched on.");
    layerToggles_[1]->setTooltip("Reverse this layer's polarity before mixing it with the other layers.");
    level_.setTooltip("Set this layer's mix level.");
    pitch_.setTooltip("Transpose the layer in semitones. Hats use this to tune their metal partials.");
    decay_.setTooltip("Set this layer's one-shot length. Kick layers scale their main kick decay by this value.");
    highPass_.setTooltip("Filter this layer before it is mixed with the other layers.");
    lowPass_.setTooltip("Filter this layer before it is mixed with the other layers.");
}

void LayerPage::paint(juce::Graphics& g) {
    auto area = getLocalBounds();
    const int height = area.getHeight() / 4;
    for (const auto* title : { "Layer mixer", "Pitch & filters", "Response", "MIDI zone" }) {
        auto strip = area.removeFromTop(height).withTrimmedBottom(5);
        g.setColour(theme::kSteel);
        g.fillRoundedRectangle(strip.toFloat(), 8);
        g.setColour(theme::kText);
        g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
        g.drawText(title, strip.reduced(14).removeFromLeft(118).removeFromTop(28), juce::Justification::centredLeft);
    }
}

void LayerPage::resized() {
    sectionLock_->setBounds(12, 50, kSectionLockWidth, kSectionLockHeight);
    auto area = getLocalBounds().withTrimmedLeft(140).withTrimmedRight(10);
    const int rowHeight = area.getHeight() / 4;
    layoutRow(area.removeFromTop(rowHeight).reduced(0, 4), { &enabled_, &sound_, &level_, layerToggles_[0].get(), layerToggles_[1].get() }, 8);
    layoutRow(area.removeFromTop(rowHeight).reduced(0, 4), { &pitch_, &decay_, &highPass_, &lowPass_, layerKnobs_[9].get() }, 8);
    layoutRow(area.removeFromTop(rowHeight).reduced(0, 4), { layerKnobs_[0].get(), layerKnobs_[1].get(), layerKnobs_[6].get(), layerKnobs_[7].get(), layerKnobs_[8].get() }, 8);
    layoutRow(area.reduced(0, 4), { layerKnobs_[2].get(), layerKnobs_[3].get(), layerKnobs_[4].get(), layerKnobs_[5].get() }, 8);
}

LayerPanel::LayerPanel(OpenEdmKickProcessor& processor) {
    std::ranges::for_each(std::views::iota(0, core::kNumSoundLayers), [&](int layer) {
        const auto index = static_cast<size_t>(layer);
        pages_[index] = std::make_unique<LayerPage>(processor, layer);
        addAndMakeVisible(*pages_[index]);
        auto& tab = tabs_[index];
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
        tab.setButtonText("Layer " + letter);
        tab.setComponentID("layer_tab_" + letter.toLowerCase());
        tab.setClickingTogglesState(false);
        tab.setColour(juce::TextButton::buttonOnColourId, theme::kViolet);
        tab.onClick = [this, layer] { selectLayer(layer); };
        addAndMakeVisible(tab);
    });
    selectLayer(0);
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
    startTimerHz(12);
}

juce::Rectangle<float> PianoRollView::gridArea() const {
    return getLocalBounds().toFloat().reduced(13.0f, 10.0f).withTrimmedTop(40.0f).withTrimmedLeft(25.0f).withTrimmedBottom(4.0f);
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
    for (int lane = 0; lane < 5; ++lane) {
        const float y = grid.getY() + static_cast<float>(lane) * rowHeight;
        g.setColour(theme::kEdge);
        g.drawHorizontalLine(juce::roundToInt(y), grid.getX(), grid.getRight());
        g.setColour(theme::kTextDim);
        const auto label = lane == 0 ? juce::String("ALL") : juce::String::charToString(static_cast<juce::juce_wchar>('A' + lane - 1));
        g.drawText(label, juce::Rectangle<float>(grid.getX() - 24, y + 4, 22, 12), juce::Justification::centredLeft);
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
        g.setColour(accent.withAlpha(0.32f + note.velocity * 0.58f));
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
    if (!grid.contains(event.position)) return;
    const auto settings = processor_.currentAutoSlotSettings();
    const int totalSteps = std::max(1, settings.bars) * core::kStepsPerBar64;
    const int gridStep = core::kGridStepSizes64[static_cast<size_t>(std::clamp(settings.gridResolution, 0, 4))];
    const float normalized = std::clamp((event.position.x - grid.getX()) / grid.getWidth(), 0.0f, 0.99999f);
    const int step = (static_cast<int>(normalized * static_cast<float>(totalSteps)) / gridStep) * gridStep;
    const int layer = std::clamp(static_cast<int>((event.position.y - grid.getY()) * 5 / grid.getHeight()), 0, 4) - 1;
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
      timeSig_(processor.parameters, params::kTimeSignature, "Meter", theme::kMint),
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
    clockLock_ = std::make_unique<SectionLock>(processor.parameters, "clock", juce::StringArray{params::kAutoEnabled, params::kAutoBypass, params::kClockSync, params::kTimeSignature, params::kTempoBpm});
    sceneLock_ = std::make_unique<SectionLock>(processor.parameters, "scene", juce::StringArray{});
    addAndMakeVisible(*clockLock_);
    addAndMakeVisible(*sceneLock_);
    enabled_.setTooltip("Play this scene using the host transport or internal clock.");
    bypass_.setTooltip("Mute the sequencer while keeping incoming MIDI playable.");
    clockSync_.setTooltip("Follow the host transport, tempo and time signature.");
    fillPreset_.setTooltip("Select a shared-lane trance pattern.");
    slotWatcher_.sendInitialUpdate();
    startTimerHz(12);
    setAdvancedMode(false);
}

void AutoPianoRollPanel::timerCallback() { syncSelectedSlot(processor_.currentAutoSlot()); }

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

PeakMeter::PeakMeter(OpenEdmKickProcessor& processor) : processor_(processor) { startTimerHz(15); }
void PeakMeter::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kInk);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
    const float peak = processor_.displayPeak.load(std::memory_order_relaxed);
    auto fill = bounds.reduced(1.5f).withWidth(std::max(0.0f, (bounds.getWidth() - 3.0f) * std::clamp(peak, 0.0f, 1.0f)));
    g.setColour(peak > 0.9f ? theme::kCoral : theme::kMint);
    g.fillRoundedRectangle(fill, 4.0f);
}

EffectsPanel::EffectsPanel(OpenEdmKickProcessor& processor)
    : reverseEnabled_(processor.parameters, params::kReverseEnabled, "Reverse MIDI", theme::kCoral),
      crushEnabled_(processor.parameters, params::kCrushEnabled, "Crush MIDI", theme::kMint),
      stutterEnabled_(processor.parameters, params::kStutterEnabled, "Stutter MIDI", theme::kViolet),
      gateEnabled_(processor.parameters, params::kGateEnabled, "Chopper MIDI", theme::kViolet),
      highPassMidi_(processor.parameters, params::kHighPassMidi, "High-pass MIDI", theme::kCoral),
      lowPassMidi_(processor.parameters, params::kLowPassMidi, "Low-pass MIDI", theme::kViolet),
      midiVolume_(processor.parameters, params::kMidiVolume, "MIDI Volume", theme::kMint),
      midiExpression_(processor.parameters, params::kMidiExpression, "Expression", theme::kMint),
      midiBrightness_(processor.parameters, params::kMidiBrightness, "Brightness", theme::kMint),
      reverseMode_(processor.parameters, params::kReverseMode, "Reverse", theme::kCoral),
      reverseMix_(processor.parameters, params::kReverseMix, "Reverse mix", theme::kCoral),
      drive_(processor.parameters, params::kDrive, "Drive", theme::kMint),
      stereoWidth_(processor.parameters, params::kStereoWidth, "Top width", theme::kMint),
      crushRate_(processor.parameters, params::kCrushRate, "Sample rate", theme::kMint),
      crushBits_(processor.parameters, params::kCrushBits, "Bit depth", theme::kMint),
      crushMix_(processor.parameters, params::kCrushMix, "Crush mix", theme::kMint),
      stutterRate_(processor.parameters, params::kStutterRate, "Repeat rate", theme::kViolet),
      stutterMix_(processor.parameters, params::kStutterMix, "Repeat mix", theme::kViolet),
      gateRate_(processor.parameters, params::kGateRate, "Gate rate", theme::kViolet),
      gateMix_(processor.parameters, params::kGateMix, "Gate depth", theme::kViolet),
      pumpRate_(processor.parameters, params::kPumpRate, "Filter rate", theme::kViolet),
      pumpMix_(processor.parameters, params::kPumpMix, "Filter depth", theme::kViolet),
      highPass_(processor.parameters, params::kHighPass, "High pass", theme::kMint),
      lowPass_(processor.parameters, params::kLowPass, "Low pass", theme::kMint),
      highPassSync_(processor.parameters, params::kHighPassSyncEnabled, "High-pass sync", theme::kCoral),
      highPassSyncRate_(processor.parameters, params::kHighPassSyncRate, "HP rate", theme::kCoral),
      lowPassSync_(processor.parameters, params::kLowPassSyncEnabled, "Low-pass sync", theme::kViolet),
      lowPassSyncRate_(processor.parameters, params::kLowPassSyncRate, "LP rate", theme::kViolet),
      output_(processor.parameters, params::kOutput, "Output", theme::kMint),
      ceiling_(processor.parameters, params::kCeiling, "Ceiling", theme::kMint),
      limiter_(processor.parameters, params::kLimiter, "Limiter", theme::kMint),
      masterBypass_(processor.parameters, params::kMasterBypass, "Bypass plug-in", theme::kCoral),
      meter_(processor) {
    std::ranges::for_each(std::array<juce::Component*, 24>{
        &reverseMode_, &reverseMix_, &drive_, &stereoWidth_, &crushRate_, &crushBits_, &crushMix_,
        &stutterRate_, &stutterMix_, &gateRate_, &gateMix_, &pumpRate_, &pumpMix_,
        &highPass_, &lowPass_, &highPassSync_, &highPassSyncRate_, &lowPassSync_, &lowPassSyncRate_,
        &output_, &ceiling_, &limiter_, &masterBypass_, &meter_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    for (auto* component : std::array<juce::Component*, 9>{&highPassMidi_, &lowPassMidi_, &reverseEnabled_, &crushEnabled_, &stutterEnabled_, &gateEnabled_, &midiVolume_, &midiExpression_, &midiBrightness_}) addAndMakeVisible(component);
    const std::array<juce::StringArray, 4> groups{{
        {params::kReverseEnabled, params::kReverseMode, params::kReverseMix, params::kDrive, params::kMasterBypass},
        {params::kCrushEnabled, params::kCrushRate, params::kCrushBits, params::kCrushMix, params::kStereoWidth},
        {params::kStutterEnabled, params::kStutterRate, params::kStutterMix, params::kGateEnabled, params::kGateRate, params::kGateMix, params::kPumpRate, params::kPumpMix},
        {params::kHighPassMidi, params::kLowPassMidi, params::kHighPass, params::kLowPass, params::kHighPassSyncEnabled, params::kHighPassSyncRate, params::kLowPassSyncEnabled, params::kLowPassSyncRate, params::kOutput, params::kCeiling, params::kLimiter, params::kMidiVolume, params::kMidiExpression, params::kMidiBrightness}
    }};
    for (size_t i = 0; i < groups.size(); ++i) {
        sectionLocks_.push_back(std::make_unique<SectionLock>(processor.parameters, "fx_" + juce::String(i), groups[i]));
        addAndMakeVisible(*sectionLocks_.back());
    }
    reverseMode_.setTooltip("Reverse on defaults to CC20 and ends at the next quarter-note boundary. Right-click to reassign.");
    highPassMidi_.setTooltip("High-pass MIDI activation. CC21 enables the high-pass filter for incoming hits.");
    highPassSync_.setTooltip("High-pass sync starts an automated host-tempo sweep. The separate MIDI activation defaults to CC21.");
    lowPassMidi_.setTooltip("Low-pass MIDI activation. CC22 enables the low-pass filter for incoming hits.");
    lowPassSync_.setTooltip("Low-pass sync starts an automated host-tempo sweep. The separate MIDI activation defaults to CC22.");
    setAdvancedMode(false);
}

void EffectsPanel::setAdvancedMode(bool advanced) {
    advancedMode_ = advanced;
    setControlsVisible({ &crushRate_, &crushBits_, &stutterRate_, &gateRate_, &pumpRate_, &pumpMix_,
                         &highPass_, &lowPass_, &highPassSync_, &highPassSyncRate_, &lowPassSync_,
                         &lowPassSyncRate_, &ceiling_, &midiVolume_, &midiExpression_, &midiBrightness_ }, advanced);
    resized();
    repaint();
}

std::array<juce::Rectangle<int>, 4> EffectsPanel::sections() const {
    auto area = getLocalBounds();
    const int row = (area.getHeight() - 3 * kCardGap) / (advancedMode_ ? 7 : 4);
    const auto reverse = area.removeFromTop(row);
    area.removeFromTop(kCardGap);
    const auto texture = area.removeFromTop(row);
    area.removeFromTop(kCardGap);
    const auto rhythm = area.removeFromTop(advancedMode_ ? 2 * row : row);
    return { reverse, texture, rhythm, area.withTrimmedTop(kCardGap) };
}

void EffectsPanel::paint(juce::Graphics& g) {
    const auto cards = sections();
    const std::array<juce::String, 4> titles{"Reverse & drive", "Bitcrusher", "Rhythm", "Tone & output"};
    for (size_t i = 0; i < cards.size(); ++i) {
        g.setColour(theme::kSteel);
        g.fillRoundedRectangle(cards[i].toFloat(), 9);
        g.setColour(theme::kText);
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText(titles[i], cards[i].reduced(14).removeFromTop(26), juce::Justification::centredLeft);
    }
    g.setColour(theme::kText);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("Output peak", meter_.getBounds().withY(meter_.getY() - 24).withHeight(19), juce::Justification::centred);
}

void EffectsPanel::resized() {
    auto cards = sections();
    for (size_t i = 0; i < cards.size(); ++i) {
        sectionLocks_[i]->setBounds(14, cards[i].getY() + 48, kSectionLockWidth, kSectionLockHeight);
        cards[i] = cards[i].withTrimmedLeft(154).reduced(8, 8);
    }
    layoutRow(cards[0], { &reverseEnabled_, &reverseMode_, &reverseMix_, &drive_, &masterBypass_ });
    layoutRow(cards[1], { &crushEnabled_, &crushMix_, &crushRate_, &crushBits_, &stereoWidth_ });
    if (advancedMode_) {
        auto stutter = cards[2].removeFromTop(cards[2].getHeight() / 2);
        layoutRow(stutter, { &stutterEnabled_, &stutterRate_, &stutterMix_, &pumpRate_, &pumpMix_ });
        layoutRow(cards[2], { &gateEnabled_, &gateRate_, &gateMix_ });
        const int third = cards[3].getHeight() / 3;
        layoutRow(cards[3].removeFromTop(third), { &highPassMidi_, &highPass_, &highPassSync_, &highPassSyncRate_ });
        layoutRow(cards[3].removeFromTop(third), { &lowPassMidi_, &lowPass_, &lowPassSync_, &lowPassSyncRate_ });
    } else {
        layoutRow(cards[2], { &stutterEnabled_, &stutterMix_, &gateEnabled_, &gateMix_ });
    }
    if (advancedMode_)
        layoutRow(cards[3], { &output_, &ceiling_, &limiter_, &midiVolume_, &midiExpression_, &midiBrightness_, &meter_ });
    else
        layoutRow(cards[3], { &highPassMidi_, &lowPassMidi_, &output_, &limiter_, &meter_ });
    const auto meter = meter_.getBounds().withTrimmedBottom(40);
    meter_.setBounds(meter.withSizeKeepingCentre(meter.getWidth() - 8, 14));
}

} // namespace openedmkick::ui
