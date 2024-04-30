//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "editor.hpp"


FindPane::FindPane() {
  {
    IPC::IpcOptions opt;
    opt.user_data      = this;
    opt.file           = "rg";
    opt.argv           = { "--files" };
    opt.timeout_sec    = 4;
    opt.sending_inputs = false;
    opt.stdout_cb      = FindPane::StdoutCallbackResults;
    ipc                = IPC::New(opt);
    ipc->Run();
  }
}


void FindPane::HandleEvent(const Event& event) {
  if (event.type == Event::Type::KEY) {
    switch (event.key.code) {

      // TODO: S-home, S-end implement.
      case Event::KEY_RIGHT: if (cursor_index < input_text.size()) cursor_index++; break;
      case Event::KEY_LEFT:  if (cursor_index > 0) cursor_index--; break;
      case Event::KEY_HOME:  cursor_index = 0; break;
      case Event::KEY_END:   cursor_index = input_text.size(); break;

      case Event::KEY_BACKSPACE:
        if (cursor_index > 0) {
          input_text.erase(cursor_index-1, 1);
          cursor_index--;
          TriggerFuzzyFilter();
        }
        break;

      default: {
        if (event.key.unicode != 0) {
          input_text.insert(cursor_index, std::string(1, event.key.unicode));
          cursor_index++;
          TriggerFuzzyFilter();
        }
      } break;

    }
  }
}


void FindPane::Update() {

}


// FIXME: This method is not final.
void FindPane::TriggerFuzzyFilter() {
  { // This scope is absolutely necessary here otherwise the bellow destrucor
    // of IPC will cause a deadlock.
    std::lock_guard<std::mutex> lock_filter(mutex_filters);
    filters.clear();
  }

  IPC::IpcOptions opt;
  opt.user_data      = this;
  opt.file           = "fzf";
  opt.argv           = { "--filter", input_text };
  opt.timeout_sec    = 2;
  opt.sending_inputs = true;
  opt.stdout_cb      = FindPane::StdoutCallbackFilter;

  // This will destroy the old ipc and starts a new one.
  ipc_filter         = IPC::New(opt);

  {
    std::lock_guard<std::mutex> lock_results(mutex_results);
    std::lock_guard<std::mutex> lock_filters(mutex_filters);
    for (auto& s : results) {
      ipc_filter->WriteToStdin(s + "\n");
    }
    ipc_filter->StopListening();
    ipc_filter->Run();
  }

}


void FindPane::StdoutCallbackResults(void* data, const char* buff, size_t length) {
  FindPane* self = (FindPane*) data;
  {
    std::lock_guard<std::mutex> lock(self->mutex_results);
    const char* end = buff;
    while (length--) {
      if (*end == '\n') {
        size_t len = (end-buff);
        if (len != 0) {
          self->results.push_back(std::string(buff, len));
        }
        buff = end+1;
      }
      end++;
    }
  }
}


void FindPane::StdoutCallbackFilter(void* data, const char* buff, size_t length) {

  FindPane* self = (FindPane*) data;
  {
    std::lock_guard<std::mutex> lock(self->mutex_filters);
    const char* end = buff;
    while (length--) {
      if (*end == '\n') {
        size_t len = (end-buff);
        if (len != 0) {
          self->filters.push_back(std::string(buff, len));
        }
        buff = end+1;
      }
      end++;
    }
  }
}


void FindPane::Draw(FrameBuffer buff, Position pos_windows, Size area) {

  // We need filters and results for the lifetime of this method.
  std::lock_guard<std::mutex> lock_r(mutex_results);
  std::lock_guard<std::mutex> lock_f(mutex_filters);

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

  // Draw the filter / results ratio.
  int filtered_size = input_text.empty() ? results.size() : filters.size();
  std::string filter_ratio =
    std::to_string(filtered_size) + " / " +
    std::to_string(results.size());
  int x_ = x + w - 2 - filter_ratio.size(); // -2: right_bar, spacing.
  int y_ = y + 1;
  DrawTextLine(buff, filter_ratio.c_str(), x_, y_, filter_ratio.size(), fg, bg, 0, false);

  // Draw the split line.
  DrawHorizontalLine(buff, x+1, y+2, w-2, fg, bg);

  // The filter/result draw pos, area.
  int x_offset = x+2; // +2: left_bar, spacing.
  int y_offset = y+3; // +3: top_bar, input, split.
  int w_effect = w-4; // -4: left_bar, right_bar, 2*spacing.

  // Nothing in the input then show all the results.
  if (input_text.empty()) {
    int h_effect = MIN(h - 4, results.size()); // -4: top_bar, input, split, bottom_bar.
    DrawResults(buff, x_offset, y_offset, w_effect, h_effect);
  } else {
    int h_effect = MIN(h - 4, filters.size()); // -4: top_bar, input, split, bottom_bar.
    DrawFilter(buff, x_offset, y_offset, w_effect, h_effect);
  }
}


void FindPane::DrawResults(FrameBuffer buff, int x, int y, int w, int h) {
  // FIXME: Move this mess. ----------------------------------------------------
  Color fg = Global::GetCurrentTheme()->GetStyleOr("ui.text", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).fg;
  Color bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).bg;
  // --------------------------------------------------------------------------
  for (int i = 0; i < h; i++) {
    const std::string& result = results[i];
    DrawTextLine(buff, result.c_str(), x, y+i, w, fg, bg, 0, false);
  }
}


void FindPane::DrawFilter(FrameBuffer buff, int x, int y, int w, int h) {
  // FIXME: Move this mess. ----------------------------------------------------
  Color fg = Global::GetCurrentTheme()->GetStyleOr("ui.text", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).fg;
  Color bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg = 0xff0000, .bg = 0xff0000, .attrib = 0}).bg;
  // --------------------------------------------------------------------------
  for (int i = 0; i < h; i++) {
    const std::string& filter = filters[i];
    DrawTextLine(buff, filter.c_str(), x, y+i, w, fg, bg, 0, false);
  }
}
