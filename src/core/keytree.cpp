//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core.hpp"


KeyTree::KeyTree() {
  root = std::make_unique<KeyTreeNode>();
  ResetCursor();
}


void KeyTree::ResetCursor() {
  cursor.node = root.get();
  cursor.recorded_events.clear();
}


bool KeyTree::IsCursorRoot() const {
  return cursor.node == root.get();
}


void KeyTree::SetMode(const std::string& mode) {
  this->mode = mode;
  ResetCursor();
}


void KeyTree::RegisterBinding(std::string mode, const std::vector<event_t>& events, FuncAction action) {
  KeyTreeNode* curr = root.get();
  for (event_t event : events) {
    if (curr->children.find(event) == curr->children.end()) {
      curr->children[event] = std::make_unique<KeyTreeNode>();
    }
    curr = curr->children[event].get();
  }
  curr->actions[mode] = action;
}


FuncAction KeyTree::ConsumeEvent(event_t event, bool* more) {

  ASSERT(cursor.node != nullptr, OOPS);
  FuncAction action = nullptr;

  auto it_node = cursor.node->children.find(event);
  if (it_node == cursor.node->children.end()) {
    if (more) *more = false;
    return nullptr;
  }

  cursor.node = it_node->second.get();
  ASSERT(cursor.node != nullptr, OOPS);

  cursor.recorded_events.push_back(event);
  auto it_action = cursor.node->actions.find(mode);
  if (it_action != cursor.node->actions.end()) {
    action = it_action->second;
  }
  if (more) *more = cursor.node->children.size() != 0;
  return action;
}

