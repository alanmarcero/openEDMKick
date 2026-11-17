#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenEdmKickEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer {
public:
    explicit OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit);
    ~OpenEdmKickEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedView(bool advanced);
    [[nodiscard]] bool isAdvancedView() const noexcept { return synthPanel_.isAdvancedMode(); }

private:
    enum class Page { kick, layers, pattern, effects };
    void timerCallback() override;
    void syncPresetBox();
    void setPage(Page page);

    OpenEdmKickProcessor& processor_;
    openedmkick::ui::EdmLookAndFeel lookAndFeel_;
    juce::Viewport workspace_;
    juce::ComboBox presetBox_;
    juce::TextButton auditionBtn_{"Audition C2"};
    juce::TextButton prevPresetBtn_{"Previous"};
    juce::TextButton nextPresetBtn_{"Next"};
    juce::TextButton viewToggleBtn_{"Advanced controls"};
    juce::TextButton kickTab_{"Kick"};
    juce::TextButton layersTab_{"Layers"};
    juce::TextButton patternTab_{"Pattern"};
    juce::TextButton effectsTab_{"Effects"};
    openedmkick::ui::KickSynthPanel synthPanel_;
    openedmkick::ui::LayerPanel layerPanel_;
    openedmkick::ui::AutoPianoRollPanel autoPanel_;
    openedmkick::ui::EffectsPanel effectsPanel_;
    juce::TooltipWindow tooltipWindow_;
    Page page_ = Page::kick;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmKickEditor)
};
