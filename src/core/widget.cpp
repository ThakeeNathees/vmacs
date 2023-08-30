//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "widget.hpp"
#include "window.hpp"


Widget* Widget::AddChild(std::unique_ptr<Widget> child, int index) {
  child->parent = this;
  Widget* ptr = nullptr;
  if (index < 0) {
    children.push_back(std::move(child));
    ptr = children[children.size() - 1].get();
  } else {
    children.insert(children.begin() + index, std::move(child));
    ptr = children[index].get();
  }
  if (focused_child == nullptr) focused_child = ptr;
  return ptr;
}


std::unique_ptr<Widget> Widget::RemoveChild(Widget* widget, int* index) {
  std::unique_ptr<Widget> ret;
  auto it = children.begin();
  while (it != children.end()) {
    if (it->get() == widget) {
      if (widget == focused_child) focused_child = nullptr;
      if (index) *index = (int)(it - children.begin());
      ret = std::move(*it);
      children.erase(it);
      return std::move(ret);
    }
    ++it;
  }
  return nullptr;
}


Widget* Widget::GetParent() const {
  return parent;
}


int Widget::GetChildCount() const {
  return (int) children.size();
}


Widget* Widget::GetChild(int index) const {
  if (!BETWEEN(0, index, children.size() - 1)) return nullptr;
  return children[index].get();
}


int Widget::GetChildIndex(const Widget* child) const {
  for (size_t i = 0; i < children.size(); i++) {
    if (children[i].get() == child) return (int)i;
  }
  return -1;
}


Widget* Widget::GetNextSibling() const {
  if (parent == nullptr) return nullptr;
  return parent->GetChild(parent->GetChildIndex(this) + 1);
}


Widget* Widget::GetPrevSibling() const {
  if (parent == nullptr) return nullptr;
  return parent->GetChild(parent->GetChildIndex(this) - 1);
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


std::unique_ptr<Widget> Widget::Copy() const {
  return nullptr;
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


Widget* Widget::GetFocusedWidget() {
  if (!IsFocused()) return nullptr;
  if (!IsSplit()) return this;
  if (focused_child == nullptr) return nullptr; // Unlikely.
  return focused_child->GetFocusedWidget();
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
