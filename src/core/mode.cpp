//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "mode.hpp"


std::shared_ptr<Mode> Mode::New(const std::string& name, BindingData&& data) {
  return New(name, "", std::move(data));
}


std::shared_ptr<Mode> Mode::New(const std::string& name, const std::string& parent, BindingData&& data) {
  std::shared_ptr<Mode> mode = std::make_shared<Mode>();
  mode->name = name;
  mode->parent_name = parent;
  mode->bindings = KeyBindings::New(std::move(data));
  return mode;
}


CommandFn Mode::GetCommand(const std::string& key_combination) const {
  const Mode* mode = this;
  CommandFn fn = nullptr;
  while (fn == nullptr) {
    if (mode == nullptr) break;
    fn = mode->bindings->GetCommand(key_combination);
    mode = mode->parent.get();
  }
  return fn;
}


const KeyBindings* Mode::GetBindings() const {
  return bindings.get();
}


const std::string& Mode::GetName() const {
  return name;
}


void Mode::ResolveParent(const ModeList& list) {
  for (const std::shared_ptr<Mode>& ptr : list) {
    if (this == ptr.get()) continue;
    if (parent_name == ptr->name) {
      parent = ptr;
      return;
    }
  }
}


void Mode::SetName(const std::string& name) {
  this->name = name;
}
