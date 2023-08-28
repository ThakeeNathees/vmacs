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
  Widget* ptr = children[children.size() - 1].get();
  if (focused_child == nullptr) focused_child = ptr;
  return ptr;
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


int Widget::GetChildCount() const {
  return (int) children.size();
}


Widget* Widget::GetChild(int index) const {
  if (!BETWEEN(0, index, children.size())) return nullptr;
  return children[index].get();
}


bool Widget::IsFocused() const {
  return is_focused;
}


void Widget::_RemoveFocusTree() {
  bool changed = is_focused;
  is_focused = false;
  for (auto& child : children) {
    child->_RemoveFocusTree();
  }
  if (changed) OnFocusChanged();
}


void Widget::SetFocused() {

  // Update the focus tree.
  if (focused_child != nullptr) {
    focused_child->SetFocused();
    return;
  }

  Widget* curr = this;
  while (curr) {

    bool changed = !curr->is_focused;
    curr->is_focused = true;
    if (changed) OnFocusChanged();

    if (curr->parent != nullptr) {
      curr->parent->focused_child = curr;

      for (auto& child : curr->parent->children) {
        if (child.get() == curr) continue;
        child.get()->_RemoveFocusTree();
      }
      
    }
    curr = curr->parent;
  }
}


Widget* Widget::GetFocusedChild() const {
  return focused_child;
}


RenderTexture2D Widget::GetCanvas() const {
  return canvas;
}


void Widget::_SetModes(std::shared_ptr<ModeList> modes_list) {
  modes = std::move(modes_list);

  for (auto& mode : *modes) {
    mode->ResolveParent(*modes);
  }

  // By default set the first mode.
  if (mode == nullptr && modes->size() > 0) {
    mode = modes->at(0).get();
  }
}


void Widget::SetMode(const std::string_view name) {
  auto iter = modes->begin();
  while (iter != modes->end()) {
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
  if (area.width <= 0 || area.height <= 0) return;

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


bool Widget::IsSplit() const {
  return false;
}


bool Widget::HandleEvent(const Event& event) {

  bool event_child_first = (mode != nullptr) ? mode->IsEventChildFirst() : true;

  if (event_child_first) {
    Widget* child = GetFocusedChild();
    if (child && child->HandleEvent(event)) return true;
  }

  switch (event.type) {

    case Event::Type::KEY: {
      if (mode == nullptr) break;

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

  if (!event_child_first) {
    Widget* child = GetFocusedChild();
    if (child && child->HandleEvent(event)) return true;
  }

  return false;
}
