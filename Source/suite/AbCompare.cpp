#include "AbCompare.h"

namespace openedmkick::suite {

void AbCompare::seed(const juce::MemoryBlock& state) {
    slots_[0] = state;
    slots_[1] = state;
    active_ = 0;
}

void AbCompare::setSlot(int slot, const juce::MemoryBlock& state) {
    slots_[static_cast<size_t>(slot == 0 ? 0 : 1)] = state;
}

const juce::MemoryBlock& AbCompare::slot(int index) const noexcept {
    return slots_[static_cast<size_t>(index == 0 ? 0 : 1)];
}

} // namespace openedmkick::suite
