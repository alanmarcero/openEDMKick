#include "ParameterUndo.h"

#include <algorithm>

namespace openedmkick::suite {

class ParameterUndo::ChangeAction : public juce::UndoableAction {
public:
    ChangeAction(ParameterUndo& owner, Snapshot before, Snapshot after)
        : owner_(owner), before_(std::move(before)), after_(std::move(after)) {}

    bool perform() override {
        if (first_) {
            first_ = false;
            return true;
        }
        owner_.restore(after_);
        return true;
    }

    bool undo() override {
        owner_.restore(before_);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    ParameterUndo& owner_;
    Snapshot before_;
    Snapshot after_;
    bool first_ = true;
};

ParameterUndo::ParameterUndo(juce::AudioProcessor& processor)
    : processor_(processor) {
    for (auto* parameter : processor_.getParameters())
        parameter->addListener(this);
}

ParameterUndo::~ParameterUndo() {
    for (auto* parameter : processor_.getParameters())
        parameter->removeListener(this);
}

bool ParameterUndo::undo() { return undo_.undo(); }
bool ParameterUndo::redo() { return undo_.redo(); }
bool ParameterUndo::canUndo() const { return undo_.canUndo(); }
bool ParameterUndo::canRedo() const { return undo_.canRedo(); }
void ParameterUndo::clear() { undo_.clearUndoHistory(); }

void ParameterUndo::beginGroup(const juce::String& name) {
    if (suppress_ > 0) return;
    if (groupDepth_++ == 0) {
        groupName_ = name;
        groupBefore_ = capture();
    }
}

void ParameterUndo::endGroup() {
    if (suppress_ > 0 || groupDepth_ <= 0) return;
    if (--groupDepth_ > 0) return;
    commit(groupBefore_, capture(), groupName_);
}

void ParameterUndo::parameterGestureChanged(int, bool gestureIsStarting) {
    if (suppress_ > 0 || groupDepth_ > 0 || !onUiThread()) return;
    if (gestureIsStarting) {
        if (gestureDepth_++ == 0) gestureBefore_ = capture();
        return;
    }
    if (gestureDepth_ == 0) return;
    if (--gestureDepth_ == 0) commit(gestureBefore_, capture(), {});
}

ParameterUndo::Snapshot ParameterUndo::capture() const {
    Snapshot snapshot;
    snapshot.values.reserve(static_cast<size_t>(processor_.getParameters().size()));
    for (auto* parameter : processor_.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            snapshot.values.push_back({ ranged->paramID, ranged->getValue() });
    }
    if (captureContext) snapshot.context = captureContext();
    return snapshot;
}

void ParameterUndo::restore(const Snapshot& snapshot) {
    juce::ScopedValueSetter<int> guard(suppress_, suppress_ + 1);
    for (const auto& value : snapshot.values) {
        for (auto* parameter : processor_.getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
            if (ranged == nullptr || ranged->paramID != value.id) continue;
            if (!juce::approximatelyEqual(ranged->getValue(), value.normalised))
                ranged->setValueNotifyingHost(value.normalised);
            break;
        }
    }
    if (restoreContext) restoreContext(snapshot.context);
}

void ParameterUndo::commit(const Snapshot& before, const Snapshot& after, const juce::String& name) {
    if (same(before, after)) return;
    undo_.beginNewTransaction(name);
    undo_.perform(new ChangeAction(*this, before, after));
}

bool ParameterUndo::onUiThread() const {
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    return manager != nullptr && manager->isThisTheMessageThread();
}

bool ParameterUndo::same(const Snapshot& left, const Snapshot& right) {
    if (left.context.program != right.context.program || left.context.custom != right.context.custom)
        return false;
    if (left.values.size() != right.values.size()) return false;
    for (size_t index = 0; index < left.values.size(); ++index) {
        if (left.values[index].id != right.values[index].id) return false;
        if (!juce::approximatelyEqual(left.values[index].normalised, right.values[index].normalised))
            return false;
    }
    return true;
}

} // namespace openedmkick::suite
