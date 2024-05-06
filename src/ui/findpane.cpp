//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"

KeyTree FindPane::keytree;

// FIXME(grep) The finder might not have the requirnment installed, handle it.

FindPane::FindPane(std::unique_ptr<Finder> finder_)
  : Pane(&keytree), finder(std::move(finder_)) {

  // Initialize the finder.
  finder->Initialize();
  finder->RegisterItemsChangeListener([&](){
    this->OnFilteredItemsChanged();
  });

  // FIXME: Initial mode.
  SetMode("*");
}


bool FindPane::_HandleEvent(const Event& event) {
  if (EventHandler::HandleEvent(event)) return true;
  if (event.type == Event::Type::KEY && event.key.unicode != 0) {
    char c = (char) event.key.unicode;
    input_text.insert(cursor_index, std::string(1, event.key.unicode));
    cursor_index++;
    finder->InputChanged(input_text);
    return true;
  }
  return false;
}


void FindPane::Update() {
  EnsureSelectionOnView();
}


void FindPane::EnsureSelectionOnView() {
  if (input_text.empty() && selected_index < 0) {
    int total_count = finder->GetTotalItemsCount();
    selected_index = (total_count > 0) ? 0 : -1;
    view_start_index = 0;
  } else {
    if (selected_index < view_start_index) {
      view_start_index = MAX(0, selected_index); // selected_index can be -1.
    } else if (selected_index >= view_start_index + selection_height) {
      view_start_index = selected_index - (selection_height - 1);
    }
  }
}


void FindPane::OnFilteredItemsChanged() {
  if (finder->GetFilteredItemsCount() == 0) this->selected_index = -1;
  else this->selected_index = 0;
  this->view_start_index = 0;
}


std::string FindPane::GetSelectedItem() {
  if (selected_index < 0) return "";
  if (input_text.size() == 0) {
    const std::vector<std::string>* total = nullptr;
    std::unique_lock<std::mutex> lock_t = finder->GetTotalItems(&total);
    if (selected_index >= total->size()) return "";
    return (*total)[selected_index];
  }

  // If we reached here, the selection is in filter list.
  const std::vector<std::string>* filtered = nullptr;
  std::unique_lock<std::mutex> lock_t = finder->GetFilteredItems(&filtered);
  if (selected_index >= filtered->size()) return "";
  return (*filtered)[selected_index];
}


void FindPane::_Draw(FrameBuffer buff, Position pos_windows, Size area) {

  // FIXME: Move this mess. ----------------------------------------------------
  const Theme* theme = Editor::GetCurrentTheme();
  Style style_text   = theme->GetStyle("ui.text");
  Style style_cursor = theme->GetStyle("ui.cursor.primary");
  Style style_bg     = theme->GetStyle("ui.background");
  Style style_border = theme->GetStyle("ui.background.separator");
  // --------------------------------------------------------------------------

  // The default fg,bg,attrib for drawing bellow.
  const Style style = style_bg.Apply(style_text);

  // Dimentions.
  const int percent = 90;
  const int w = (area.width * percent) / 100;
  const int h = (area.height * percent) / 100;
  const int x = (area.width - w) / 2;
  const int y = (area.height - h) / 2;

  // We need a witdh of at least 4 cells (two borders and two for spacing).
  if (w-4 <= 0) return;

  // TODO: Check if the input area is enough and h-scroll if reached the end.
  // And drawing the filter_ratio shouldn't block the input if the input is reached
  // the end.

  // Draw the border.
  DrawRectangleLine(buff, x, y, w, h, style_border, true);

  // Draw the input area.
  for (int i = 0; i < input_text.size(); i++) {
    SET_CELL(buff, x+2+i, y+1, input_text[i],
       (i == cursor_index) ? style.Apply(style_cursor) : style);
  }
  if (cursor_index == input_text.size()) {
    auto s = style.Apply(style_cursor);
    SET_CELL(buff, x+2 + cursor_index, y+1, ' ', s);
  }

  int total_count = finder->GetTotalItemsCount();
  int filtered_count = finder->GetFilteredItemsCount();

  {
    // Draw the filter / results ratio.
    std::string filter_ratio =
      std::to_string(input_text.empty() ? total_count : filtered_count) +
      " / " + std::to_string(total_count);
    int x_ = x + w - 2 - filter_ratio.size(); // -2: right_bar, spacing.
    int y_ = y + 1;
    DrawTextLine(buff, filter_ratio.c_str(), x_, y_, filter_ratio.size(), style, false);
  }

  // Draw the split line.
  DrawHorizontalLine(buff, x+2, y+2, w-4, style_border);

  // Draw the filtered items.

  // The filter/result draw pos, area.
  int x_offset = x+2; // +2: left_bar, spacing.
  int y_offset = y+3; // +3: top_bar, input, split.
  int w_effect = w-4; // -4: left_bar, right_bar, 2*spacing.

  // Cache the selection height.
  selection_height = h - 4; // -4: top_bar, input, split, bottom_bar.

  // TODO: If the line is too long, draw the tail only.
  //
  // TODO: handle if the "returned" list is nullptr this may not be assered since
  // the user plugins can also return this and we don't want to crash on it.
  if (input_text.empty()) {
    const std::vector<std::string>* total = nullptr;
    std::unique_lock<std::mutex> lock_t = finder->GetTotalItems(&total);
    int h_effect = MIN(selection_height, total_count);
    DrawItems(buff, x_offset, y_offset, w_effect, h_effect, total);
  } else {
    const std::vector<std::string>* filtered = nullptr;
    std::unique_lock<std::mutex> lock_f = finder->GetFilteredItems(&filtered);
    int h_effect = MIN(selection_height, filtered_count); // -4: top_bar, input, split, bottom_bar.
    DrawItems(buff, x_offset, y_offset, w_effect, h_effect, filtered);
  }

}


void FindPane::DrawItems(FrameBuffer buff, int x, int y, int w, int h, const std::vector<std::string>* items) {
  // FIXME: Move this mess. ----------------------------------------------------
  const Theme* theme = Editor::GetCurrentTheme();
  Style style_text = theme->GetStyle("ui.text");
  Style style_bg   = theme->GetStyle("ui.background");
  Style style_selected = theme->GetStyle("ui.menu.selected");
  // --------------------------------------------------------------------------
  Style style = style_bg.Apply(style_text);
  for (int i = 0; i < h; i++) {
    if  (view_start_index + i >= items->size()) return; // Out of bound.
    const std::string& item = (*items)[view_start_index + i];
    DrawTextLine(buff, item.c_str(), x, y+i, w, 
        (view_start_index + i == selected_index) ? style.Apply(style_selected) : style,
        true);
  }
}

// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------

bool FindPane::Action_CursorRight(FindPane* self) { if (self->cursor_index < self->input_text.size()) self->cursor_index++; return true; }
bool FindPane::Action_CursorLeft(FindPane* self) { if (self->cursor_index > 0) self->cursor_index--; return true; }
bool FindPane::Action_CursorHome(FindPane* self) { self->cursor_index = 0; return true; }
bool FindPane::Action_CursorEnd(FindPane* self) { self->cursor_index = self->input_text.size(); return true; }

bool FindPane::Action_CycleSelection(FindPane* self) {
  int items_count = self->input_text.empty()
    ? self->finder->GetTotalItemsCount()
    : self->finder->GetFilteredItemsCount();

  if (items_count  == 0) self->selected_index = -1;
  else {
    self->selected_index = (self->selected_index + 1) % items_count;
  }
  return true;
}

bool FindPane::Action_CycleSelectionReversed(FindPane* self) {
  int items_count = self->input_text.empty()
    ? self->finder->GetTotalItemsCount()
    : self->finder->GetFilteredItemsCount();

  if (items_count == 0) self->selected_index = -1;
  else {
    self->selected_index--;
    if (self->selected_index < 0) {
      self->selected_index = items_count - 1;
    }
  }
  return true;
}


bool FindPane::Action_Backspace(FindPane* self) {
  if (self->cursor_index > 0) {
    self->input_text.erase(self->cursor_index-1, 1);
    self->cursor_index--;
    // If the input changed, the selection become invalid and we'll set to -1.
    self->finder->InputChanged(self->input_text);
    self->view_start_index = 0;
    self->selected_index = -1;
  }
  return true;
}

// FIXME(mess): This is temproary.
bool FindPane::Action_AcceptSelection(FindPane* self) {
  Path path(self->GetSelectedItem());
  if (!path.Exists()) {
    // TODO: Error to editor.
  }
  Editor::Info(path.String());
  // e->ClosePopup(); // Will destroy this, can't.
  return true;
}
