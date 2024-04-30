//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "pane.hpp"

// FIXME(grep) The finder might not have the requirnment installed, handle it.

FindPane::FindPane(std::unique_ptr<Finder> finder_): finder(std::move(finder_)) {

  // Initialize the finder.
  finder->Initialize();
  finder->RegisterItemsChangeListener([&](){
    this->OnFilteredItemsChanged();
  });

  RegisterAction("cursor_right", [&] { if (cursor_index < input_text.size()) cursor_index++; return true; });
  RegisterAction("cursor_left", [&] { if (cursor_index > 0) cursor_index--; return true; });
  RegisterAction("cursor_home", [&] { cursor_index = 0; return true; });
  RegisterAction("cursor_end", [&] { cursor_index = input_text.size(); return true; });
  RegisterAction("backspace", [&] {
    if (cursor_index > 0) {
      input_text.erase(cursor_index-1, 1);
      cursor_index--;

      // If the input changed, the selection become invalid and we'll set to -1.
      finder->InputChanged(this->input_text);
      view_start_index = 0;
      selected_index = -1;
    }
    return true;
  });
  RegisterAction("cycle_selection", [&] { this->CycleItems(); return true; });
  RegisterAction("cycle_selection_reversed", [&] { this->CycleItemsReversed(); return true; });

  RegisterBinding("*", "<right>", "cursor_right");
  RegisterBinding("*", "<left>",  "cursor_left");
  RegisterBinding("*", "<home>",  "cursor_home");
  RegisterBinding("*", "<end>",  "cursor_home");
  RegisterBinding("*", "<backspace>",  "backspace");
  RegisterBinding("*", "<C-n>",  "cycle_selection");
  RegisterBinding("*", "<C-p>",  "cycle_selection_reversed");
  RegisterBinding("*", "<tab>",  "cycle_selection");
  RegisterBinding("*", "<S-tab>", "cycle_selection_reversed");
  SetMode("*");
}


bool FindPane::HandleEvent(const Event& event) {
  if (TryEvent(event)) return true;
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
      view_start_index = selected_index;
    } else if (selected_index >= view_start_index + selection_height) {
      view_start_index = selected_index - (selection_height - 1);
    }
  }
}


void FindPane::OnFilteredItemsChanged() {
  int items_count = finder->GetFilteredItemsCount();
  if (items_count == 0) {
    this->selected_index = -1;
  } else {
    if (this->selected_index < 0) {
      this->selected_index = 0;
      this->view_start_index = 0;
    } else if (this->selected_index >=  items_count) {
      this->selected_index = items_count - 1;
    }
  }
}


void FindPane::CycleItems() {
  int items_count = input_text.empty()
    ? finder->GetTotalItemsCount()
    : finder->GetFilteredItemsCount();

  if (items_count  == 0) selected_index = -1;
  else {
    selected_index = (selected_index + 1) % items_count;
  }
}


void FindPane::CycleItemsReversed() {
  int items_count = input_text.empty()
    ? finder->GetTotalItemsCount()
    : finder->GetFilteredItemsCount();

  if (items_count == 0) selected_index = -1;
  else {
    selected_index--;
    if (selected_index < 0) {
      selected_index = items_count - 1;
    }
  }
}


void FindPane::Draw(FrameBuffer buff, Position pos_windows, Size area) {

  // FIXME: Move this mess. ----------------------------------------------------
  Color fg = Global::GetCurrentTheme()->GetStyleOr("ui.text", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).fg;
  Color bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).bg;
  Color color_cursor_fg     = Global::GetCurrentTheme()->GetStyleOr("ur.cursor.primary", {.fg = 0x0000ff, .bg = 0xffffff, .attrib = 0}).fg;
  Color color_cursor_bg     = Global::GetCurrentTheme()->GetStyleOr("ui.cursor.primary", {.fg = 0x000000, .bg = 0xffffff, .attrib = 0}).bg;
  // --------------------------------------------------------------------------

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
  DrawRectangleLine(buff, x, y, w, h, fg, bg, true);

  // Draw the input area.
  for (int i = 0; i < input_text.size(); i++) {
    Color fg_ = (i == cursor_index) ? color_cursor_fg : fg;
    Color bg_ = (i == cursor_index) ? color_cursor_bg : bg;
    SET_CELL(buff, x+2+i, y+1, input_text[i], fg_, bg_, 0);
  }
  if (cursor_index == input_text.size()) {
    SET_CELL(buff, x+2 + cursor_index, y+1, ' ', color_cursor_fg, color_cursor_bg, 0);
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
    DrawTextLine(buff, filter_ratio.c_str(), x_, y_, filter_ratio.size(), fg, bg, 0, false);
  }

  // Draw the split line.
  DrawHorizontalLine(buff, x+1, y+2, w-2, fg, bg);

  // The filter/result draw pos, area.
  int x_offset = x+2; // +2: left_bar, spacing.
  int y_offset = y+3; // +3: top_bar, input, split.
  int w_effect = w-4; // -4: left_bar, right_bar, 2*spacing.

  // TODO: If the line is too long, draw the tail only.
  //
  // TODO: handle if the "returned" list is nullptr this may not be assered since
  // the user plugins can also return this and we don't want to crash on it.
  if (input_text.empty()) {
    const std::vector<std::string>* total = nullptr;
    std::unique_lock<std::mutex> lock_t = finder->GetTotalItems(&total);
    int h_effect = MIN(h - 4, total_count); // -4: top_bar, input, split, bottom_bar.
    DrawItems(buff, x_offset, y_offset, w_effect, h_effect, total);
    selection_height = h_effect; // Cache the selection height.
  } else {
    const std::vector<std::string>* filtered = nullptr;
    std::unique_lock<std::mutex> lock_f = finder->GetFilteredItems(&filtered);
    int h_effect = MIN(h - 4, filtered_count); // -4: top_bar, input, split, bottom_bar.
    DrawItems(buff, x_offset, y_offset, w_effect, h_effect, filtered);
    selection_height = h_effect; // Cache the selection height.
  }

}


void FindPane::DrawItems(FrameBuffer buff, int x, int y, int w, int h, const std::vector<std::string>* items) {
  // FIXME: Move this mess. ----------------------------------------------------
  Color fg = Global::GetCurrentTheme()->GetStyleOr("ui.text", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).fg;
  Color bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).bg;
  // --------------------------------------------------------------------------
  for (int i = 0; i < h; i++) {
    const std::string& item = (*items)[view_start_index + i];
    Color bg_ = (view_start_index + i == selected_index) ? 0xff0000 : bg;
    DrawTextLine(buff, item.c_str(), x, y+i, w, fg, bg_, 0, true);
  }
}

