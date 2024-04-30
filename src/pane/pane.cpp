//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "pane.hpp"


void Pane::RegisterAction(const std::string& action_name, FuncAction action) {
  actions[action_name] = action;
}


void Pane::RegisterBinding(const std::string& mode, const std::string& key_combination, const std::string& action_name) {
  std::vector<event_t> events;

  if (!ParseKeyBindingString(events, key_combination.c_str())) {
    // TODO: Report error to user.
    return;
  }

  auto it = actions.find(action_name);
  if (it == actions.end()) {
    // TODO: Error(""); // Report to user.
    return;
  }

  keytree.RegisterBinding(mode, events, it->second);
  events.clear();
}


void Pane::SetMode(const std::string& mode) {
  keytree.SetMode(mode);
}


bool Pane::TryEvent(const Event& event) {
  bool more = false;

  FuncAction action = keytree.ConsumeEvent(EncodeKeyEvent(event.key), &more);
  // if (action && more) { } // TODO: Timeout and perform action.

  if (action) {
    action();
    keytree.ResetCursor();
    return true;
  }

  // Don't do anything, just wait for the next keystroke and perform on it.
  if (more) return true;

  // Sequence is not registered, reset and listen from start. We'll be sending
  // true and treat this event to reset the cursor.
  if (!keytree.IsCursorRoot()) {
    keytree.ResetCursor();
    return true;
  }

  return false;
}


RootPane::RootPane() {

  // FIXME: Cleanup this mess.
  RegisterAction("toggle_file_finder", [&](){
    if (popup_pane == nullptr) {
      popup_pane = std::make_unique<FindPane>(std::make_unique<FilesFinder>());
      focused = popup_pane.get();
    } else if (focused && focused == popup_pane.get()) {
      if (child_panes.size()) focused = child_panes[0].get();
      else focused = nullptr;
    } else {
      focused = popup_pane.get();
    }
    return true;
  });
  RegisterAction("close_popup", [&](){
    if (focused && focused == popup_pane.get()) {
      focused = nullptr;
      return true;
    }
    return false;
  });

  // FIXME: This is temp.
  RegisterBinding("*", "<C-o>", "toggle_file_finder");
  RegisterBinding("*", "<esc>", "close_popup");
  SetMode("*");
}


bool RootPane::HandleEvent(const Event& event) {
  if (TryEvent(event)) return true;
  if (focused != nullptr) {
    return focused->HandleEvent(event);
  }
  return false;
}


void RootPane::Update(){
  if (focused && focused == popup_pane.get()) {
    popup_pane->Update();
  }
  for (auto& it : child_panes) {
    it->Update();
  }
}


void RootPane::Draw(FrameBuffer buff, Position pos, Size area) {

  // uint8_t color_bg = 0x272829;
  Color color_bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg=0, .bg=0xffffff, .attrib=0}).bg;

  // Clean the buffer.
  for (int i    = 0; i < buff.width*buff.height; i++) {
    Cell& cell  = buff.cells[i];
    cell.ch     = ' ';
    cell.fg     = 0;
    cell.bg     = color_bg;
    cell.attrib = 0;
  }

  // FIXME: The panes should be In a tree to draw the splits.
  for (auto& it : child_panes) {
    it->Draw(buff, pos, area);
  }

  // Draw popup, if it's focused.
  if (focused && focused == popup_pane.get()) {
    popup_pane->Draw(buff, pos, area);
  }


  // Draw a spinning indicator yell it's re-drawn.
  // ⡿ ⣟ ⣯ ⣷ ⣾ ⣽ ⣻ ⢿
  static int curr = 0;
  int icons[] = { 0x287f, 0x28df, 0x28ef, 0x28f7, 0x28fe, 0x28fd, 0x28fb, 0x28bf };
  int icon_count = sizeof icons / sizeof *icons;
  if (curr >= icon_count) curr = 0;
  SET_CELL(buff, 0, buff.height-1, icons[curr++], 0xffffff, 0, 0); // FIXME:

}

