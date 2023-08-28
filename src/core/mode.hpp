//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "common.hpp"
#include "keybindings.hpp"

class Mode;
typedef std::vector<std::shared_ptr<Mode>> ModeList;


class Mode {

public:
  Mode() {} // Need default constructor for the map.

  static std::shared_ptr<Mode> New(const char* name, BindingData&& data);
  static std::shared_ptr<Mode> New(const char* name, bool event_child_first, BindingData&& data);
  static std::shared_ptr<Mode> New(const char* name, const char* parent, bool event_child_first, BindingData&& data);
  static std::shared_ptr<Mode> New(const char* name, const char* parent, BindingData&& data);

  CommandFn GetCommand(const std::string& key_combination) const;
  const KeyBindings* GetBindings() const;
  const std::string& GetName() const;

  bool IsEventChildFirst() const;

  void SetName(const std::string& name);

  // We'll set the parent name when we construct modes and this list contains
  // all the modes for the current widget that also has the parent mode of this.
  void ResolveParent(const ModeList& list);

private:
  std::string name;
  std::string parent_name; // We find the parent from the ModeList using this parent name.
  std::shared_ptr<Mode> parent = nullptr;
  std::unique_ptr<KeyBindings> bindings;

  // If true it'll pass the event to its child to handle and if not it'll
  // handle, otherwise it'll try to handle and pass to its child.
  bool event_child_first = true;
};
