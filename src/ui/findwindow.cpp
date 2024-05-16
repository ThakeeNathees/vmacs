//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"

// FIXME(grep) The finder might not have the requirnment installed, handle it.

FindWindow::FindWindow(std::unique_ptr<Finder> finder_) : finder(std::move(finder_)) {

  finder->RegisterItemsChangeListener([&](){ this->OnItemsChanged(); });
  finder->Initialize();
}


Window::Type FindWindow::GetType() const {
  return Type::FINDER;
}


bool FindWindow::_HandleEvent(const Event& event) {
  if (event.type == Event::Type::KEY && event.key.unicode != 0) {
    char c = (char) event.key.unicode;
    finder->GetSearchText().insert(cursor_index, std::string(1, event.key.unicode));
    finder->InputChanged();
    cursor_index++;
    selected_index = 0;
    return true;
  }
  return false;
}


void FindWindow::_Update() {
  EnsureSelectionOnView();
}


void FindWindow::EnsureSelectionOnView() {
  if (selected_index < view_start_index) {
    view_start_index = MAX(0, selected_index); // selected_index can be -1.
  } else if (selected_index >= view_start_index + selection_height) {
    view_start_index = selected_index - (selection_height - 1);
  }
}


void FindWindow::OnItemsChanged() {
  int items_count = finder->GetItemsCount();
  if (selected_index >= items_count) {
    // If items count is 0, selected index will be -1 (no-selection).
    selected_index = items_count - 1;
    if (view_start_index >= items_count) {
      view_start_index = MAX(0, selected_index);
    }
  } else if (selected_index < 0 && items_count > 0) {
    selected_index = 0;
    view_start_index = 0;
  }

  Editor::ReDraw();
}


std::string FindWindow::GetSelectedItem() {
  if (selected_index < 0) return "";
  const std::vector<std::string>* filtered = nullptr;
  std::unique_lock<std::mutex> lock = finder->GetItems(&filtered);
  if (selected_index >= filtered->size()) return "";
  return (*filtered)[selected_index];
}


void FindWindow::_Draw(FrameBuffer& buff, Position pos, Area area) {

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  const int x = pos.x;
  const int y = pos.y;
  const int w = area.width;
  const int h = area.height;

  // We need a witdh of at least 4 cells (two borders and two for spacing).
  if (area.width-4 <= 0) return;

  // TODO: Check if the input area is enough and h-scroll if reached the end.
  // And drawing the filter_ratio shouldn't block the input if the input is reached
  // the end.

  // Draw the border.
  DrawRectangleLine(buff, pos, area, theme.lines, icons, true);

  // Draw the input area.
  const std::string& input_text = finder->GetSearchText();
  for (int i = 0; i < input_text.size(); i++) {
    Style style = theme.style;
    if (i == cursor_index) style.ApplyInplace(theme.cursor);
    SET_CELL(buff, x+2+i, y+1, input_text[i], style);
  }
  if (cursor_index == input_text.size()) {
    auto style = theme.style.Apply(theme.cursor);
    SET_CELL(buff, x+2 + cursor_index, y+1, ' ', style);
  }

  int total_count = finder->GetItemsCountTotal();
  int items_count = finder->GetItemsCount();

  {
    // Draw the filter / results ratio.
    std::string filter_ratio;
    if (items_count > total_count || items_count == 0) {
      filter_ratio = std::to_string(items_count);
    } else {
      filter_ratio = std::to_string(items_count) + " / " + std::to_string(total_count);
    }
    int x_ = x + w - 2 - filter_ratio.size(); // -2: right_bar, spacing.
    int y_ = y + 1;
    DrawTextLine(
      buff,
      filter_ratio.c_str(),
      Position(x_, y_),
      filter_ratio.size(),
      theme.style,
      icons,
      false);
  }

  // Draw the split line.
  DrawHorizontalLine(buff, Position(x+2, y+2), w-4, theme.lines, icons);

  // Draw the filtered items.

  // The filter/result draw pos, area.
  int x_offset = x+2; // +2: left_bar, spacing.
  int y_offset = y+3; // +3: top_bar, input, split.
  int w_effect = w-4; // -4: left_bar, right_bar, 2*spacing.

  // Cache the selection height.
  selection_height = h - 4; // -4: top_bar, input, split, bottom_bar.

  // TODO: If the line is too long, draw the tail only.
  const std::vector<std::string>* filtered = nullptr;
  std::unique_lock<std::mutex> lock_f = finder->GetItems(&filtered);
  if (filtered == nullptr) return;
  int h_effect = MIN(selection_height, items_count); // -4: top_bar, input, split, bottom_bar.
  DrawItems(buff, x_offset, y_offset, w_effect, h_effect, filtered);
}


void FindWindow::DrawItems(FrameBuffer& buff, int x, int y, int w, int h, const std::vector<std::string>* items) {

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  for (int i = 0; i < h; i++) {
    if  (view_start_index + i >= items->size()) return; // Out of bound.
    const std::string& item = (*items)[view_start_index + i];
    Style style = theme.style;
    if (view_start_index + i == selected_index) style.ApplyInplace(theme.selection);
    DrawTextLine(buff, item.c_str(), Position(x, y+i), w, style, icons, true, true);
  }
}


// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------

bool FindWindow::Action_CursorRight(FindWindow* self) { ASSERT(self->finder != nullptr, OOPS); if (self->cursor_index < self->finder->GetSearchText().size()) self->cursor_index++; return true; }
bool FindWindow::Action_CursorLeft(FindWindow* self)  { if (self->cursor_index > 0) self->cursor_index--; return true; }
bool FindWindow::Action_CursorHome(FindWindow* self)  { self->cursor_index = 0; return true; }
bool FindWindow::Action_CursorEnd(FindWindow* self)   { ASSERT(self->finder != nullptr, OOPS); self->cursor_index = self->finder->GetSearchText().size(); return true; }

bool FindWindow::Action_CycleSelection(FindWindow* self) {
  int items_count = self->finder->GetItemsCount();
  if (items_count  == 0) {
    self->selected_index = -1;
  } else {
    self->selected_index = (self->selected_index + 1) % items_count;
  }
  return true;
}

bool FindWindow::Action_CycleSelectionReversed(FindWindow* self) {
  int items_count = self->finder->GetItemsCount();
  if (items_count == 0) self->selected_index = -1;
  else {
    self->selected_index--;
    if (self->selected_index < 0) {
      self->selected_index = items_count - 1;
    }
  }
  return true;
}


bool FindWindow::Action_Backspace(FindWindow* self) {
  if (self->cursor_index > 0) {
    self->finder->GetSearchText().erase(self->cursor_index-1, 1);
    self->finder->InputChanged();
    self->cursor_index--;
    self->view_start_index = 0;
    self->selected_index   = 0;
  }
  return true;
}


bool FindWindow::Action_AcceptSelection(FindWindow* self) {
  std::string selected = self->GetSelectedItem();
  if (selected.empty()) return false;

  if (self->finder->SelectItem(selected)) {
    self->SetShouldClose();
    return true;
  }

  return false;
}


bool FindWindow::Action_Close(FindWindow* self) {
  self->SetShouldClose();
  return true;
}
