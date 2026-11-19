#pragma once

#include "PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace openedmkick::suite {

class DragExportButton : public juce::TextButton {
public:
    explicit DragExportButton(const juce::String& text = "Drag hit");
    std::function<juce::File()> onRender;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    bool dragged_ = false;
};

struct SuiteActions {
    std::function<void()> undo;
    std::function<void()> redo;
    std::function<void()> selectA;
    std::function<void()> selectB;
    std::function<void()> copyAtoB;
    std::function<void()> copyBtoA;
    std::function<bool()> canUndo;
    std::function<bool()> canRedo;
    std::function<int()> activeSlot;
    std::function<juce::File()> renderHit;
    std::function<void()> togglePresets;
};

class SuiteBar : public juce::Component {
public:
    SuiteBar();
    void bind(SuiteActions actions);
    void refresh();
    void resized() override;

private:
    SuiteActions actions_;
    juce::TextButton undo_{ "Undo" };
    juce::TextButton redo_{ "Redo" };
    juce::TextButton slotA_{ "A" };
    juce::TextButton slotB_{ "B" };
    juce::TextButton copyAtoB_{ "A to B" };
    juce::TextButton copyBtoA_{ "B to A" };
    DragExportButton drag_{ "Drag hit" };
    juce::TextButton presets_{ "Presets" };
};

class PresetBrowser : public juce::Component, private juce::ListBoxModel {
public:
    explicit PresetBrowser(std::vector<juce::String> factoryNames);
    void setDirectory(juce::File directory);
    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<juce::MemoryBlock()> captureState;
    std::function<void(int factoryIndex)> loadFactory;
    std::function<void(const juce::MemoryBlock&)> loadUser;

    [[nodiscard]] int rowCount() const { return static_cast<int>(entries_.size()); }
    [[nodiscard]] juce::String rowName(int row) const;

private:
    int getNumRows() override { return rowCount(); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void refresh();
    void loadRow(int row);
    void setStatus(const juce::String& text);

    PresetLibrary library_;
    std::vector<PresetEntry> entries_;
    juce::Label title_{ {}, "Presets" };
    juce::TextEditor search_;
    juce::TextEditor name_;
    juce::ListBox list_{ "presetList", this };
    juce::TextButton save_{ "Save" };
    juce::TextButton rename_{ "Rename" };
    juce::TextButton remove_{ "Delete" };
    juce::Label status_{ {}, {} };
    int selected_ = -1;
};

} // namespace openedmkick::suite
