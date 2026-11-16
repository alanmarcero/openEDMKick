#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <memory>
#include <vector>

class ParameterRouting final : private juce::AudioProcessorParameter::Listener {
public:
    explicit ParameterRouting(juce::AudioProcessorValueTreeState&);
    ~ParameterRouting() override;
    void manualValue(const juce::String&, float);
    void setLocked(const juce::String&, bool);
    bool isLocked(const juce::String&) const;
    void toggleSection(const juce::StringArray&);
    bool sectionLocked(const juce::StringArray&) const;
    void assignCc(const juce::String&, int);
    int assignment(const juce::String&) const;
    int defaultAssignment(const juce::String&) const;
    juce::PopupMenu midiMenu(const juce::String&) const;
    float displayValue(const juce::String&) const;
    bool isOverridden(const juce::String&) const;
    std::atomic<float>* effectiveRaw(const juce::String&);
    bool applyCc(int controller, int value) noexcept;
    void clearPerformance() noexcept;
    void saveSample() noexcept;
    void restoreSample() noexcept;
    void writeState(juce::ValueTree&) const;
    void restoreState(const juce::ValueTree&);
    struct Entry {
        juce::RangedAudioParameter* parameter;
        std::atomic<float>* host;
        std::atomic<float> effective{0};
        std::atomic<int> cc{-1};
        std::atomic<bool> locked{false}, active{false};
        std::atomic<uint32_t> revision{0};
        float baseline = 0;
        bool baselineActive = false;
        uint32_t baselineRevision = 0;
    };
    struct LockedValue { Entry* entry; float normalized, effective; bool active; };
    std::vector<LockedValue> lockedValues() const;
    void restoreLocked(const std::vector<LockedValue>&);
private:
    Entry* find(const juce::String&) const;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}
    std::vector<std::unique_ptr<Entry>> entries_;
};
