#include "ParameterRouting.h"
#include <algorithm>

ParameterRouting::ParameterRouting(juce::AudioProcessorValueTreeState& state) {
    for (auto* parameter : state.processor.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        if (ranged == nullptr) continue;
        auto entry = std::make_unique<Entry>();
        entry->parameter = ranged;
        entry->host = state.getRawParameterValue(ranged->paramID);
        entry->effective.store(entry->host->load());
        entry->cc.store(defaultAssignment(ranged->paramID));
        entries_.push_back(std::move(entry));
        ranged->addListener(this);
    }
}
ParameterRouting::~ParameterRouting() {
    for (const auto& entry : entries_) entry->parameter->removeListener(this);
}
ParameterRouting::Entry* ParameterRouting::find(const juce::String& id) const {
    for (const auto& entry : entries_) if (entry->parameter->paramID == id) return entry.get();
    return nullptr;
}
void ParameterRouting::parameterValueChanged(int index, float normalized) {
    for (const auto& entry : entries_) {
        if (entry->parameter->getParameterIndex() != index) continue;
        entry->revision.fetch_add(1);
        entry->effective.store(entry->parameter->convertFrom0to1(normalized));
        entry->active.store(false);
        return;
    }
}
void ParameterRouting::manualValue(const juce::String& id, float value) {
    if (auto* entry = find(id)) {
        entry->revision.fetch_add(1);
        entry->effective.store(value);
        entry->active.store(false);
    }
}
int ParameterRouting::defaultAssignment(const juce::String&) const {
    return -1;
}
void ParameterRouting::setLocked(const juce::String& id, bool locked) { if (auto* entry = find(id)) entry->locked.store(locked); }
bool ParameterRouting::isLocked(const juce::String& id) const { if (auto* entry = find(id)) return entry->locked.load(); return false; }
bool ParameterRouting::sectionLocked(const juce::StringArray& ids) const {
    return !ids.isEmpty() && std::all_of(ids.begin(), ids.end(), [&](const auto& id) { return isLocked(id); });
}
void ParameterRouting::toggleSection(const juce::StringArray& ids) {
    const bool locked = !sectionLocked(ids);
    for (const auto& id : ids) setLocked(id, locked);
}
void ParameterRouting::assignCc(const juce::String& id, int cc) {
    if (auto* entry = find(id)) {
        entry->revision.fetch_add(1);
        entry->cc.store(std::clamp(cc, -1, 127));
        entry->active.store(false);
        entry->effective.store(entry->host->load());
    }
}
int ParameterRouting::assignment(const juce::String& id) const { if (auto* entry = find(id)) return entry->cc.load(); return -1; }
float ParameterRouting::displayValue(const juce::String& id) const { if (auto* entry = find(id)) return entry->effective.load(); return 0; }
bool ParameterRouting::isOverridden(const juce::String& id) const { if (auto* entry = find(id)) return entry->active.load(); return false; }
std::atomic<float>* ParameterRouting::effectiveRaw(const juce::String& id) { if (auto* entry = find(id)) return &entry->effective; return nullptr; }
juce::PopupMenu ParameterRouting::midiMenu(const juce::String& id) const {
    juce::PopupMenu menu;
    const int selected = assignment(id);
    menu.addItem(1, "Unassigned (default)", true, selected == -1);
    for (int cc = 0; cc < 128; ++cc) menu.addItem(cc + 2, "CC " + juce::String(cc), true, selected == cc);
    menu.addItem(130, "Restore factory assignment");
    return menu;
}
bool ParameterRouting::applyCc(int, int) noexcept {
    return false;
}
void ParameterRouting::clearPerformance() noexcept {
    for (const auto& entry : entries_) {
        entry->effective.store(entry->host->load());
        entry->active.store(false);
    }
}
void ParameterRouting::saveSample() noexcept {
    for (const auto& entry : entries_) {
        entry->baselineRevision = entry->revision.load();
        entry->baseline = entry->effective.load();
        entry->baselineActive = entry->active.load();
    }
}
void ParameterRouting::restoreSample() noexcept {
    for (const auto& entry : entries_) {
        if (entry->revision.load() != entry->baselineRevision) continue;
        entry->effective.store(entry->baseline);
        entry->active.store(entry->baselineActive);
    }
}
std::vector<ParameterRouting::LockedValue> ParameterRouting::lockedValues() const {
    std::vector<LockedValue> values;
    for (const auto& entry : entries_) if (entry->locked.load())
        values.push_back({entry.get(), entry->parameter->getValue(), entry->effective.load(), entry->active.load()});
    return values;
}
void ParameterRouting::restoreLocked(const std::vector<LockedValue>& values) {
    for (const auto& value : values) {
        value.entry->parameter->setValueNotifyingHost(value.normalized);
        value.entry->effective.store(value.effective);
        value.entry->active.store(value.active);
    }
}
void ParameterRouting::writeState(juce::ValueTree& state) const {
    state.removeChild(state.getChildWithName("ParameterRouting"), nullptr);
    juce::ValueTree metadata("ParameterRouting");
    for (const auto& entry : entries_) {
        juce::ValueTree parameter("Parameter");
        parameter.setProperty("id", entry->parameter->paramID, nullptr);
        parameter.setProperty("locked", entry->locked.load(), nullptr);
        metadata.appendChild(parameter, nullptr);
    }
    state.appendChild(metadata, nullptr);
}
void ParameterRouting::restoreState(const juce::ValueTree& state) {
    const auto metadata = state.getChildWithName("ParameterRouting");
    for (const auto& entry : entries_) {
        auto parameter = metadata.getChildWithProperty("id", entry->parameter->paramID);
        if (!parameter.isValid() && entry->parameter->paramID == "limiter_enabled")
            parameter = metadata.getChildWithProperty("id", "limiter");
        entry->locked.store(parameter.getProperty("locked", false));
        assignCc(entry->parameter->paramID, -1);
    }
}
