#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <utility>
#include <vector>

namespace openedmkick::suite {

struct EditContext {
    int program = 0;
    bool custom = false;
};

// Records UI parameter gestures in a JUCE UndoManager. One drag, click, or preset load is one step.
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    explicit ParameterUndo(juce::AudioProcessor& processor);
    ~ParameterUndo() override;

    [[nodiscard]] juce::UndoManager& manager() noexcept { return undo_; }
    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    void clear();

    void beginGroup(const juce::String& name);
    void endGroup();

    std::function<EditContext()> captureContext;
    std::function<void(const EditContext&)> restoreContext;

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

    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

    [[nodiscard]] Snapshot capture() const;
    void restore(const Snapshot& snapshot);
    void commit(const Snapshot& before, const Snapshot& after, const juce::String& name);
    [[nodiscard]] bool onUiThread() const;
    [[nodiscard]] static bool same(const Snapshot& left, const Snapshot& right);

    juce::AudioProcessor& processor_;
    juce::UndoManager undo_;
    int suppress_ = 0;
    int gestureDepth_ = 0;
    int groupDepth_ = 0;
    juce::String groupName_;
    Snapshot groupBefore_;
    Snapshot gestureBefore_;
};

} // namespace openedmkick::suite
