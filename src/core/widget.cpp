//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "widget.hpp"
#include "window.hpp"


Widget* Widget::AddChild(std::unique_ptr<Widget> child) {
  child->parent = this;
  children.push_back(std::move(child));
  return children[children.size() - 1].get();
}


void Widget::RemoveChild(Widget* widget) {
  auto it = children.begin();
  while (it != children.end()) {
    if (it->get() == widget) {
      children.erase(it);
      return;
    }
    ++it;
  }
}


Widget* Widget::GetParent() const {
  return parent;
}


bool Widget::IsFocused() const {
  return is_focused;
}


void Widget::SetFocused(bool is_focused) {
  this->is_focused = is_focused;

  if (is_focused) {
    if (focused_child != nullptr) focused_child->SetFocused(true);

  } else {
    for (auto& child : children) {
      child->SetFocused(false);
    }
  }

  OnFocusChanged();
}


Widget* Widget::GetFocusedChild() const {
  return focused_child;
}


void Widget::SetFocusedChild(Widget* widget) {
  focused_child = nullptr;
  for (int i = 0; i < (int) children.size(); i++) {
    Widget* c = children[i].get();
    if (c == widget) focused_child = widget;
    c->SetFocused(c == widget);
  }
}


RenderTexture2D Widget::GetCanvas() const {
  return canvas;
}


void Widget::_SetModes(ModeList&& modes_list) {
  modes = std::move(modes_list);

  for (auto& mode : modes) {
    mode->ResolveParent(modes);
  }

  // By default set the first mode.
  if (mode == nullptr && modes.size() > 0) {
    mode = modes.at(0).get();
  }
}


void Widget::SetMode(const std::string_view name) {
  auto iter = modes.begin();
  while (iter != modes.end()) {
    if (iter->get()->GetName() == name) {
      mode = iter->get();
      return;
    }
    ++iter;
  }
  Window::Error(std::string("Mode not found to set ('") + std::string(name) + "').");
}


void Widget::Update() {
  for (auto& child : children) {
    Widget* c = child.get();
    child->Update();
  }
}


void Widget::Draw(const Widget* parent, Size area) {
  if (area.width == 0 || area.height == 0) return;
  ASSERT(area.width > 0 && area.height > 0, OOPS);

  // Create / update the canvas.
  if (!IsRenderTextureReady(canvas)) {
    canvas = LoadRenderTexture(area.width, area.height);
  } else {
    if (canvas.texture.width != area.width || canvas.texture.height != area.height) {
      UnloadRenderTexture(canvas);
      canvas = LoadRenderTexture(area.width, area.height);
    }
  }

  BeginTextureMode(canvas);
  {
    _Draw(area);
  }
  if (parent != nullptr) BeginTextureMode(parent->GetCanvas());
  else EndTextureMode();

}


bool Widget::HandleEvent(const Event& event) {

  Widget* child = GetFocusedChild();
  if (child && child->HandleEvent(event)) return true;

  switch (event.type) {

    case Event::Type::KEY: {

      const Event::Key& key = event.key;

      CommandFn fn = nullptr;
      std::vector<std::string> args;

      if (key.unicode != 0) {
        fn = mode->GetCommand("<char>");
        if (fn != nullptr) {
          args.push_back(std::string(1, (char)key.unicode));
        }
      }

      // FIXME: Keycombination should be bitmasked integer not string.
      const char* key_name = KeyBindings::GetKeyName(key.key);
      if (fn == nullptr && key_name != nullptr) {
        std::string key_combination = "";
        if (key.control) key_combination += "ctrl+";
        if (key.shift) key_combination += "shift+";
        if (key.alt) key_combination += "alt+";
        key_combination += key_name;
        fn = mode->GetCommand(key_combination);
      }

      // No command bind to this combination, try "<other>".
      if (fn == nullptr) {
        fn = mode->GetCommand("<other>");
      }

      if (fn != nullptr) {
        fn(this, &args);
        return true;
      }

    } break;
  }

  return false;
}
