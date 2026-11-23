#include "SuiteBar.h"

namespace openedmkick::suite {
namespace {
const juce::Colour kInk{ 0xff0d1317 };
const juce::Colour kSteel{ 0xff152026 };
const juce::Colour kEdge{ 0xff283842 };
const juce::Colour kText{ 0xffe6f0f5 };
const juce::Colour kTextDim{ 0xff869ca8 };
const juce::Colour kViolet{ 0xff5ce1ff };
const juce::Colour kCoral{ 0xffffbe3b };
const juce::Colour kMint{ 0xff48f0c8 };
constexpr int kConfirmMs = 3000;

void styleTool(juce::TextButton& button, const juce::String& id, const juce::String& tip) {
    button.setComponentID(id);
    button.setTitle(button.getButtonText());
    button.setTooltip(tip);
    button.setColour(juce::TextButton::buttonOnColourId, kViolet);
    button.setMouseClickGrabsKeyboardFocus(false);
}
} // namespace

DragExportButton::DragExportButton(const juce::String& text) : juce::TextButton(text) {}

void DragExportButton::mouseDown(const juce::MouseEvent& event) {
    dragged_ = false;
    juce::TextButton::mouseDown(event);
}

void DragExportButton::mouseDrag(const juce::MouseEvent& event) {
    if (dragged_ || event.getDistanceFromDragStart() < 8 || !onRender) return;
    dragged_ = true;
    const auto file = onRender();
    if (!file.existsAsFile()) return;
    juce::StringArray files;
    files.add(file.getFullPathName());
    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this);
}

void DragExportButton::mouseUp(const juce::MouseEvent& event) {
    if (dragged_) {
        setState(buttonNormal);
        return;
    }
    juce::TextButton::mouseUp(event);
}

SuiteBar::SuiteBar() {
    styleTool(undo_, "undo", "Undo the last control change. Cmd or Ctrl Z.");
    styleTool(redo_, "redo", "Redo the last undone change. Shift Cmd or Ctrl Z.");
    styleTool(slotA_, "abA", "Hear compare slot A.");
    styleTool(slotB_, "abB", "Hear compare slot B.");
    styleTool(copyAtoB_, "copyAtoB", "Copy slot A into slot B.");
    styleTool(copyBtoA_, "copyBtoA", "Copy slot B into slot A.");
    styleTool(drag_, "dragHit", "Drag the current hit as a WAV onto a track or the desktop.");
    styleTool(presets_, "presets", "Search factory and saved presets. Star, save, rename, or delete your own.");
    slotA_.setTitle("Slot A");
    slotB_.setTitle("Slot B");
    copyAtoB_.setTitle("Copy A to B");
    copyBtoA_.setTitle("Copy B to A");
    drag_.setColour(juce::TextButton::buttonColourId, kMint);
    drag_.setColour(juce::TextButton::textColourOffId, kInk);
    slotA_.setClickingTogglesState(false);
    slotB_.setClickingTogglesState(false);
    for (auto* button : { &undo_, &redo_, &slotA_, &slotB_, &copyAtoB_, &copyBtoA_, static_cast<juce::TextButton*>(&drag_), &presets_ })
        addAndMakeVisible(button);
    undo_.onClick = [this] { if (actions_.undo) actions_.undo(); refresh(); };
    redo_.onClick = [this] { if (actions_.redo) actions_.redo(); refresh(); };
    slotA_.onClick = [this] { if (actions_.selectA) actions_.selectA(); refresh(); };
    slotB_.onClick = [this] { if (actions_.selectB) actions_.selectB(); refresh(); };
    copyAtoB_.onClick = [this] { if (actions_.copyAtoB) actions_.copyAtoB(); refresh(); };
    copyBtoA_.onClick = [this] { if (actions_.copyBtoA) actions_.copyBtoA(); refresh(); };
    presets_.onClick = [this] { if (actions_.togglePresets) actions_.togglePresets(); };
    refresh();
}

void SuiteBar::bind(SuiteActions actions) {
    actions_ = std::move(actions);
    drag_.onRender = actions_.renderHit;
    refresh();
}

void SuiteBar::refresh() {
    undo_.setEnabled(!actions_.canUndo || actions_.canUndo());
    redo_.setEnabled(!actions_.canRedo || actions_.canRedo());
    const int slot = actions_.activeSlot ? actions_.activeSlot() : 0;
    slotA_.setToggleState(slot == 0, juce::dontSendNotification);
    slotB_.setToggleState(slot == 1, juce::dontSendNotification);
}

void SuiteBar::resized() {
    auto row = getLocalBounds();
    const auto place = [&](juce::Component& component, int width) {
        component.setBounds(row.removeFromLeft(std::min(width, std::max(0, row.getWidth()))));
        row.removeFromLeft(4);
    };
    place(undo_, 64);
    place(redo_, 64);
    place(slotA_, 40);
    place(slotB_, 40);
    place(copyAtoB_, 76);
    place(copyBtoA_, 76);
    place(drag_, 100);
    place(presets_, 92);
}

PresetBrowser::PresetBrowser(std::vector<juce::String> factoryNames)
    : library_("openKicker", std::move(factoryNames)) {
    setComponentID("presetBrowser");
    title_.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, kText);
    search_.setComponentID("presetSearch");
    search_.setTextToShowWhenEmpty("Search presets", kTextDim);
    search_.setColour(juce::TextEditor::backgroundColourId, kInk);
    search_.setColour(juce::TextEditor::textColourId, kText);
    search_.onTextChange = [this] { refresh(); };
    search_.onReturnKey = [this] {
        if (rowCount() > 0) {
            list_.selectRow(0);
            loadRow(0);
        }
    };
    search_.onEscapeKey = [this] { setVisible(false); };
    name_.setComponentID("presetName");
    name_.setTextToShowWhenEmpty("Preset name", kTextDim);
    name_.setColour(juce::TextEditor::backgroundColourId, kInk);
    name_.setColour(juce::TextEditor::textColourId, kText);
    name_.onTextChange = [this] { disarm(); updateButtons(); };
    name_.onReturnKey = [this] { save_.triggerClick(); };
    name_.onEscapeKey = [this] { setVisible(false); };
    list_.setComponentID("presetList");
    list_.setRowHeight(28);
    list_.setColour(juce::ListBox::backgroundColourId, kInk);
    list_.setColour(juce::ListBox::outlineColourId, kEdge);
    for (auto* button : { &save_, &rename_, &remove_ }) {
        button->setTitle(button->getButtonText());
        button->setColour(juce::TextButton::buttonOnColourId, kViolet);
        addAndMakeVisible(button);
    }
    save_.setComponentID("presetSave");
    rename_.setComponentID("presetRename");
    remove_.setComponentID("presetDelete");
    save_.setTooltip("Save the current sound as a user preset.");
    rename_.setTooltip("Rename the selected user preset.");
    remove_.setTooltip("Delete the selected user preset. Factory presets stay read-only.");
    status_.setColour(juce::Label::textColourId, kTextDim);
    status_.setComponentID("presetStatus");
    status_.setJustificationType(juce::Justification::centredLeft);
    for (auto* child : { static_cast<juce::Component*>(&title_), static_cast<juce::Component*>(&search_),
                         static_cast<juce::Component*>(&name_), static_cast<juce::Component*>(&list_),
                         static_cast<juce::Component*>(&status_) })
        addAndMakeVisible(child);
    save_.onClick = [this] {
        if (!captureState) return;
        const auto presetName = name_.getText().trim();
        if (presetName.isEmpty()) {
            setStatus("Type a name, then save.");
            return;
        }
        if (library_.isFactory(presetName)) {
            setStatus("Factory presets stay read-only.");
            return;
        }
        if (library_.exists(presetName) && armed_ != &save_) {
            disarm();
            armed_ = &save_;
            save_.setButtonText("Replace?");
            save_.setTitle("Replace?");
            startTimer(kConfirmMs);
            return;
        }
        disarm();
        const bool saved = library_.save(presetName, captureState());
        setStatus(saved ? "Saved " + presetName : "Could not save that preset.");
        if (saved && onSave) onSave(presetName);
        refresh();
    };
    rename_.onClick = [this] {
        if (selected_ < 0 || selected_ >= rowCount()) return;
        const auto& entry = entries_[static_cast<size_t>(selected_)];
        if (entry.factory) {
            setStatus("Factory presets stay read-only.");
            return;
        }
        const auto next = name_.getText().trim();
        setStatus(library_.rename(entry.name, next) ? "Renamed to " + next : "Could not rename that preset.");
        refresh();
    };
    remove_.onClick = [this] {
        if (selected_ < 0 || selected_ >= rowCount()) return;
        const auto& entry = entries_[static_cast<size_t>(selected_)];
        if (entry.factory) {
            setStatus("Factory presets stay read-only.");
            return;
        }
        if (armed_ != &remove_) {
            disarm();
            armed_ = &remove_;
            remove_.setButtonText("Delete?");
            remove_.setTitle("Delete?");
            startTimer(kConfirmMs);
            return;
        }
        disarm();
        const auto presetName = entry.name;
        setStatus(library_.remove(presetName) ? "Deleted " + presetName : "Could not delete that preset.");
        selected_ = -1;
        refresh();
    };
    refresh();
}

void PresetBrowser::setDirectory(juce::File directory) {
    library_.setDirectory(std::move(directory));
    refresh();
}

void PresetBrowser::paint(juce::Graphics& g) {
    g.setColour(kSteel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(kViolet.withAlpha(0.7f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.2f);
}

void PresetBrowser::resized() {
    auto area = getLocalBounds().reduced(12);
    title_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    search_.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);
    name_.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);
    auto buttons = area.removeFromBottom(40);
    status_.setBounds(area.removeFromBottom(22));
    area.removeFromBottom(4);
    list_.setBounds(area);
    const int width = (buttons.getWidth() - 8) / 3;
    save_.setBounds(buttons.removeFromLeft(width));
    buttons.removeFromLeft(4);
    rename_.setBounds(buttons.removeFromLeft(width));
    buttons.removeFromLeft(4);
    remove_.setBounds(buttons);
}

juce::String PresetBrowser::rowName(int row) const {
    if (row < 0 || row >= rowCount()) return {};
    return entries_[static_cast<size_t>(row)].name;
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
    if (row < 0 || row >= rowCount()) return;
    const auto& entry = entries_[static_cast<size_t>(row)];
    if (selected) g.fillAll(kViolet.withAlpha(0.18f));
    auto area = juce::Rectangle<int>(width, height).reduced(8, 0);
    auto star = area.removeFromRight(28);
    g.setColour(entry.favorite ? kCoral : kTextDim);
    g.setFont(juce::FontOptions(16.0f));
    g.drawText(entry.favorite ? juce::String::charToString(0x2605) : juce::String::charToString(0x2606),
               star, juce::Justification::centred);
    g.setColour(kTextDim);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(entry.factory ? "Factory" : "User", area.removeFromLeft(entry.factory ? 62 : 42),
               juce::Justification::centredLeft);
    g.setColour(kText);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(entry.name, area, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& event) {
    if (row < 0 || row >= rowCount()) return;
    selected_ = row;
    name_.setText(entries_[static_cast<size_t>(row)].name, juce::dontSendNotification);
    updateButtons();
    const int rowWidth = event.eventComponent != nullptr ? event.eventComponent->getWidth() : list_.getWidth();
    if (event.x >= rowWidth - 36) {
        const auto& entry = entries_[static_cast<size_t>(row)];
        library_.setFavorite(entry.name, !entry.favorite);
        refresh();
        list_.selectRow(row);
        return;
    }
    loadRow(row);
}

void PresetBrowser::selectedRowsChanged(int row) {
    selected_ = row;
    disarm();
    updateButtons();
}

void PresetBrowser::visibilityChanged() {
    disarm();
}

bool PresetBrowser::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        setVisible(false);
        return true;
    }
    return false;
}

void PresetBrowser::updateButtons() {
    const bool hasName = name_.getText().trim().isNotEmpty();
    const bool userSelected = selected_ >= 0 && selected_ < rowCount()
        && !entries_[static_cast<size_t>(selected_)].factory;
    save_.setEnabled(hasName);
    rename_.setEnabled(hasName && userSelected);
    remove_.setEnabled(userSelected);
}

void PresetBrowser::disarm() {
    stopTimer();
    armed_ = nullptr;
    save_.setButtonText("Save");
    save_.setTitle("Save");
    remove_.setButtonText("Delete");
    remove_.setTitle("Delete");
}

void PresetBrowser::loadRow(int row) {
    disarm();
    const auto& entry = entries_[static_cast<size_t>(row)];
    if (entry.factory) {
        if (loadFactory) loadFactory(entry.factoryIndex);
        setStatus(entry.name);
        return;
    }
    juce::MemoryBlock state;
    if (!library_.load(entry.name, state) || !loadUser) {
        setStatus("Could not load " + entry.name);
        return;
    }
    loadUser(state, entry.name);
    setStatus(entry.name);
}

void PresetBrowser::step(const juce::String& current, int direction) {
    disarm();
    const int count = rowCount();
    if (count == 0) return;
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const PresetEntry& entry) { return entry.name == current; });
    const int notListed = direction > 0 ? count - 1 : 0;
    const int position = found == entries_.end() ? notListed : static_cast<int>(found - entries_.begin());
    const int row = (position + direction + count) % count;
    list_.selectRow(row);
    loadRow(row);
}

void PresetBrowser::refresh() {
    disarm();
    const int keep = selected_;
    entries_ = library_.list(search_.getText().trim());
    list_.updateContent();
    if (keep >= 0 && keep < rowCount()) list_.selectRow(keep);
    updateButtons();
    repaint();
}

void PresetBrowser::setStatus(const juce::String& text) {
    status_.setText(text, juce::dontSendNotification);
}

} // namespace openedmkick::suite
