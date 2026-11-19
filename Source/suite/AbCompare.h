#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace openedmkick::suite {

// Two full plugin-state slots. The active slot tracks the live sound.
class AbCompare {
public:
    void seed(const juce::MemoryBlock& state);
    void setSlot(int slot, const juce::MemoryBlock& state);
    [[nodiscard]] const juce::MemoryBlock& slot(int index) const noexcept;
    [[nodiscard]] int active() const noexcept { return active_; }
    void setActive(int slot) noexcept { active_ = slot == 0 ? 0 : 1; }

private:
    std::array<juce::MemoryBlock, 2> slots_{};
    int active_ = 0;
};

} // namespace openedmkick::suite
