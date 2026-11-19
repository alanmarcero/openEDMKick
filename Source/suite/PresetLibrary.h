#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace openedmkick::suite {

struct PresetEntry {
    juce::String name;
    bool factory = false;
    bool favorite = false;
    int factoryIndex = -1;
    juce::File file;
};

// Factory names are read-only. User presets live in ~/Documents/Alan Marcero/<plugin>/Presets.
class PresetLibrary {
public:
    PresetLibrary(juce::String pluginName, std::vector<juce::String> factoryNames);

    [[nodiscard]] const juce::File& directory() const noexcept { return directory_; }
    void setDirectory(juce::File directory);

    [[nodiscard]] std::vector<PresetEntry> list(const juce::String& query) const;
    [[nodiscard]] bool isFactory(const juce::String& name) const;
    bool save(const juce::String& name, const juce::MemoryBlock& state);
    bool rename(const juce::String& from, const juce::String& to);
    bool remove(const juce::String& name);
    bool load(const juce::String& name, juce::MemoryBlock& state) const;
    bool setFavorite(const juce::String& name, bool favorite);

private:
    [[nodiscard]] juce::String sanitise(juce::String name) const;
    [[nodiscard]] juce::File fileFor(const juce::String& name) const;
    [[nodiscard]] std::vector<juce::String> readFavorites() const;
    bool writeFavorites(const std::vector<juce::String>& names) const;

    juce::String pluginName_;
    std::vector<juce::String> factoryNames_;
    juce::File directory_;
};

} // namespace openedmkick::suite
