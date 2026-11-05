#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenEdmKickEditor : public juce::AudioProcessorEditor {
public:
    explicit OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit);
    ~OpenEdmKickEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    OpenEdmKickProcessor& processor_;
    openedmkick::ui::EdmLookAndFeel lookAndFeel_;
    juce::Image faceplate_;
    juce::ComboBox presetBox_;
    openedmkick::ui::Toggle masterBypass_;
    openedmkick::ui::KickSynthPanel synthPanel_;
    openedmkick::ui::MuleStagePanel muleStage_;
    openedmkick::ui::AutoPianoRollPanel autoPanel_;
    openedmkick::ui::EffectsPanel effectsPanel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmKickEditor)
};
