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
constexpr int kCardHeader = 48;
constexpr int kSectionLockWidth = 136;
constexpr int kSectionLockHeight = 36;
constexpr int kPlaybackCardHeight = 290;

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
    g.drawText("MIDI pitch with layer transposition", title, juce::Justification::centredRight);

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
      ampEnabled_(processor.parameters, params::kAmpEnabled, "Full ADSR", theme::kViolet),
      ampAttack_(processor.parameters, params::kAmpAttack, "ADSR attack", theme::kBrass),
      ampDecay_(processor.parameters, params::kAmpDecay, "ADSR decay", theme::kBrass),
      ampSustain_(processor.parameters, params::kAmpSustain, "Sustain", theme::kBrass),
      ampRelease_(processor.parameters, params::kAmpRelease, "Release / 20 s", theme::kBrass),
      waveform_(processor) {
    for (juce::Component* component : std::array<juce::Component*, 25>{
             &sweepOctaves_, &sweepMs_, &sweepShape_, &snapOctaves_, &snapMs_,
             &bodyTone_, &bodyDrive_, &saturation_, &knockHz_, &knockDb_, &grit_, &toneHz_, &subLevel_, &startPhase_,
             &clickType_, &clickLevel_, &clickHz_, &clickMs_, &attack_, &hold_, &decay_, &decayShape_, &lowCut_, &clip_, &waveform_})
        addAndMakeVisible(component);

    for (auto* component : std::array<juce::Component*, 5>{&ampEnabled_, &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_}) addAndMakeVisible(component);
    const std::array<juce::StringArray, 5> groups{{
        {params::kSweepOctaves, params::kSweepMs, params::kSweepShape, params::kSnapOctaves, params::kSnapMs},
        {params::kBodyTone, params::kBodyDrive, params::kSaturation, params::kKnockHz, params::kKnockDb, params::kGrit, params::kToneHz, params::kSubLevel, params::kStartPhase},
        {params::kClickType, params::kClickLevel, params::kClickHz, params::kClickMs},
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
    drawCard(g, area.removeFromTop(132), "Full ADSR / note/channel matched / one-shot when off", theme::kBrass);
    area.removeFromTop(8);
    const int waveHeight = 96;
    area.removeFromTop(2);
    drawCard(g, area.removeFromTop(waveHeight), "Played note", theme::kViolet);
    area.removeFromTop(kCardGap);
    const int cardHeight = (area.getHeight() - kCardGap) * 3 / 5;
    auto top = area.removeFromTop(cardHeight);
    area.removeFromTop(kCardGap);
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
    const auto adsr = area.removeFromTop(132);
    auto ampControls = adsr.withTrimmedTop(46).reduced(14, 5);
    layoutRow(ampControls, {&ampEnabled_, &ampAttack_, &ampDecay_, &ampSustain_, &ampRelease_}, 8);
    sectionLocks_[4]->setBounds(adsr.getRight() - kSectionLockWidth - 12, adsr.getY() + 5,
                                kSectionLockWidth, kSectionLockHeight);
    area.removeFromTop(8);
    const int waveHeight = 96;
    waveform_.setBounds(area.removeFromTop(waveHeight).reduced(1));
    area.removeFromTop(kCardGap);
    const int cardHeight = (area.getHeight() - kCardGap) * 3 / 5;
    auto top = area.removeFromTop(cardHeight);
    area.removeFromTop(kCardGap);
    auto bottom = area;
    const int cardWidth = (top.getWidth() - kCardGap) / 2;
    const auto dive = top.removeFromLeft(cardWidth); top.removeFromLeft(kCardGap);
    const auto body = top;
    const auto transient = bottom.removeFromLeft(cardWidth); bottom.removeFromLeft(kCardGap);
    const auto tail = bottom;

    const std::array<juce::Rectangle<int>, 4> cards{dive, body, transient, tail};
    for (size_t i = 0; i < cards.size(); ++i)
        sectionLocks_[i]->setBounds(cards[i].getRight() - kSectionLockWidth - 12, cards[i].getY() + 5,
                                    kSectionLockWidth, kSectionLockHeight);
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

LayerPage::LayerPage(OpenEdmKickProcessor& processor, int layer)
    : layer_(std::clamp(layer, 0, core::kNumSoundLayers - 1)),
      enabled_(processor.parameters, params::layerId(layer_, params::kLayerEnabled), "Layer on", theme::kViolet),
      sound_(processor.parameters, params::layerId(layer_, params::kLayerSound), "Sound", theme::kCoral),
      level_(processor.parameters, params::layerId(layer_, params::kLayerGain), "Level", theme::kBrass),
      pitch_(processor.parameters, params::layerId(layer_, params::kLayerTranspose), "Pitch", theme::kViolet),
      decay_(processor.parameters, params::layerId(layer_, params::kLayerDecay), "Decay", theme::kBrass),
      highPass_(processor.parameters, params::layerId(layer_, params::kLayerHighPass), "High pass", theme::kCoral),
      lowPass_(processor.parameters, params::layerId(layer_, params::kLayerLowPass), "Low pass", theme::kCoral) {
    std::ranges::for_each(std::array<juce::Component*, 7>{
        &enabled_, &sound_, &level_, &pitch_, &decay_, &highPass_, &lowPass_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    layerKnobs_[0] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityCurve), "Velocity curve", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[0]);
    layerKnobs_[1] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityTimbre), "Velocity timbre", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[1]);
    layerKnobs_[2] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerKeyLow), "Key low", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[2]);
    layerKnobs_[3] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerKeyHigh), "Key high", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[3]);
    layerKnobs_[4] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityLow), "Velocity low", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[4]);
    layerKnobs_[5] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerVelocityHigh), "Velocity high", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[5]);
    layerKnobs_[6] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerDriveOffset), "Drive offset", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[6]);
    layerKnobs_[7] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerToneScale), "Tone scale", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[7]);
    layerKnobs_[8] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerSweepScale), "Sweep scale", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[8]);
    layerKnobs_[9] = std::make_unique<Knob>(processor.parameters, params::layerId(layer_, params::kLayerPan), "Pan", theme::kBrass);
    addAndMakeVisible(*layerKnobs_[9]);
    layerToggles_[0] = std::make_unique<Toggle>(processor.parameters, params::layerId(layer_, params::kLayerSolo), "Solo", theme::kViolet);
    addAndMakeVisible(*layerToggles_[0]);
    layerToggles_[1] = std::make_unique<Toggle>(processor.parameters, params::layerId(layer_, params::kLayerPolarity), "Invert", theme::kViolet);
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
    level_.setTooltip("Set this layer's mix level.");
    pitch_.setTooltip("Transpose the layer in semitones. Hats use this to tune their metal partials.");
    decay_.setTooltip("Set this layer's one-shot length. Kick layers scale their main kick decay by this value.");
    highPass_.setTooltip("Filter this layer before it is mixed with the other layers.");
    lowPass_.setTooltip("Filter this layer before it is mixed with the other layers.");
}

void LayerPage::paint(juce::Graphics& g) {
    const auto panel = getLocalBounds().withTrimmedTop(24).toFloat();
    g.setColour(theme::kSteel);
    g.fillRoundedRectangle(panel, 9);
    g.setColour(theme::kEdge);
    g.drawRoundedRectangle(panel.reduced(0.5f), 9, 1);
    drawSmallLabel(g, getLocalBounds().withWidth(getWidth() - 160).removeFromTop(20),
        "Layer " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer_))
        + " / key/velocity limits are inclusive / solo respects Layer on", theme::kTextDim);
}

void LayerPage::resized() {
    sectionLock_->setBounds(getWidth() - kSectionLockWidth - 12, 0, kSectionLockWidth, kSectionLockHeight);
    auto area = getLocalBounds().withTrimmedTop(42).reduced(10, 0);
    const int rowHeight = area.getHeight() / 4;
    layoutRow(area.removeFromTop(rowHeight), { &enabled_, &sound_, &level_, layerToggles_[0].get(), layerToggles_[1].get() }, 10);
    layoutRow(area.removeFromTop(rowHeight), { &pitch_, &decay_, &highPass_, &lowPass_, layerKnobs_[9].get() }, 10);
    layoutRow(area.removeFromTop(rowHeight), { layerKnobs_[0].get(), layerKnobs_[1].get(), layerKnobs_[6].get(), layerKnobs_[7].get(), layerKnobs_[8].get() }, 10);
    layoutRow(area, { layerKnobs_[2].get(), layerKnobs_[3].get(), layerKnobs_[4].get(), layerKnobs_[5].get() }, 10);
}

LayerPanel::LayerPanel(OpenEdmKickProcessor& processor) {
    std::ranges::for_each(std::views::iota(0, core::kNumSoundLayers), [&](int layer) {
        const auto index = static_cast<size_t>(layer);
        pages_[index] = std::make_unique<LayerPage>(processor, layer);
        addAndMakeVisible(*pages_[index]);
        auto& tab = tabs_[index];
        const auto letter = juce::String::charToString(static_cast<juce::juce_wchar>('A' + layer));
        tab.setButtonText("LAYER " + letter);
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

void LayerPanel::paint(juce::Graphics& g) {
    drawSmallLabel(g, getLocalBounds().removeFromTop(20), "Build one sound from four independently filtered voices",
                   theme::kTextDim);
}

void LayerPanel::resized() {
    auto area = getLocalBounds().withTrimmedTop(24);
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
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("ALL / A / B / C / D / CLICK: HIT / SHIFT-CLICK: RATCHETS 1-8", getLocalBounds().reduced(14, 5).removeFromTop(17),
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
            g.drawText("BAR " + juce::String(bar), static_cast<int>(x) + 4, static_cast<int>(grid.getY()) - 16, 46, 13,
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
    addAndMakeVisible(scene_);
    clockLock_ = std::make_unique<SectionLock>(processor.parameters, "clock", juce::StringArray{params::kAutoEnabled, params::kAutoBypass, params::kClockSync, params::kTimeSignature, params::kTempoBpm, params::kAutoSlot});
    sceneLock_ = std::make_unique<SectionLock>(processor.parameters, "scene", juce::StringArray{});
    addAndMakeVisible(*clockLock_);
    addAndMakeVisible(*sceneLock_);
    fillPreset_.setTooltip("Select a shared-lane trance pattern.");
    slotWatcher_.sendInitialUpdate();
    startTimerHz(12);
    setAdvancedMode(false);
}

void AutoPianoRollPanel::timerCallback() { syncSelectedSlot(processor_.currentAutoSlot()); }

void AutoPianoRollPanel::bindSlotControls() {
    if (sceneLock_ != nullptr) {
        juce::StringArray ids;
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
    auto area = getLocalBounds();
    drawSmallLabel(g, area.removeFromTop(21), "Sequence shared hits and individual layers", theme::kTextDim);
    const int top = 35;
    drawCard(g, { 0, top, getWidth(), kPlaybackCardHeight }, "Clock and playback", theme::kViolet);
    drawSmallLabel(g, {12, top + 143, 300, 18}, "Scene controls", theme::kBrass);
    drawCard(g, { 0, top + kPlaybackCardHeight + 11, getWidth(), getHeight() - top - kPlaybackCardHeight - 11 }, "Step editor", theme::kBrass);
}

void AutoPianoRollPanel::resized() {
    auto area = getLocalBounds().withTrimmedTop(39);
    auto card = area.removeFromTop(kPlaybackCardHeight);
    clockLock_->setBounds(card.getRight() - kSectionLockWidth - 12, card.getY() + 4,
                          kSectionLockWidth, kSectionLockHeight);
    sceneLock_->setBounds(card.getRight() - kSectionLockWidth - 12, card.getY() + 143,
                          kSectionLockWidth, kSectionLockHeight);
    auto first = card.reduced(12, 48).removeFromTop(88);
    layoutRow(first, {&scene_, &enabled_, &bypass_, &clockSync_, &timeSig_, &tempo_}, 8);
    auto second = card.withTrimmedTop(180).reduced(12, 10);
    layoutRow(second, {&bars_, &grid_, &fillPreset_, &tailShorten_, &crushSweep_}, 8);

    area.removeFromTop(11);
    pianoRoll_.setBounds(area);

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
    : reverseEnabled_(processor.parameters, params::kReverseEnabled, "Reverse MIDI", theme::kCoral),
      crushEnabled_(processor.parameters, params::kCrushEnabled, "Crush MIDI", theme::kBrass),
      stutterEnabled_(processor.parameters, params::kStutterEnabled, "Stutter MIDI", theme::kViolet),
      gateEnabled_(processor.parameters, params::kGateEnabled, "Chopper MIDI", theme::kViolet),
      highPassMidi_(processor.parameters, params::kHighPassMidi, "HP MIDI", theme::kCoral),
      lowPassMidi_(processor.parameters, params::kLowPassMidi, "LP MIDI", theme::kViolet),
      midiVolume_(processor.parameters, params::kMidiVolume, "MIDI Volume", theme::kBrass),
      midiExpression_(processor.parameters, params::kMidiExpression, "Expression", theme::kBrass),
      midiBrightness_(processor.parameters, params::kMidiBrightness, "Brightness", theme::kBrass),
      reverseMode_(processor.parameters, params::kReverseMode, "Reverse", theme::kCoral),
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
      highPassSync_(processor.parameters, params::kHighPassSyncEnabled, "HP sync", theme::kCoral),
      highPassSyncRate_(processor.parameters, params::kHighPassSyncRate, "HP rate", theme::kCoral),
      lowPassSync_(processor.parameters, params::kLowPassSyncEnabled, "LP sync", theme::kViolet),
      lowPassSyncRate_(processor.parameters, params::kLowPassSyncRate, "LP rate", theme::kViolet),
      output_(processor.parameters, params::kOutput, "Output", theme::kBrass),
      ceiling_(processor.parameters, params::kCeiling, "Ceiling", theme::kBrass),
      limiter_(processor.parameters, params::kLimiter, "Limiter", theme::kBrass),
      masterBypass_(processor.parameters, params::kMasterBypass, "Bypass all", theme::kCoral),
      meter_(processor) {
    std::ranges::for_each(std::array<juce::Component*, 23>{
        &reverseMode_, &reverseMix_, &drive_, &crushRate_, &crushBits_, &crushMix_,
        &stutterRate_, &stutterMix_, &gateRate_, &gateMix_, &pumpRate_, &pumpMix_,
        &highPass_, &lowPass_, &highPassSync_, &highPassSyncRate_, &lowPassSync_, &lowPassSyncRate_,
        &output_, &ceiling_, &limiter_, &masterBypass_, &meter_},
        [this](juce::Component* component) { addAndMakeVisible(component); });
    for (auto* component : std::array<juce::Component*, 9>{&highPassMidi_, &lowPassMidi_, &reverseEnabled_, &crushEnabled_, &stutterEnabled_, &gateEnabled_, &midiVolume_, &midiExpression_, &midiBrightness_}) addAndMakeVisible(component);
    const std::array<juce::StringArray, 4> groups{{
        {params::kReverseEnabled, params::kReverseMode, params::kReverseMix, params::kDrive, params::kMasterBypass},
        {params::kCrushEnabled, params::kCrushRate, params::kCrushBits, params::kCrushMix},
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
    setControlsVisible({ &crushRate_, &crushBits_, &crushMix_, &stutterRate_, &stutterMix_, &gateRate_, &gateMix_,
                 &pumpRate_, &pumpMix_, &highPass_, &lowPass_, &ceiling_ }, advanced);
    setControlsVisible({ &highPassSync_, &highPassSyncRate_, &lowPassSync_, &lowPassSyncRate_ }, true);
    resized();
    repaint();
}

void EffectsPanel::paint(juce::Graphics& g) {
    drawSmallLabel(g, getLocalBounds().removeFromTop(21), "Host-synced FX / MIDI activations", theme::kTextDim);
    auto area = getLocalBounds().withTrimmedTop(28);
    const int gap = 10;
    const int cardWidth = (area.getWidth() - gap) / 2;
    const int cardHeight = (area.getHeight() - gap) / 2;
    drawCard(g, { area.getX(), area.getY(), cardWidth, cardHeight }, "Reverse and drive", theme::kCoral);
    drawCard(g, { area.getX() + cardWidth + gap, area.getY(), cardWidth, cardHeight }, "Texture", theme::kBrass);
    drawCard(g, { area.getX(), area.getY() + cardHeight + gap, cardWidth, cardHeight }, "Rhythm", theme::kViolet);
    drawCard(g, { area.getX() + cardWidth + gap, area.getY() + cardHeight + gap, cardWidth, cardHeight }, "Tone and output", theme::kBrass);
    drawSmallLabel(g, meter_.getBounds().translated(0, -18).withHeight(14), "OUTPUT PEAK", theme::kTextDim);

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

    const std::array<juce::Rectangle<int>, 4> cards{reverse, texture, rhythm, tone};
    for (size_t i = 0; i < cards.size(); ++i)
        sectionLocks_[i]->setBounds(cards[i].getRight() - kSectionLockWidth - 12, cards[i].getY() + 5,
                                    kSectionLockWidth, kSectionLockHeight);
    placeRow(reverse, 0, 2, { &reverseEnabled_, &reverseMode_, &reverseMix_ });
    placeRow(reverse, 1, 2, { &drive_, &masterBypass_, &meter_ });
    const auto meterArea = meter_.getBounds();
    meter_.setBounds(meterArea.withHeight(10).withY(meterArea.getCentreY() - 5));
    placeRow(texture, 0, 2, { &crushEnabled_, &crushRate_, &crushBits_ });
    placeRow(texture, 1, 2, { &crushMix_ });
    placeRow(rhythm, 0, 3, { &stutterEnabled_, &stutterRate_, &stutterMix_ });
    placeRow(rhythm, 1, 3, { &gateEnabled_, &gateRate_, &gateMix_ });
    placeRow(rhythm, 2, 3, { &pumpRate_, &pumpMix_ });
    placeRow(tone, 0, 4, { &highPass_, &lowPass_ });
    placeRow(tone, 1, 4, { &highPassMidi_, &highPassSync_, &highPassSyncRate_, &lowPassMidi_, &lowPassSync_, &lowPassSyncRate_ });
    placeRow(tone, 2, 4, { &output_, &ceiling_, &limiter_ });
    placeRow(tone, 3, 4, { &midiVolume_, &midiExpression_, &midiBrightness_ });

    if (!advancedMode_) {
        reverseMode_.setVisible(true); reverseMix_.setVisible(true); drive_.setVisible(true);
        masterBypass_.setVisible(true); meter_.setVisible(true); output_.setVisible(true); limiter_.setVisible(true);
    }
}

} // namespace openedmkick::ui
