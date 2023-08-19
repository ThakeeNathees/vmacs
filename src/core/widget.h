// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <vector>
#include <string>
#include <memory>

#include <raylib.h>

#include "common.h"
#include "theme.h"
#include "event.h"
#include "keybindings.h"

class Window;


class Mode {
public:
  Mode() {} // Need default constructor for the map.
  Mode(const std::string& name, std::shared_ptr<Mode> parent, std::unique_ptr<KeyBindings> bindings);

  CommandFn GetCommand(const std::string& key_combination) const;
  const KeyBindings* GetBindings() const { return bindings.get(); }
  const std::string& GetName() const { return name; }

  void SetName(const std::string& name) { this->name = name; }

private:
  std::shared_ptr<Mode> parent = nullptr;
  std::string name;
  std::unique_ptr<KeyBindings> bindings;
};


class Widget {
public:

  Widget(Window* window);

  virtual void Init() {}

  virtual bool HandleEvent(const Event& event);
  virtual void Update();
  virtual void Draw(const Widget* parent, Size area) final;
  virtual void Cleanup() {}

  RenderTexture2D GetCanvas() const { return canvas; }
  
  void AddMode(std::shared_ptr<Mode> mode, bool update);
  void SetMode(const std::string& name);

  void AddChild(std::unique_ptr<Widget> child);
  Widget* GetChild(int index) const;
  Widget* GetParent() const { return parent; }
  int GetChildrenCount() const { return (int)children.size(); }

  bool IsFocused() const { return is_focused; }
  void SetFocused(bool is_focused);
  void SetFocusedChild(int index);

private:
  // Implement this in all child classes and call the above Draw method when
  // needs to draw the widget.
  virtual void _Draw(Size area) {}

protected:
  RenderTexture2D canvas = { 0 };
  Window* window = nullptr;
  ThemePtr theme_ptr = nullptr;

private:
  Mode* mode = nullptr; // Current mode in the modes map.
  std::map<std::string, std::shared_ptr<Mode>> modes;

  Widget* parent = nullptr;
  std::vector<std::unique_ptr<Widget>> children;

  bool is_focused = false;

  // Index of the focused child. When this widget gain focus it'll pass the
  // focus to it's focused_childs along the tree.
  int focused_child = -1;
};
