#include "PresetLibrary.h"

#include <algorithm>

namespace openedmkick::suite {

PresetLibrary::PresetLibrary(juce::String pluginName, std::vector<juce::String> factoryNames)
    : pluginName_(std::move(pluginName)),
      factoryNames_(std::move(factoryNames)),
      directory_(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                     .getChildFile("Alan Marcero")
                     .getChildFile(pluginName_)
                     .getChildFile("Presets")) {}

void PresetLibrary::setDirectory(juce::File directory) { directory_ = std::move(directory); }

juce::String PresetLibrary::sanitise(juce::String name) const {
    name = name.trim().substring(0, 80);
    name = name.replaceCharacters("/\\:*?\"<>|", "_________");
    while (name.contains("..")) name = name.replace("..", ".");
    return name.trim();
}

juce::File PresetLibrary::fileFor(const juce::String& name) const {
    return directory_.getChildFile(sanitise(name) + ".oekpreset");
}

bool PresetLibrary::isFactory(const juce::String& name) const {
    const auto trimmed = name.trim();
    return std::find(factoryNames_.begin(), factoryNames_.end(), trimmed) != factoryNames_.end();
}

std::vector<juce::String> PresetLibrary::readFavorites() const {
    std::vector<juce::String> names;
    const auto file = directory_.getChildFile("favorites.txt");
    if (!file.existsAsFile()) return names;
    for (auto& line : juce::StringArray::fromLines(file.loadFileAsString())) {
        line = line.trim();
        if (line.isNotEmpty()) names.push_back(line);
    }
    return names;
}

bool PresetLibrary::writeFavorites(const std::vector<juce::String>& names) const {
    directory_.createDirectory();
    juce::String text;
    for (const auto& name : names) text << name << '\n';
    return directory_.getChildFile("favorites.txt").replaceWithText(text);
}

std::vector<PresetEntry> PresetLibrary::list(const juce::String& query) const {
    const auto favorites = readFavorites();
    const auto starred = [&](const juce::String& name) {
        return std::find(favorites.begin(), favorites.end(), name) != favorites.end();
    };
    std::vector<PresetEntry> entries;
    for (size_t index = 0; index < factoryNames_.size(); ++index) {
        const auto& name = factoryNames_[index];
        if (query.isNotEmpty() && !name.containsIgnoreCase(query)) continue;
        entries.push_back({ name, true, starred(name), static_cast<int>(index), {} });
    }
    std::vector<PresetEntry> user;
    if (directory_.isDirectory()) {
        for (const auto& file : directory_.findChildFiles(juce::File::findFiles, false, "*.oekpreset")) {
            const auto name = file.getFileNameWithoutExtension();
            if (query.isNotEmpty() && !name.containsIgnoreCase(query)) continue;
            user.push_back({ name, false, starred(name), -1, file });
        }
    }
    std::sort(user.begin(), user.end(), [](const PresetEntry& left, const PresetEntry& right) {
        return left.name.compareNatural(right.name) < 0;
    });
    entries.insert(entries.end(), user.begin(), user.end());
    std::stable_partition(entries.begin(), entries.end(), [](const PresetEntry& entry) { return entry.favorite; });
    return entries;
}

bool PresetLibrary::save(const juce::String& name, const juce::MemoryBlock& state) {
    if (name.trim().isEmpty() || isFactory(name) || state.getSize() == 0) return false;
    const auto clean = sanitise(name);
    if (clean.isEmpty()) return false;
    if (!directory_.createDirectory()) return false;
    auto stream = fileFor(clean).createOutputStream();
    if (stream == nullptr) return false;
    const bool wrote = stream->write(state.getData(), state.getSize());
    stream->flush();
    return wrote;
}

bool PresetLibrary::rename(const juce::String& from, const juce::String& to) {
    if (isFactory(from) || isFactory(to)) return false;
    const auto source = sanitise(from);
    const auto target = sanitise(to);
    if (source.isEmpty() || target.isEmpty()) return false;
    const auto file = fileFor(source);
    if (!file.existsAsFile()) return false;
    if (source == target) return true;
    const auto destination = fileFor(target);
    if (destination.existsAsFile()) return false;
    if (!file.moveFileTo(destination)) return false;
    auto favorites = readFavorites();
    bool changed = false;
    for (auto& name : favorites) {
        if (name == source) {
            name = target;
            changed = true;
        }
    }
    if (changed) writeFavorites(favorites);
    return true;
}

bool PresetLibrary::remove(const juce::String& name) {
    if (isFactory(name)) return false;
    const auto clean = sanitise(name);
    if (clean.isEmpty()) return false;
    const auto file = fileFor(clean);
    if (!file.existsAsFile() || !file.deleteFile()) return false;
    auto favorites = readFavorites();
    const auto end = std::remove(favorites.begin(), favorites.end(), clean);
    if (end != favorites.end()) {
        favorites.erase(end, favorites.end());
        writeFavorites(favorites);
    }
    return true;
}

bool PresetLibrary::load(const juce::String& name, juce::MemoryBlock& state) const {
    const auto file = fileFor(sanitise(name));
    if (!file.existsAsFile()) return false;
    return file.loadFileAsData(state) && state.getSize() > 0;
}

bool PresetLibrary::setFavorite(const juce::String& name, bool favorite) {
    const auto clean = name.trim();
    if (clean.isEmpty()) return false;
    if (!isFactory(clean) && !fileFor(sanitise(clean)).existsAsFile()) return false;
    auto favorites = readFavorites();
    const auto found = std::find(favorites.begin(), favorites.end(), clean);
    if (favorite && found == favorites.end()) favorites.push_back(clean);
    if (!favorite && found != favorites.end()) favorites.erase(found);
    return writeFavorites(favorites);
}

} // namespace openedmkick::suite
