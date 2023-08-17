// Copyright (c) 2023 Thakee Nathees

#include "widget.h"
#include "window.h"
#include "keybindings.h"


Mode::Mode(const std::string& name, std::shared_ptr<Mode> parent, std::unique_ptr<KeyBindings> bindings)
  : name(name), parent(parent), bindings(std::move(bindings))
{ }


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


Widget::Widget(Window* window) {
  this->window = window;
  theme_ptr = window->GetThemeManager().GetThemePtr();
}


void Widget::AddMode(std::shared_ptr<Mode> mode, bool update) {
  std::shared_ptr<Mode>& m = modes[mode->GetName()];
  m = std::move(mode);
  if (update) this->mode = m.get();
}


void Widget::SetMode(const std::string& name) {
  auto iter = modes.find(name);
  if (iter == modes.end()) {
    // TODO: error("Mode with name %s not exists.", name);
    return;
  }

  mode = iter->second.get();
}


bool Widget::HandleEvent(const Event& event) {
  if (mode == nullptr) {
    // TODO: error("No mode was set to the widget.");
    return false;
  }

  switch (event.type) {

    case Event::Type::KEY:
    {

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

      if (fn == nullptr) {
        std::string key_combination = "";
        if (key.control) key_combination += "ctrl+";
        if (key.shift) key_combination += "shift+";
        if (key.alt) key_combination += "alt+";
        key_combination += KeyBindings::GetKeyName(key.key);
        fn = mode->GetCommand(key_combination);
      }

      if (fn != nullptr) {
        fn(this, &args);
        return true;
      }

    } break;
  }

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
