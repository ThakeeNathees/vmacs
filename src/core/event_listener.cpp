//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core.hpp"


void EventListener::RegisterAction(const std::string& action_name, FuncAction action) {
  actions[action_name] = action;
}


void EventListener::RegisterBinding(const std::string& mode, const std::string& key_combination, const std::string& action_name) {
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


void EventListener::SetMode(const std::string& mode) {
  keytree.SetMode(mode);
}


bool EventListener::TryEvent(const Event& event) {
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
