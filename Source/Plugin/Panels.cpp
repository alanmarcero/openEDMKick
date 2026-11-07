#include "Panels.h"
#include "BinaryData.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <cstring>
#include <numeric>
#include <ranges>

namespace openedmkick::ui {

namespace {

namespace core = openedmkick::core;
namespace params = openedmkick::params;

constexpr int kRefreshHz = 30;
constexpr int kSectionGap = 8;
constexpr int kCaptionHeight = 13;
constexpr std::array<int, 6> kPianoRollSemitones = { 12, 7, 5, 3, 1, 0 };
constexpr std::array<const char*, 6> kPianoRollRowLabels = {
    "+12 st", "+7 st", "+5 st", "+3 st", "+1 st", "ROOT"
};

[[nodiscard]] bool sameFloatBits(float a, float b) noexcept {
    static_assert(sizeof(float) == sizeof(uint32_t));
    uint32_t left = 0;
    uint32_t right = 0;
    std::memcpy(&left, &a, sizeof(float));
    std::memcpy(&right, &b, sizeof(float));
    return left == right;
}

[[nodiscard]] bool sameSynthSettings(const core::KickSynthSettings& a, const core::KickSynthSettings& b) noexcept {
    return a.note == b.note
        && a.octave == b.octave
        && sameFloatBits(a.fineCents, b.fineCents)
        && sameFloatBits(a.startPitchHz, b.startPitchHz)
        && sameFloatBits(a.pitchDecayMs, b.pitchDecayMs)
        && sameFloatBits(a.pitchCurve, b.pitchCurve)
        && sameFloatBits(a.tailLengthMs, b.tailLengthMs)
        && sameFloatBits(a.tailCurve, b.tailCurve)
        && sameFloatBits(a.subDrive, b.subDrive)
        && sameFloatBits(a.clickLevel, b.clickLevel)
        && sameFloatBits(a.clickFreqHz, b.clickFreqHz)
        && sameFloatBits(a.punch, b.punch)
        && sameFloatBits(a.snareLevel, b.snareLevel)
        && sameFloatBits(a.snareTone, b.snareTone)
        && sameFloatBits(a.snareDecay, b.snareDecay)
        && sameFloatBits(a.clapLevel, b.clapLevel)
        && sameFloatBits(a.clapSpread, b.clapSpread)
        && sameFloatBits(a.clapTone, b.clapTone)
        && sameFloatBits(a.clapDecay, b.clapDecay)
        && sameFloatBits(a.hatLevel, b.hatLevel)
        && sameFloatBits(a.hatTone, b.hatTone)
        && sameFloatBits(a.hatDecay, b.hatDecay);
}

bool isSharpNote(int noteIndex) noexcept {
    return noteIndex == 1 || noteIndex == 3 || noteIndex == 6 || noteIndex == 8 || noteIndex == 10;
}

void drawGlow(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour) {
    const juce::Graphics::ScopedSaveState saved(g);
    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f;
    if (radius <= 1.0f) {
        return;
    }
    g.addTransform(juce::AffineTransform::scale(1.0f, bounds.getHeight() / bounds.getWidth(), centre.x, centre.y));
    g.setGradientFill(juce::ColourGradient(colour, centre, colour.withAlpha(0.0f), centre.translated(radius, 0.0f), true));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
}

} // namespace

KickWaveformView::KickWaveformView(OpenEdmKickProcessor& processor)
    : processor_(processor) {
    startTimerHz(15);
}

void KickWaveformView::refreshCache() {
    const auto settings = processor_.currentSynthSettings();
    const bool locked = processor_.isKickLockedIn();
    const float highPass = processor_.parameters.getRawParameterValue(params::kHighPass)->load();
    const float lowPass = processor_.parameters.getRawParameterValue(params::kLowPass)->load();
    if (!cachedSamples_.empty() && sameSynthSettings(settings, cachedSettings_) && locked == cachedLocked_
        && sameFloatBits(highPass, cachedHighPass_) && sameFloatBits(lowPass, cachedLowPass_)) {
        return;
    }
    cachedSamples_ = processor_.previewKickWaveform();
    cachedSettings_ = settings;
    cachedLocked_ = locked;
    cachedHighPass_ = highPass;
    cachedLowPass_ = lowPass;
}

void KickWaveformView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kBackground.brighter(0.04f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto plot = bounds.reduced(10.0f, 16.0f);
    const float midY = plot.getCentreY();
    g.setColour(theme::kTrack.withAlpha(0.55f));
    g.drawHorizontalLine(juce::roundToInt(midY), plot.getX(), plot.getRight());

    std::ranges::for_each(std::views::iota(1, 4), [&](int div) {
        const float x = plot.getX() + plot.getWidth() * (static_cast<float>(div) / 4.0f);
        g.setColour(theme::kPanelEdge.withAlpha(0.45f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
    });

    refreshCache();
    const auto& samples = cachedSamples_;
    const bool locked = cachedLocked_;
    const auto waveColour = locked ? theme::kAccentGold : theme::kAccentMint;

    if (!samples.empty()) {
        constexpr int kPoints = 220;
        juce::Path wavePath;
        juce::Path envPath;
        std::ranges::for_each(std::views::iota(0, kPoints), [&](int i) {
            const float norm = static_cast<float>(i) / static_cast<float>(kPoints - 1);
            const size_t idx = std::min(
                static_cast<size_t>(norm * static_cast<float>(samples.size() - 1u)),
                samples.size() - 1u);
            const float s = std::clamp(samples[idx], -1.0f, 1.0f);
            const float x = plot.getX() + norm * plot.getWidth();
            const float y = midY - s * (plot.getHeight() * 0.45f);
            const float envY = midY - std::abs(s) * (plot.getHeight() * 0.46f);
            if (i == 0) {
                wavePath.startNewSubPath(x, y);
                envPath.startNewSubPath(x, envY);
                return;
            }
            wavePath.lineTo(x, y);
            envPath.lineTo(x, envY);
        });

        juce::Path envFill(envPath);
        envFill.lineTo(plot.getRight(), midY);
        envFill.lineTo(plot.getX(), midY);
        envFill.closeSubPath();
        g.setColour(waveColour.withAlpha(0.14f));
        g.fillPath(envFill);

        g.setColour(waveColour);
        g.strokePath(wavePath, juce::PathStrokeType(1.6f));
    }

    const auto synth = cachedSettings_;
    const float fundamentalHz = core::fundamentalHzForNote(synth.note, synth.octave, synth.fineCents);
    const float startHz = std::max(fundamentalHz + 20.0f, synth.startPitchHz);
    if (startHz > fundamentalHz + 5.0f && synth.tailLengthMs > 1.0f) {
        juce::Path pitchPath;
        constexpr int kPitchPoints = 72;
        std::ranges::for_each(std::views::iota(0, kPitchPoints), [&](int i) {
            const float frac = static_cast<float>(i) / static_cast<float>(kPitchPoints - 1);
            const float progress = (frac * synth.tailLengthMs) / std::max(1.0f, synth.pitchDecayMs);
            const float env = core::pitchSweepGain(progress, synth.pitchCurve);
            const float x = plot.getX() + frac * plot.getWidth();
            const float y = plot.getY() + 2.0f + (1.0f - env) * (plot.getHeight() * 0.38f);
            if (i == 0) {
                pitchPath.startNewSubPath(x, y);
                return;
            }
            pitchPath.lineTo(x, y);
        });
        g.setColour(theme::kAccentGold.withAlpha(0.9f));
        g.strokePath(pitchPath, juce::PathStrokeType(1.25f));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText("PITCH", juce::Rectangle<float>(plot.getRight() - 46.0f, plot.getY(), 42.0f, 12.0f),
                   juce::Justification::centredRight);
    }

    const auto noteName = juce::String(core::kNoteNames[static_cast<size_t>(std::clamp(synth.note, 0, 11))].data());
    const juce::String pitchLabel = "TUNED: " + noteName + juce::String(synth.octave)
        + " (" + juce::String(fundamentalHz, 1) + " Hz)  |  TAIL: " + juce::String(juce::roundToInt(synth.tailLengthMs)) + " ms";

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(theme::kText);
    g.drawText(pitchLabel, bounds.reduced(8.0f, 2.0f).removeFromTop(14.0f), juce::Justification::centredLeft);

    const juce::String modeStatus = locked
        ? "LOCKED IN: CACHED RIFF WAV (SYNTH BYPASSED)"
        : (processor_.displayReversing.load() ? "SYNC REVERSE ACTIVE" : "LIVE SYNTHESIS");
    g.setColour(locked ? theme::kAccentGold : theme::kAccentMint);
    g.drawText(modeStatus, bounds.reduced(8.0f, 2.0f).removeFromTop(14.0f), juce::Justification::centredRight);
}

KickSynthPanel::KickSynthPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      auditionButton_("TRIG"),
      triggerMode_(processor.parameters, params::kMidiMode, "", theme::kAccentMint),
      lockIn_(processor.parameters, params::kLockIn, "LOCK IN (WAV)", theme::kAccentGold),
      waveformView_(processor),
      octave_(processor.parameters, params::kOctave, "Octave", theme::kAccentMint),
      fineTune_(processor.parameters, params::kFineTune, "Fine", theme::kAccentMint),
      startPitch_(processor.parameters, params::kStartPitch, "Start Hz", theme::kAccentMint),
      pitchDecay_(processor.parameters, params::kPitchDecay, "P. Decay", theme::kAccentMint),
      pitchCurve_(processor.parameters, params::kPitchCurve, "P. Curve", theme::kAccentMint),
      punch_(processor.parameters, params::kPunch, "Punch", theme::kAccentMint),
      tailLength_(processor.parameters, params::kTailLength, "Tail Len", theme::kAccentGold),
      tailCurve_(processor.parameters, params::kTailCurve, "Tail Crv", theme::kAccentGold),
      subDrive_(processor.parameters, params::kSubDrive, "Sub Drv", theme::kAccentGold),
      clickLevel_(processor.parameters, params::kClickLevel, "Click Lvl", theme::kAccentGold),
      clickFreq_(processor.parameters, params::kClickFreq, "Click Hz", theme::kAccentGold),
      snareLevel_(processor.parameters, params::kSnareLevel, "Level", theme::kAccentGold),
      snareTone_(processor.parameters, params::kSnareTone, "Tone", theme::kAccentGold),
      snareDecay_(processor.parameters, params::kSnareDecay, "Decay", theme::kAccentGold),
      clapLevel_(processor.parameters, params::kClapLevel, "Level", theme::kAccentReverse),
      clapSpread_(processor.parameters, params::kClapSpread, "Spread", theme::kAccentReverse),
      clapTone_(processor.parameters, params::kClapTone, "Tone", theme::kAccentReverse),
      clapDecay_(processor.parameters, params::kClapDecay, "Decay", theme::kAccentReverse),
      hatLevel_(processor.parameters, params::kHatLevel, "Level", theme::kAccentMint),
      hatTone_(processor.parameters, params::kHatTone, "Tone", theme::kAccentMint),
      hatDecay_(processor.parameters, params::kHatDecay, "Decay", theme::kAccentMint),
      noteWatcher_(*processor.parameters.getParameter(params::kNote), [this](float noteValue) {
          updateNoteButtons(static_cast<int>( std::round(noteValue) ));
      }) {
    std::ranges::for_each(std::views::iota(0, core::kNumNotes), [&](int i) {
        auto& btn = noteButtons_[static_cast<size_t>(i)];
        const auto name = core::kNoteNames[static_cast<size_t>(i)];
        const bool sharp = isSharpNote(i);
        btn.setButtonText(juce::String(name.data(), name.size()));
        btn.setComponentID("note_" + juce::String(i));
        btn.setColour(juce::TextButton::buttonOnColourId, theme::kAccentMint);
        btn.setColour(juce::TextButton::buttonColourId, sharp ? juce::Colour(0xff141a1d) : juce::Colour(0xffe6dfd2));
        btn.setColour(juce::TextButton::textColourOffId, sharp ? theme::kText : juce::Colour(0xff1c2428));
        btn.setColour(juce::TextButton::textColourOnId, theme::kBackground);
        btn.setTooltip("Tune the kick to " + btn.getButtonText() + " and audition it.");
        btn.onClick = [this, i] { selectNote(i); };
        addAndMakeVisible(btn);
    });
    std::ranges::for_each(std::array{ 1, 3, 6, 8, 10 }, [&](int sharp) {
        noteButtons_[static_cast<size_t>(sharp)].toFront(false);
    });

    auditionButton_.setComponentID("audition_trig");
    auditionButton_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentMint);
    auditionButton_.setTooltip("Audition the panel tuning. Works in Auto Mode. Ignores Note tracking.");
    auditionButton_.onClick = [this] { processor_.triggerAudition(); };
    addAndMakeVisible(auditionButton_);
    triggerMode_.setTooltip("Fixed: every note plays the panel kick. Note: pitch follows the MIDI note number. Oct 1 Club F is MIDI 29. MIDI 60 is C4.");
    addAndMakeVisible(triggerMode_);
    lockIn_.setTooltip("Freeze this kick into an internal 32-bit WAV. Playback keeps that hit if the host sample rate changes.");

    std::ranges::for_each(std::initializer_list<juce::Component*>{
        &lockIn_, &triggerMode_, &waveformView_, &octave_, &fineTune_, &startPitch_, &pitchDecay_,
        &pitchCurve_, &punch_, &tailLength_, &tailCurve_, &subDrive_, &clickLevel_, &clickFreq_,
        &snareLevel_, &snareTone_, &snareDecay_, &clapLevel_, &clapSpread_, &clapTone_, &clapDecay_,
        &hatLevel_, &hatTone_, &hatDecay_
    }, [this](juce::Component* c) { addAndMakeVisible(c); });
    snareLevel_.setTooltip("Snare layer amount. Zero leaves the kick unchanged.");
    snareTone_.setTooltip("Snare color. Low is a woody shell, high is bright wire buzz.");
    snareDecay_.setTooltip("Snare ring. A long decay continues after a short kick tail. Auto Mode Tail Shortener still cuts fast rolls.");
    clapLevel_.setTooltip("Clap layer amount. Four staggered noise hits.");
    clapSpread_.setTooltip("Clap flam width. Tight is one crack, wide is four staggered hits.");
    clapTone_.setTooltip("Clap brightness. Low is a muffled hand, high is a sharp crack.");
    clapDecay_.setTooltip("Clap tail. The middle setting matches the previous clap. Spread still sets the flam.");
    hatLevel_.setTooltip("Hi-hat layer amount. Inharmonic metal plus a short chiff.");
    hatTone_.setTooltip("Hat color. Low is chunky metal, high is a thin bright chiff.");
    hatDecay_.setTooltip("Hat length. Short is a closed hat, long is an open ring that can outlast the kick tail.");

    noteWatcher_.sendInitialUpdate();
}

void KickSynthPanel::selectNote(int noteIndex) {
    const int clamped = std::clamp(noteIndex, 0, core::kNumNotes - 1);
    noteWatcher_.setValueAsCompleteGesture(static_cast<float>(clamped));
    processor_.triggerAudition();
}

void KickSynthPanel::updateNoteButtons(int activeNote) {
    std::ranges::for_each(std::views::iota(0, core::kNumNotes), [&](int i) {
        noteButtons_[static_cast<size_t>(i)].setToggleState(i == activeNote, juce::dontSendNotification);
    });
    repaint();
}

void KickSynthPanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), "Kick Synth & 12-Note Chromatic Tuner", theme::kAccentMint);
    drawCaption(g, noteBarArea_, "Chromatic keyboard. Click a key to tune and audition.");
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(theme::kAccentGold);
    g.drawText("SNARE", snareArea_.withHeight(12), juce::Justification::centredLeft);
    g.setColour(theme::kAccentReverse);
    g.drawText("CLAP", clapArea_.withHeight(12), juce::Justification::centredLeft);
    g.setColour(theme::kAccentMint);
    g.drawText("HAT", hatArea_.withHeight(12), juce::Justification::centredLeft);
}

void KickSynthPanel::resized() {
    auto bounds = getLocalBounds().reduced(12, 6);
    auto header = bounds.removeFromTop(26);
    lockIn_.setBounds(header.removeFromRight(118));
    header.removeFromRight(6);
    auditionButton_.setBounds(header.removeFromRight(58).reduced(0, 1));
    header.removeFromRight(6);
    triggerMode_.setBounds(header.removeFromRight(78).reduced(0, 1));

    bounds.removeFromTop(4);
    noteBarArea_ = bounds.removeFromTop(54);
    auto notesRow = noteBarArea_.withTrimmedTop(kCaptionHeight);
    constexpr int kNaturalCount = 7;
    constexpr int kNaturalGap = 3;
    const std::array<int, kNaturalCount> naturals{ 0, 2, 4, 5, 7, 9, 11 };
    const int naturalWidth = (notesRow.getWidth() - kNaturalGap * (kNaturalCount - 1)) / kNaturalCount;
    std::array<juce::Rectangle<int>, kNaturalCount> naturalBounds{};
    std::ranges::for_each(std::views::iota(0, kNaturalCount), [&](int i) {
        const bool last = i == kNaturalCount - 1;
        auto key = last ? notesRow : notesRow.removeFromLeft(naturalWidth);
        if (!last) {
            notesRow.removeFromLeft(kNaturalGap);
        }
        naturalBounds[static_cast<size_t>(i)] = key;
        noteButtons_[static_cast<size_t>(naturals[static_cast<size_t>(i)])].setBounds(key);
    });

    const int sharpWidth = std::max(14, (naturalWidth * 58) / 100);
    const int sharpHeight = std::max(16, (naturalBounds[0].getHeight() * 62) / 100);
    const std::array<int, 5> sharpAfter{ 0, 1, 3, 4, 5 };
    const std::array<int, 5> sharps{ 1, 3, 6, 8, 10 };
    std::ranges::for_each(std::views::iota(0, 5), [&](int i) {
        const auto& left = naturalBounds[static_cast<size_t>(sharpAfter[static_cast<size_t>(i)])];
        const int boundary = left.getRight() + kNaturalGap / 2;
        auto& sharp = noteButtons_[static_cast<size_t>(sharps[static_cast<size_t>(i)])];
        sharp.setBounds(boundary - sharpWidth / 2, left.getY(), sharpWidth, sharpHeight);
        sharp.toFront(false);
    });

    bounds.removeFromTop(6);
    waveformView_.setBounds(bounds.removeFromTop(58));
    bounds.removeFromTop(4);

    auto layers = bounds.removeFromBottom(92);
    bounds.removeFromBottom(4);
    const int layerUnit = (layers.getWidth() - 2 * kSectionGap) / 10;
    snareArea_ = layers.removeFromLeft(layerUnit * 3);
    layers.removeFromLeft(kSectionGap);
    clapArea_ = layers.removeFromLeft(layerUnit * 4);
    layers.removeFromLeft(kSectionGap);
    hatArea_ = layers;

    pitchRowArea_ = bounds.removeFromLeft((bounds.getWidth() - kSectionGap) * 6 / 11);
    bounds.removeFromLeft(kSectionGap);
    bodyRowArea_ = bounds;

    layoutRow(pitchRowArea_, { &octave_, &fineTune_, &startPitch_, &pitchDecay_, &pitchCurve_, &punch_ });
    layoutRow(bodyRowArea_, { &tailLength_, &tailCurve_, &subDrive_, &clickLevel_, &clickFreq_ });
    layoutRow(snareArea_.withTrimmedTop(kCaptionHeight), { &snareLevel_, &snareTone_, &snareDecay_ });
    layoutRow(clapArea_.withTrimmedTop(kCaptionHeight), { &clapLevel_, &clapSpread_, &clapTone_, &clapDecay_ });
    layoutRow(hatArea_.withTrimmedTop(kCaptionHeight), { &hatLevel_, &hatTone_, &hatDecay_ });
}

MuleStagePanel::MuleStagePanel(OpenEdmKickProcessor& processor)
    : processor_(processor) {
    mascot_ = juce::ImageCache::getFromMemory(BinaryData::kickingmule_png, BinaryData::kickingmule_pngSize);
    startTimerHz(kRefreshHz);
}

void MuleStagePanel::timerCallback() {
    const uint32_t serial = processor_.displayTriggerCount.load(std::memory_order_relaxed);
    if (serial != seenTriggers_) {
        seenTriggers_ = serial;
        kickStartedMs_ = juce::Time::getMillisecondCounterHiRes();
    }
    repaint();
}

void MuleStagePanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), "Kicking Mule Stage", theme::kAccentGold);

    const auto stage = getLocalBounds().reduced(10, 32).withTrimmedBottom(22).toFloat();
    const juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(stage.toNearestInt());

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff141d22), stage.getTopLeft(),
        juce::Colour(0xff090d10), stage.getBottomRight(), false));
    g.fillRoundedRectangle(stage, 10.0f);
    g.setColour(theme::kPanelEdge.withAlpha(0.6f));
    g.drawRoundedRectangle(stage.reduced(0.5f), 10.0f, 1.0f);

    const float peak = std::clamp(processor_.displayPeak.load(), 0.0f, 1.0f);
    const float beatPhase = std::clamp(processor_.displayBeatPhase.load(), 0.0f, 1.0f);
    const float pulse = std::max(peak, 0.35f * std::pow(1.0f - beatPhase, 3.0f));
    const bool locked = processor_.isKickLockedIn();
    const double kickAgeMs = juce::Time::getMillisecondCounterHiRes() - kickStartedMs_;
    const float kickPhase = kickAgeMs < 0.0 ? 0.0f : static_cast<float>(std::min(1.0, kickAgeMs / 220.0));
    float pose = 0.0f;
    float coneSquash = 1.0f;
    float shock = 0.0f;
    if (kickPhase > 0.0f && kickPhase < 1.0f) {
        if (kickPhase < 0.18f) {
            pose = -(kickPhase / 0.18f);
        } else if (kickPhase < 0.42f) {
            const float strike = (kickPhase - 0.18f) / 0.24f;
            pose = -1.0f + 2.0f * strike;
            coneSquash = 1.0f - 0.22f * std::sin(strike * std::numbers::pi_v<float>);
            shock = strike;
        } else {
            const float recoil = (kickPhase - 0.42f) / 0.58f;
            pose = 1.0f - recoil;
            shock = 1.0f - recoil;
            coneSquash = 1.0f + 0.06f * std::sin(recoil * std::numbers::pi_v<float>);
        }
    }

    // Subwoofer cone behind the Kicking Mule
    const juce::Point<float> coneCentre{ stage.getX() + stage.getWidth() * 0.36f, stage.getCentreY() + 4.0f };
    const float baseRadius = std::min(stage.getWidth(), stage.getHeight()) * 0.42f;
    const float pumpRadius = (baseRadius + pulse * 8.0f) * coneSquash;
    const juce::Rectangle<float> rimRect(
        coneCentre.x - baseRadius - 6.0f, coneCentre.y - baseRadius - 6.0f,
        (baseRadius + 6.0f) * 2.0f, (baseRadius + 6.0f) * 2.0f);

    drawGlow(g, rimRect.expanded(14.0f), (locked ? theme::kAccentGold : theme::kAccentMint).withAlpha(0.18f + pulse * 0.35f));
    g.setColour(juce::Colour(0xff232e33));
    g.fillEllipse(rimRect);
    g.setColour((locked ? theme::kAccentGold : theme::kAccentMint).withAlpha(0.35f + pulse * 0.55f));
    g.drawEllipse(rimRect.reduced(3.0f), 2.0f);

    const juce::Rectangle<float> coneRect(
        coneCentre.x - pumpRadius, coneCentre.y - pumpRadius,
        pumpRadius * 2.0f, pumpRadius * 2.0f);
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff0d1519), coneCentre,
        juce::Colour(0xff3e4e56), coneCentre.translated(pumpRadius, 0.0f), true));
    g.fillEllipse(coneRect);

    std::ranges::for_each(std::views::iota(1, 4), [&](int ring) {
        const float ringReduce = static_cast<float>(ring) * 11.0f;
        g.setColour(theme::kAccentMint.withAlpha(0.12f + pulse * 0.22f));
        g.drawEllipse(coneRect.reduced(ringReduce), 1.2f);
    });

    // Stage spotlight beams
    std::ranges::for_each(std::views::iota(0, 2), [&](int side) {
        const float srcX = side == 0 ? stage.getX() + 26.0f : stage.getRight() - 26.0f;
        const float floorY = stage.getBottom() - 4.0f;
        const auto beamCol = side == 0 ? theme::kAccentMint : theme::kAccentGold;
        juce::Path beam;
        beam.startNewSubPath(srcX - 4.0f, floorY);
        beam.lineTo(stage.getCentreX() - 45.0f, stage.getY() + 6.0f);
        beam.lineTo(stage.getCentreX() + 45.0f, stage.getY() + 6.0f);
        beam.lineTo(srcX + 4.0f, floorY);
        beam.closeSubPath();
        g.setGradientFill(juce::ColourGradient(
            beamCol.withAlpha(0.18f + pulse * 0.22f), srcX, floorY,
            beamCol.withAlpha(0.0f), stage.getCentreX(), stage.getY(), false));
        g.fillPath(beam);
    });

    // Shockwave kick rings on the right where the mule kicks
    const juce::Point<float> kickImpact{ stage.getRight() - 52.0f, stage.getCentreY() - 8.0f };
    std::ranges::for_each(std::views::iota(0, 3), [&](int wave) {
        const float r = 14.0f + static_cast<float>(wave) * 14.0f + (pulse + shock) * 12.0f;
        g.setColour(theme::kAccentGold.withAlpha((0.35f - static_cast<float>(wave) * 0.09f) * (0.35f + shock + pulse * 0.4f)));
        g.drawEllipse(kickImpact.x - r * 0.55f, kickImpact.y - r, r * 1.1f, r * 2.0f, 1.8f);
    });

    // Draw the 3D Kicking Mule mascot kicking with pulse animation
    if (mascot_.isValid()) {
        const juce::Graphics::ScopedSaveState muleSave(g);
        const float kickShiftX = pose * 18.0f + pulse * 2.0f;
        const float kickShiftY = -std::max(0.0f, pose) * 6.0f;
        g.addTransform(juce::AffineTransform::translation(kickShiftX, kickShiftY));
        g.setColour(juce::Colours::white);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        const auto muleBounds = stage.reduced(4.0f, 2.0f).withTrimmedLeft(32.0f).toNearestInt();
        g.drawImageWithin(mascot_, muleBounds.getX(), muleBounds.getY(), muleBounds.getWidth(), muleBounds.getHeight(),
                          juce::RectanglePlacement::centred);
    }

    // Bottom status bar
    const auto footer = getLocalBounds().reduced(14, 6).removeFromBottom(18);
    const auto synth = processor_.currentSynthSettings();
    const float hz = core::fundamentalHzForNote(synth.note, synth.octave, synth.fineCents);
    const auto noteStr = juce::String(core::kNoteNames[static_cast<size_t>(std::clamp(synth.note, 0, 11))].data());
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(theme::kAccentMint);
    g.drawText("KEY: " + noteStr + juce::String(synth.octave) + " (" + juce::String(hz, 1) + " Hz)",
               footer, juce::Justification::centredLeft);

    const float bpm = processor_.displayBpm.load();
    const bool hostSync = processor_.parameters.getRawParameterValue(params::kClockSync)->load() > 0.5f;
    const juce::String clockLabel = locked ? "WAV LOCKED" : (hostSync ? "HOST SYNC" : "INTERNAL CLOCK");
    g.setColour(locked ? theme::kAccentGold : theme::kTextDim);
    g.drawText(juce::String(bpm, 1) + " BPM  |  " + clockLabel, footer, juce::Justification::centredRight);
}

PianoRollView::PianoRollView(OpenEdmKickProcessor& processor)
    : processor_(processor) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(kRefreshHz);
}

juce::Rectangle<float> PianoRollView::gridArea() const {
    return getLocalBounds().toFloat().reduced(6.0f, 18.0f).withTrimmedLeft(52.0f).withTrimmedBottom(12.0f);
}

void PianoRollView::mouseDown(const juce::MouseEvent& event) {
    const auto grid = gridArea();
    if (!grid.contains(event.position)) {
        return;
    }
    const auto settings = processor_.currentAutoSlotSettings();
    const int totalSteps64 = std::max(1, settings.bars) * core::kStepsPerBar64;
    const int gridStepSize = core::kGridStepSizes64[static_cast<size_t>(
        std::clamp(settings.gridResolution, 0, static_cast<int>(core::kGridStepSizes64.size()) - 1))];

    const float relX = std::clamp((event.position.x - grid.getX()) / std::max(1.0f, grid.getWidth()), 0.0f, 0.9999f);
    const float relY = std::clamp((event.position.y - grid.getY()) / std::max(1.0f, grid.getHeight()), 0.0f, 0.9999f);

    const int rawStep64 = static_cast<int>(relX * static_cast<float>(totalSteps64));
    const int snappedStep64 = (rawStep64 / gridStepSize) * gridStepSize;
    const int rowIdx = std::clamp(
        static_cast<int>(relY * static_cast<float>(kPianoRollSemitones.size())),
        0,
        static_cast<int>(kPianoRollSemitones.size()) - 1);
    const int semitones = kPianoRollSemitones[static_cast<size_t>(rowIdx)];

    processor_.togglePianoRollNote(snappedStep64, gridStepSize, semitones);
    repaint();
}

void PianoRollView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kBackground.brighter(0.03f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto grid = gridArea();
    const auto keyArea = juce::Rectangle<float>(bounds.getX() + 6.0f, grid.getY(), 46.0f, grid.getHeight());
    const auto settings = processor_.currentAutoSlotSettings();
    const int bars = std::max(1, settings.bars);
    const int totalSteps64 = bars * core::kStepsPerBar64;
    const int numRows = static_cast<int>(kPianoRollSemitones.size());
    const float rowHeight = grid.getHeight() / static_cast<float>(numRows);

    // Draw Piano Roll keyboard rows on the left & horizontal lanes
    std::ranges::for_each(std::views::iota(0, numRows), [&](int row) {
        const float y = grid.getY() + static_cast<float>(row) * rowHeight;
        const juce::Rectangle<float> rowRect(grid.getX(), y, grid.getWidth(), rowHeight);
        const juce::Rectangle<float> keyRect(keyArea.getX(), y, keyArea.getWidth() - 2.0f, rowHeight - 1.0f);
        const bool isRoot = row == numRows - 1;
        g.setColour(row % 2 == 0 ? theme::kPanel.withAlpha(0.55f) : theme::kBackground.brighter(0.06f));
        g.fillRect(rowRect);

        g.setColour(isRoot ? theme::kAccentMint.withAlpha(0.28f) : theme::kKnobBody);
        g.fillRoundedRectangle(keyRect, 2.5f);
        g.setColour(theme::kPanelEdge);
        g.drawHorizontalLine(juce::roundToInt(y), grid.getX(), grid.getRight());

        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.setColour(isRoot ? theme::kAccentMint : theme::kTextDim);
        g.drawText(kPianoRollRowLabels[static_cast<size_t>(row)], keyRect.reduced(3.0f, 0.0f), juce::Justification::centredLeft);
    });

    // Vertical grid lines (64th-note subdivisions / beats / bars)
    const int visualDivisions = std::min(64, totalSteps64);
    std::ranges::for_each(std::views::iota(0, visualDivisions + 1), [&](int div) {
        const float frac = static_cast<float>(div) / static_cast<float>(visualDivisions);
        const float x = grid.getX() + frac * grid.getWidth();
        const bool isMajor = (div % 16) == 0;
        const bool isMedium = (div % 4) == 0;
        g.setColour(theme::kPanelEdge.withAlpha(isMajor ? 0.85f : (isMedium ? 0.45f : 0.2f)));
        g.drawVerticalLine(juce::roundToInt(x), grid.getY(), grid.getBottom());
    });

    // Bar markers along the bottom
    const int barMarkers = std::min(bars, 8);
    const int barsPerMarker = std::max(1, bars / barMarkers);
    g.setFont(juce::FontOptions(9.5f));
    std::ranges::for_each(std::views::iota(0, barMarkers), [&](int m) {
        const float frac = static_cast<float>(m) / static_cast<float>(barMarkers);
        const float x = grid.getX() + frac * grid.getWidth();
        const int barNum = m * barsPerMarker + 1;
        g.setColour(theme::kTextDim.withAlpha(0.75f));
        g.drawText("B" + juce::String(barNum),
                   juce::Rectangle<float>(x + 2.0f, grid.getBottom() + 1.0f, 34.0f, 11.0f),
                   juce::Justification::centredLeft);
    });

    // Draw Piano Roll kick note blocks
    const auto notes = processor_.currentPianoRollNotes();
    const float tailShorten = settings.tailShorten;
    std::ranges::for_each(notes, [&](const core::PianoRollNote& note) {
        if (note.step64 < 0 || note.step64 >= totalSteps64) {
            return;
        }
        const float startFrac = static_cast<float>(note.step64) / static_cast<float>(totalSteps64);
        const float lenFrac = static_cast<float>(std::max(1, note.length64)) / static_cast<float>(totalSteps64);
        const float x = grid.getX() + startFrac * grid.getWidth();
        const float w = std::max(3.5f, lenFrac * grid.getWidth() - 1.0f);

        const auto closestIt = std::ranges::min_element(kPianoRollSemitones, [&](int a, int b) {
            return std::abs(a - note.semitoneOffset) < std::abs(b - note.semitoneOffset);
        });
        const int row = static_cast<int>(std::distance(kPianoRollSemitones.begin(), closestIt));
        const float y = grid.getY() + static_cast<float>(row) * rowHeight + 1.5f;
        const juce::Rectangle<float> noteRect(x, y, w, rowHeight - 3.0f);

        const auto colour = note.reverseCue ? theme::kAccentReverse : theme::kAccentMint;
        g.setColour(colour.withAlpha(0.32f + 0.55f * std::clamp(note.velocity, 0.2f, 1.0f)));
        g.fillRoundedRectangle(noteRect, 2.5f);

        // Visualize the Auto-Mode Tail Shortener inside each note block
        const float tailFrac = std::clamp(1.0f - 0.65f * tailShorten, 0.2f, 1.0f);
        g.setColour(theme::kAccentGold.withAlpha(0.85f));
        g.fillRect(noteRect.withHeight(2.5f).withWidth(noteRect.getWidth() * tailFrac));

        g.setColour(colour);
        g.drawRoundedRectangle(noteRect, 2.5f, 1.0f);
    });

    const bool autoOn = processor_.parameters.getRawParameterValue(params::kAutoEnabled)->load() > 0.5f;
    if (!autoOn) {
        const auto banner = juce::Rectangle<float>(grid.getX(), grid.getBottom() - 18.0f, grid.getWidth(), 16.0f);
        g.setColour(theme::kBackground.withAlpha(0.88f));
        g.fillRoundedRectangle(banner, 3.0f);
        g.setColour(theme::kAccentGold);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawFittedText("AUTO MODE OFF. Edit the roll here. TRIG and MIDI audition the tuned kick.",
                         banner.toNearestInt().reduced(4, 0), juce::Justification::centredLeft, 1);
    }

    // Live playhead
    const float phase = std::clamp(processor_.displayAutoPhase.load(), 0.0f, 0.9999f);
    const float playheadX = grid.getX() + phase * grid.getWidth();
    g.setColour(theme::kAccentGold);
    g.drawVerticalLine(juce::roundToInt(playheadX), grid.getY() - 2.0f, grid.getBottom() + 2.0f);

    // Header bar over the Piano Roll
    const auto topInfo = bounds.reduced(8.0f, 2.0f).removeFromTop(14.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(theme::kAccentMint);
    const auto presetName = core::kFillPresetNames[static_cast<size_t>(std::clamp(settings.fillPreset, 0, core::kNumFillPresets - 1))];
    g.drawText("PATTERN: " + juce::String(presetName.data(), presetName.size()) + "  (" + juce::String(notes.size()) + " KICKS)",
               topInfo.withWidth(380.0f), juce::Justification::centredLeft);

    g.setColour(theme::kAccentGold);
    const int shortenPct = juce::roundToInt(settings.tailShorten * 100.0f);
    g.drawText("AUTO TAIL SHORTENER: " + juce::String(shortenPct) + "% (ANTI-MUD ON 1/64 ROLLS)  |  WINDOW: "
               + juce::String(bars) + (bars == 1 ? " BAR" : " BARS"),
               topInfo, juce::Justification::centredRight);
}

AutoPianoRollPanel::AutoPianoRollPanel(OpenEdmKickProcessor& processor)
    : processor_(processor),
      bypass_(processor.parameters, params::kAutoBypass, "BYPASS", theme::kAccentMint),
      enabled_(processor.parameters, params::kAutoEnabled, "AUTO MODE", theme::kAccentMint),
      clockSync_(processor.parameters, params::kClockSync, "HOST SYNC", theme::kAccentOut),
      timeSig_(processor.parameters, params::kTimeSignature, {}, theme::kAccentOut),
      bars_(processor.parameters, params::kAutoBars, {}, theme::kAccentMint),
      grid_(processor.parameters, params::kAutoGrid, {}, theme::kAccentMint),
      fillPreset_(processor.parameters, params::kAutoFillPreset, {}, theme::kAccentMint),
      tempo_(processor.parameters, params::kTempoBpm, "Tempo", theme::kAccentOut),
      tailShorten_(processor.parameters, params::kAutoTailShorten, "Tail Shorten", theme::kAccentGold),
      pitchRise_(processor.parameters, params::kAutoPitchRise, "Pitch Rise", theme::kAccentMint),
      crushSweep_(processor.parameters, params::kAutoCrushSweep, "Crush Ramp", theme::kAccentFx),
      pianoRoll_(processor),
      bypassWatcher_(*processor.parameters.getParameter(params::kAutoBypass), [this](float isBypassed) {
          const float alpha = isBypassed > 0.5f ? 0.35f : 1.0f;
          std::ranges::for_each(getChildren(), [&](juce::Component* child) {
              if (child != &bypass_) {
                  child->setAlpha(alpha);
              }
          });
      }),
      slotWatcher_(*processor.parameters.getParameter(params::kAutoSlot), [this](float slot) {
          selectSlot(static_cast<int>(slot));
      }) {
    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int i) {
        auto& button = slotButtons_[static_cast<size_t>(i)];
        button.setButtonText(juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)));
        button.setComponentID("auto_slot_" + juce::String(i));
        button.setColour(juce::TextButton::buttonOnColourId, theme::kAccentMint);
        button.onClick = [this, i] { slotWatcher_.setValueAsCompleteGesture(static_cast<float>(i)); };
        addAndMakeVisible(button);
    });

    quickPresetBox_.setTextWhenNothingSelected("Kick Fill Presets...");
    const auto& buildPresets = openedmkick::presets::fillBuildPresets();
    std::ranges::for_each(buildPresets, [&, id = 1](const auto& preset) mutable {
        quickPresetBox_.addItem(preset.name, id++);
    });
    quickPresetBox_.setColour(juce::ComboBox::outlineColourId, theme::kAccentGold);
    quickPresetBox_.onChange = [this] {
        const int idx = quickPresetBox_.getSelectedId() - 1;
        const auto& presets = openedmkick::presets::fillBuildPresets();
        if (idx >= 0 && idx < static_cast<int>(presets.size())) {
            openedmkick::presets::applyToCurrentAutoSlot(presets[static_cast<size_t>(idx)], processor_.parameters);
        }
    };

    std::ranges::for_each(std::initializer_list<juce::Component*>{
        &bypass_, &enabled_, &clockSync_, &timeSig_, &bars_, &grid_, &fillPreset_, &quickPresetBox_,
        &tempo_, &tailShorten_, &pitchRise_, &crushSweep_, &pianoRoll_
    }, [this](juce::Component* c) { addAndMakeVisible(c); });

    tailShorten_.setTooltip("Auto Mode only. Shortens overlapping tails on 1/16, 1/32, and 1/64 rolls. Regular MIDI is unchanged.");
    clockSync_.setTooltip("Follow the host tempo and playhead. Off uses the Tempo knob, including while the host is stopped.");
    enabled_.setTooltip("Play the selected A/B/C/D piano-roll pattern from the host clock.");
    bypassWatcher_.sendInitialUpdate();
    slotWatcher_.sendInitialUpdate();
}

void AutoPianoRollPanel::selectSlot(int slot) {
    const int safeSlot = std::clamp(slot, 0, core::kNumAutoSlots - 1);
    auto& state = processor_.parameters;
    bars_.bind(state, params::autoSlotId(safeSlot, params::kAutoBars));
    fillPreset_.bind(state, params::autoSlotId(safeSlot, params::kAutoFillPreset));
    grid_.bind(state, params::autoSlotId(safeSlot, params::kAutoGrid));
    tailShorten_.bind(state, params::autoSlotId(safeSlot, params::kAutoTailShorten));
    pitchRise_.bind(state, params::autoSlotId(safeSlot, params::kAutoPitchRise));
    crushSweep_.bind(state, params::autoSlotId(safeSlot, params::kAutoCrushSweep));

    std::ranges::for_each(std::views::iota(0, core::kNumAutoSlots), [&](int i) {
        slotButtons_[static_cast<size_t>(i)].setToggleState(i == safeSlot, juce::dontSendNotification);
    });
    quickPresetBox_.setSelectedId(0, juce::dontSendNotification);
    repaint();
}

void AutoPianoRollPanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), "Auto Mode & 1/64 Piano Roll", theme::kAccentMint);
    drawCaption(g, leftControlsArea_, "Clock & Auto Tail Shortener (Auto-Only)");
    drawCaption(g, pianoRollArea_, "Interactive Piano Roll (Click Grid to Toggle 1/4 .. 1/64 Kick Notes)");

    g.setColour(theme::kTextDim);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("AUTO MODE", slotRowArea_.withWidth(74), juce::Justification::centredLeft);

    auto legend = slotRowArea_.withTrimmedLeft(260);
    g.setColour(theme::kAccentMint);
    g.drawText("FORWARD KICK (1/4 .. 1/64)", legend.removeFromLeft(175), juce::Justification::centredLeft);
    g.setColour(theme::kAccentGold);
    g.drawText("TAIL SHORTENER GATE", legend.removeFromLeft(155), juce::Justification::centredLeft);
    g.setColour(theme::kAccentReverse);
    g.drawText("BEAT 4 SYNC REVERSE", legend.removeFromLeft(160), juce::Justification::centredLeft);
}

void AutoPianoRollPanel::resized() {
    auto bounds = getLocalBounds().reduced(12, 6);
    auto header = bounds.removeFromTop(26);
    quickPresetBox_.setBounds(header.removeFromRight(170).withSizeKeepingCentre(170, 24));
    header.removeFromRight(5);
    fillPreset_.setBounds(header.removeFromRight(175));
    header.removeFromRight(5);
    grid_.setBounds(header.removeFromRight(68));
    header.removeFromRight(5);
    bars_.setBounds(header.removeFromRight(78));
    header.removeFromRight(5);
    timeSig_.setBounds(header.removeFromRight(58));
    header.removeFromRight(5);
    clockSync_.setBounds(header.removeFromRight(80));
    header.removeFromRight(5);
    enabled_.setBounds(header.removeFromRight(86));
    header.removeFromRight(5);
    bypass_.setBounds(header.removeFromRight(66));

    bounds.removeFromTop(4);
    slotRowArea_ = bounds.removeFromTop(24);
    auto slotBtns = slotRowArea_.withTrimmedLeft(76);
    std::ranges::for_each(slotButtons_, [&](juce::TextButton& btn) {
        btn.setBounds(slotBtns.removeFromLeft(38).reduced(2, 1));
    });

    bounds.removeFromTop(4);
    leftControlsArea_ = bounds.removeFromLeft(280);
    bounds.removeFromLeft(kSectionGap);
    pianoRollArea_ = bounds;

    layoutRow(leftControlsArea_.withTrimmedTop(kCaptionHeight), { &tempo_, &tailShorten_, &pitchRise_, &crushSweep_ });
    pianoRoll_.setBounds(pianoRollArea_.withTrimmedTop(kCaptionHeight));
}

PeakMeter::PeakMeter(OpenEdmKickProcessor& processor)
    : processor_(processor) {
    startTimerHz(30);
    setTooltip("Pre-limiter peak. The gold tick is the limiter ceiling; the bar turns hot when the peak is above it.");
}

void PeakMeter::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kBackground.brighter(0.08f));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);

    const float peak = std::clamp(processor_.displayPeak.load(), 0.0f, 1.5f);
    const float ceilingDb = processor_.parameters.getRawParameterValue(params::kCeiling)->load();
    const float ceiling = juce::Decibels::decibelsToGain(ceilingDb);
    constexpr float kMeterFullScale = 1.2f;
    auto fill = bounds.reduced(2.0f);
    const float levelHeight = fill.getHeight() * std::clamp(peak / kMeterFullScale, 0.0f, 1.0f);
    const auto level = fill.removeFromBottom(levelHeight);
    const bool over = peak > ceiling + 0.001f;
    g.setColour(over ? juce::Colour(0xffe07a6a) : theme::kAccentMint);
    if (levelHeight > 0.5f) {
        g.fillRoundedRectangle(level, 2.0f);
    }
    const float tick = bounds.getBottom() - 2.0f - (bounds.getHeight() - 4.0f) * std::clamp(ceiling / kMeterFullScale, 0.0f, 1.0f);
    g.setColour(theme::kAccentGold);
    g.drawHorizontalLine(juce::roundToInt(tick), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
}

EffectsPanel::EffectsPanel(OpenEdmKickProcessor& processor)
    : reverseMode_(processor.parameters, params::kReverseMode, "Sync Mode", theme::kAccentReverse),
      reverseMix_(processor.parameters, params::kReverseMix, "Rev Mix", theme::kAccentReverse),
      crushRate_(processor.parameters, params::kCrushRate, "Rate", theme::kAccentFx),
      crushBits_(processor.parameters, params::kCrushBits, "Bits", theme::kAccentFx),
      crushMix_(processor.parameters, params::kCrushMix, "Mix", theme::kAccentFx),
      stutterRate_(processor.parameters, params::kStutterRate, "Sync Div", theme::kAccentMint),
      stutterMix_(processor.parameters, params::kStutterMix, "Mix", theme::kAccentMint),
      gateRate_(processor.parameters, params::kGateRate, "Gate Div", theme::kAccentGold),
      gateMix_(processor.parameters, params::kGateMix, "Gate Mix", theme::kAccentGold),
      pumpRate_(processor.parameters, params::kPumpRate, "Pump Div", theme::kAccentGold),
      pumpMix_(processor.parameters, params::kPumpMix, "Pump Mix", theme::kAccentGold),
      drive_(processor.parameters, params::kDrive, "Drive", theme::kAccentOut),
      output_(processor.parameters, params::kOutput, "Output", theme::kAccentOut),
      ceiling_(processor.parameters, params::kCeiling, "Ceiling", theme::kAccentOut),
      limiter_(processor.parameters, params::kLimiter, "LIMIT", theme::kAccentOut),
      highPass_(processor.parameters, params::kHighPass, "High Pass", theme::kAccentFx),
      lowPass_(processor.parameters, params::kLowPass, "Low Pass", theme::kAccentFx),
      meter_(processor),
      sections_{ {
          { "Reverse Sync", theme::kAccentReverse, 2, {} },
          { "Bitcrush", theme::kAccentFx, 3, {} },
          { "Beat Stutter", theme::kAccentMint, 2, {} },
          { "Sync Gate & Pump", theme::kAccentGold, 4, {} },
          { "Filter", theme::kAccentFx, 3, {} },
          { "Drive & Output", theme::kAccentOut, 4, {} }
      } } {
    std::ranges::for_each(std::initializer_list<juce::Component*>{
        &reverseMode_, &reverseMix_, &crushRate_, &crushBits_, &crushMix_,
        &stutterRate_, &stutterMix_, &gateRate_, &gateMix_, &pumpRate_, &pumpMix_,
        &drive_, &output_, &ceiling_, &limiter_, &highPass_, &lowPass_, &meter_
    }, [this](juce::Component* c) { addAndMakeVisible(c); });
    reverseMode_.setTooltip("Beat 4 Auto reverses kicks that land on the last beat so the swell ends on the bar line.");
    limiter_.setTooltip("Zero-latency ceiling. Transients stay on the grid and are caught on the sample that crosses the ceiling.");
    highPass_.setTooltip("High-pass cutoff. Minimum is off. No resonance.");
    lowPass_.setTooltip("Low-pass cutoff. Maximum is off. No resonance.");
}

void EffectsPanel::paint(juce::Graphics& g) {
    std::ranges::for_each(sections_, [&](const Section& section) {
        drawPanel(g, section.area, section.title, section.accent);
    });
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(theme::kTextDim);
    g.drawText("v" JucePlugin_VersionString, getLocalBounds().reduced(8, 2), juce::Justification::bottomRight);
}

void EffectsPanel::resized() {
    auto bounds = getLocalBounds();
    const int totalUnits = std::accumulate(sections_.begin(), sections_.end(), 0,
        [](int sum, const Section& s) { return sum + s.units; });
    const int gaps = kSectionGap * static_cast<int>(sections_.size() - 1);
    const int unitWidth = (bounds.getWidth() - gaps) / totalUnits;

    std::ranges::for_each(sections_, [&](Section& section) {
        section.area = bounds.removeFromLeft(section.units * unitWidth);
        bounds.removeFromLeft(kSectionGap);
    });
    sections_.back().area = sections_.back().area.withRight(getWidth());

    const auto content = [&](size_t idx) {
        return sections_[idx].area.reduced(10, 0).withTrimmedTop(32).withTrimmedBottom(6);
    };
    layoutRow(content(0), { &reverseMode_, &reverseMix_ });
    layoutRow(content(1), { &crushRate_, &crushBits_, &crushMix_ });
    layoutRow(content(2), { &stutterRate_, &stutterMix_ });
    layoutRow(content(3), { &gateRate_, &gateMix_, &pumpRate_, &pumpMix_ });
    layoutRow(content(4), { &highPass_, &lowPass_ });
    auto outputRow = content(5);
    meter_.setBounds(outputRow.removeFromRight(18).reduced(1, 6));
    outputRow.removeFromRight(4);
    layoutRow(outputRow, { &drive_, &output_, &ceiling_, &limiter_ });
}

} // namespace openedmkick::ui
