//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core.hpp"


KeyTree::KeyTree() {
  root = std::make_unique<Node>();
}


void KeyTree::RegisterAction(const std::string& action_name, FuncAction action) {
  actions[action_name] = action;
}


void KeyTree::RegisterBinding(const std::string& key_combination, const std::string& action_name) {
  RegisterBinding("", key_combination, action_name);
}


void KeyTree::RegisterBinding(const std::string& mode, const std::string& key_combination, const std::string& action_name) {
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

  Node* curr = root.get();
  for (event_t event : events) {
    if (curr->children.find(event) == curr->children.end()) {
      curr->children[event] = std::make_unique<Node>();
    }
    curr = curr->children[event].get();
  }
  curr->actions[mode] = it->second;
}


// ----------------------------------------------------------------------------
// KeyTreeCursor.
// ----------------------------------------------------------------------------


KeyTreeCursor::KeyTreeCursor(const KeyTree* tree) : tree(tree) {
  ResetCursor();
}


void KeyTreeCursor::ResetCursor() {
  ASSERT(tree != nullptr, OOPS);
  node = tree->root.get();
  recorded_events.clear();
}


bool KeyTreeCursor::IsCursorRoot() const {
  ASSERT(tree != nullptr, OOPS);
  return node == tree->root.get();
}


void KeyTreeCursor::SetMode(const std::string& mode) {
  this->mode = mode;
  ResetCursor();
}


FuncAction KeyTreeCursor::ConsumeEvent(event_t event, bool* more) {
  ASSERT(tree != nullptr, OOPS);

  FuncAction action = nullptr;
  auto it_node = node->children.find(event);
  if (it_node == node->children.end()) {
    if (more) *more = false;
    return nullptr;
  }

  node = it_node->second.get();
  ASSERT(node != nullptr, OOPS);

  recorded_events.push_back(event);
  auto it_action = node->actions.find(mode);
  if (it_action != node->actions.end()) {
    action = it_action->second;
  }
  if (more) *more = node->children.size() != 0;
  return action;
}


bool KeyTreeCursor::TryEvent(EventHandler* handler, const Event& event) {
  bool more = false;

  FuncAction action = ConsumeEvent(EncodeKeyEvent(event.key), &more);
  // if (action && more) { } // TODO: Timeout and perform action.

  if (action) {
    bool ret = action(handler);
    ResetCursor();
    return ret;
  }

  // Don't do anything, just wait for the next keystroke and perform on it.
  if (more) return true;

  // Sequence is not registered, reset and listen from start. We'll be sending
  // true and treat this event to reset the cursor.
  if (!IsCursorRoot()) {
    ResetCursor();
    return true;
  }

  return false;
}


// ----------------------------------------------------------------------------
// Event handler.
// ----------------------------------------------------------------------------


EventHandler::EventHandler(const KeyTree* keytree)
  : keytree(keytree), cursor(keytree) { }


bool EventHandler::HandleEvent(const Event& event) {
  return cursor.TryEvent(this, event);
}


void EventHandler::ResetCursor() {
  cursor.ResetCursor();
}


bool EventHandler::ListeningCombination() const {
  return !cursor.IsCursorRoot();
}


void EventHandler::SetMode(const std::string& mode) {
  cursor.SetMode(mode);
}

