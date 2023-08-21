// Copyright (c) 2023 Thakee Nathees

#include "widget.h"
#include "window.h"
#include "keybindings.h"


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


void Mode::ResolveParent(ModeList list) {
  for (const std::shared_ptr<Mode>& ptr : *list) {
    if (this == ptr.get()) {
      // TODO: error(cyclic dependency);
      return;
    }
    if (parent_name == ptr->name) {
      parent = ptr;
      return;
    }
  }
}


Widget::Widget(Window* window) {
  this->window = window;
  theme_ptr = window->GetThemeManager().GetThemePtr();
}


void Widget::_UpdateModes(ModeList modes_list) {
  modes = modes_list;
  if (modes == nullptr) return;

  for (auto& mode : *modes) {
    mode->ResolveParent(modes);
  }
  if (mode == nullptr && modes->size() > 0) {
    mode = modes->at(0).get(); // SetMode.
  }
}


void Widget::AddMode(std::shared_ptr<Mode> mode) {
  modes->push_back(mode);
}


void Widget::SetMode(const std::string& name) {
  auto iter = modes->begin();
  while (iter != modes->end()) {
    if (iter->get()->GetName() == name) {
      mode = iter->get();
    }
    ++iter;
  }
  // TODO: error();
}


int Widget::AddChild(std::unique_ptr<Widget> child) {
  child->parent = this;
  children.push_back(std::move(child));
  return (int)children.size() - 1;
}


void Widget::RemoveChild(int index) {
  if (!BETWEEN(0, index, children.size() - 1)) return;
  children[index] = nullptr;
}


Widget* Widget::GetChild(int index) const {
  if (!BETWEEN(0, index, children.size() - 1)) return nullptr;
  return children[index].get();
}


void Widget::SetFocusedChild(int index) {
  focused_child = index;
  for (int i = 0; i < (int)children.size(); i++) {
    Widget* c = children[i].get();
    if (c) c->SetFocused(i == index);
  }
}


void Widget::SetFocusedChild(Widget* widget) {
  focused_child = -1;
  for (int i = 0; i < (int)children.size(); i++) {
    Widget* c = children[i].get();
    if (!c) continue;
    if (c == widget) {
      c->SetFocused(true);
      focused_child = i;
    } else {
      c->SetFocused(false);
    }
  }
}


void Widget::SetFocused(bool is_focused) {
  this->is_focused = is_focused;

  if (is_focused) {
    if (BETWEEN(0, focused_child, children.size() - 1)) {
      Widget* c = children[focused_child].get();
      if (c) c->SetFocused(true);
    }

  } else {
    for (auto& child : children) {
      if (child != nullptr) child->SetFocused(false);
    }
  }

  OnFocusChanged();
}


void Widget::Update() {
  for (auto& child : children) {
    Widget* c = child.get();
    if (c) child->Update();
  }
}


bool Widget::HandleEvent(const Event& event) {

  if (mode == nullptr) goto handle_child;

  switch (event.type) {

    case Event::Type::KEY: {

      const Event::Key& key = event.key;

      if (key.unicode == '1') { window->GetThemeManager().SetTheme("darcula"); return true; }
      if (key.unicode == '2') { window->GetThemeManager().SetTheme("dark_plus"); return true; }
      if (key.unicode == '3') { window->GetThemeManager().SetTheme("jellybeans"); return true; }
      if (key.unicode == '4') { window->GetThemeManager().SetTheme("everforest_dark"); return true; }

      CommandFn fn = nullptr;
      std::vector<std::string> args;

      if (key.unicode != 0) {
        fn = mode->GetCommand("<char>");
        if (fn != nullptr) {
          args.push_back(std::string(1, (char)key.unicode));
        }
      }

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

handle_child:
  Widget* child = GetChild(focused_child);
  if (child && child->HandleEvent(event)) return true;

  return false;
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
