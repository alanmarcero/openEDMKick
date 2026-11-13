#pragma once

#include "EdmLookAndFeel.h"
#include "Panels.h"
#include "PluginProcessor.h"

class OpenEdmKickEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer,
                                private juce::AudioProcessorListener {
public:
    explicit OpenEdmKickEditor(OpenEdmKickProcessor& processorToEdit);
    ~OpenEdmKickEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAdvancedView(bool advanced);
    [[nodiscard]] bool isAdvancedView() const noexcept { return synthPanel_.isAdvancedMode(); }

private:
    enum class Page { kick, pattern, effects };
    void timerCallback() override;
    void audioProcessorChanged(juce::AudioProcessor* processor, const ChangeDetails& details) override;
    void audioProcessorParameterChanged(juce::AudioProcessor* processor, int parameterIndex, float newValue) override;
    void syncPresetBox();
    void setPage(Page page);

    OpenEdmKickProcessor& processor_;
    openedmkick::ui::EdmLookAndFeel lookAndFeel_;
    juce::ComboBox presetBox_;
    juce::TextButton prevPresetBtn_{"<"};
    juce::TextButton nextPresetBtn_{">"};
    juce::TextButton viewToggleBtn_{"ADVANCED"};
    juce::TextButton kickTab_{"KICK"};
    juce::TextButton patternTab_{"PATTERN"};
    juce::TextButton effectsTab_{"FX"};
    openedmkick::ui::KickSynthPanel synthPanel_;
    openedmkick::ui::AutoPianoRollPanel autoPanel_;
    openedmkick::ui::EffectsPanel effectsPanel_;
    juce::TooltipWindow tooltipWindow_;
    Page page_ = Page::kick;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenEdmKickEditor)
};
