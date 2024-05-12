//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core.hpp"


// -----------------------------------------------------------------------------
// ActionExecutor.
// -----------------------------------------------------------------------------


const std::string& ActionExecutor::GetMode() const {
  return mode;
}


void ActionExecutor::SetMode(const std::string& mode) {
  this->mode = mode;
}


// -----------------------------------------------------------------------------
// KeyTree.
// -----------------------------------------------------------------------------


KeyTree::KeyTree() {
  root = std::make_unique<Node>();
}


void KeyTree::RegisterAction(const ActExName& class_name, const ActionName& action_name, FuncAction action) {
  ActionKey key = std::make_pair(class_name, action_name);
  actions[key] = action;
}


void KeyTree::RegisterBinding(const ActExName& actex_name, const std::string& key_combination, const std::string& action_name) {
  RegisterBinding(actex_name, "", key_combination, action_name);
}


void KeyTree::RegisterBinding(const ActExName& actex_name, const ModeName& mode, const std::string& key_combination, const std::string& action_name) {
  std::vector<event_t> events;

  if (!ParseKeyBindingString(events, key_combination.c_str())) {
    // TODO: Report error to user.
    return;
  }

  ActionKey action_key = std::make_pair(actex_name, action_name);
  auto it = actions.find(action_key);
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

  BindingKey binding_key = std::make_pair(actex_name, mode);
  curr->bindings[binding_key] = it->second;
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


bool KeyTreeCursor::HasMore() const {
  ASSERT(node != nullptr, OOPS);
  return !node->children.empty();
}


bool KeyTreeCursor::ConsumeEvent(const Event& event) {
  ASSERT(tree != nullptr, OOPS);

  event_t key = EncodeKeyEvent(event.key);

  auto it_node = node->children.find(key);
  if (it_node == node->children.end()) {
    return false;
  }

  node = it_node->second.get();
  ASSERT(node != nullptr, OOPS);
  recorded_events.push_back(key);

  return true;
}


bool KeyTreeCursor::TryEvent(ActionExecutor* actex) {
  ASSERT(actex != nullptr, OOPS);

  BindingKey key = std::make_pair(actex->GetClassName(), actex->GetMode());
  auto it_action = node->bindings.find(key);
  if (it_action != node->bindings.end()) {
    FuncAction action = it_action->second;
    ASSERT(action != nullptr, OOPS);
    return action(actex);
  }

  return false;
}


