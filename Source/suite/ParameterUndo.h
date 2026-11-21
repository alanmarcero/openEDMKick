#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <functional>
#include <utility>
#include <vector>

namespace openedmkick::suite {

struct EditContext {
    int program = 0;
    bool custom = false;
    juce::String presetName;
    bool edited = false;
};

// Records UI parameter gestures in one JUCE UndoManager per A/B slot. One drag, click, or preset load is one step.
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    explicit ParameterUndo(juce::AudioProcessor& processor);
    ~ParameterUndo() override;

    [[nodiscard]] juce::UndoManager& manager() noexcept { return histories_[active_]; }
    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    void clear();
    void clearHistory(int slot);
    void selectHistory(int slot);

    void beginGroup(const juce::String& name);
    void endGroup();

    std::function<EditContext()> captureContext;
    std::function<void(const EditContext&)> restoreContext;
    // Runs on the thread that changed the value, which can be the audio thread.
    std::function<void()> onParameterChange;

private:
    struct Value {
        juce::String id;
        float normalised = 0.0f;
    };
    struct Snapshot {
        std::vector<Value> values;
        EditContext context;
    };

    class ChangeAction;

    void parameterValueChanged(int, float) override { if (onParameterChange) onParameterChange(); }
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

    [[nodiscard]] Snapshot capture() const;
    void restore(const Snapshot& snapshot);
    void commit(const Snapshot& before, const Snapshot& after, const juce::String& name);
    [[nodiscard]] bool onUiThread() const;
    [[nodiscard]] static bool same(const Snapshot& left, const Snapshot& right);

    juce::AudioProcessor& processor_;
    std::array<juce::UndoManager, 2> histories_;
    size_t active_ = 0;
    int suppress_ = 0;
    int gestureDepth_ = 0;
    int groupDepth_ = 0;
    juce::String groupName_;
    Snapshot groupBefore_;
    Snapshot gestureBefore_;
};

} // namespace openedmkick::suite
