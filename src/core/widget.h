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
class Mode;

// Yes this is how the widget stores all the modes was constructed statically
// inside each instance.
typedef std::shared_ptr<std::vector<std::shared_ptr<Mode>>> ModeList;


class Mode {

public:
  Mode() {} // Need default constructor for the map.

  static std::shared_ptr<Mode> New(const std::string& name, BindingData&& data);
  static std::shared_ptr<Mode> New(const std::string& name, const std::string& parent, BindingData&& data);

  CommandFn GetCommand(const std::string& key_combination) const;
  const KeyBindings* GetBindings() const { return bindings.get(); }
  const std::string& GetName() const { return name; }

  void ResolveParent(ModeList list);
  void SetName(const std::string& name) { this->name = name; }

private:
  std::string name;
  std::string parent_name; // We find the parent from the ModeList using this parent name.
  std::shared_ptr<Mode> parent = nullptr;
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

  virtual void OnFocusChanged() {}

  RenderTexture2D GetCanvas() const { return canvas; }
  
  // If it doesn't have any modes set, it'll update this to current mode.
  void AddMode(std::shared_ptr<Mode> mode);
  void SetMode(const std::string& name);

  // Returns the index of the child. Note that if we ever remove a child from a widget
  // we only set the slot to nullptr so that the index of a child will never change.
  int AddChild(std::unique_ptr<Widget> child);
  void RemoveChild(int index);
  Widget* GetChild(int index) const;
  Widget* GetParent() const { return parent; }

  bool IsFocused() const { return is_focused; }
  void SetFocused(bool is_focused);
  void SetFocusedChild(int index);
  void SetFocusedChild(Widget* widget);

protected:
  // Implement this in all child classes and call the above Draw method when
  // needs to draw the widget.
  virtual void _Draw(Size area) {}

  // Construct and initialize the list of modes.
  void _UpdateModes(ModeList mode_list);

  RenderTexture2D canvas = { 0 };
  Window* window = nullptr;
  ThemePtr theme_ptr = nullptr;
  Mode* mode = nullptr; // Current mode in the modes list.
  ModeList modes;

private:
  Widget* parent = nullptr;
  std::vector<std::unique_ptr<Widget>> children;

  bool is_focused = false;

  // Index of the focused child. When this widget gain focus it'll pass the
  // focus to it's focused_childs along the tree.
  int focused_child = -1;
};
