//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "window.hpp"

#include <tomlcpp.hpp>
#include <filesystem>

#include "theme.hpp"
#include "font.hpp"


std::unique_ptr<Window> Window::singleton = nullptr;


void Window::Init() {

  Window::singleton = std::unique_ptr<Window>(new Window);
  singleton->_LoadConfig();

  uint32_t window_flags = FLAG_WINDOW_RESIZABLE; // | FLAG_WINDOW_MAXIMIZED;
  SetConfigFlags(window_flags);
  InitWindow(singleton->width, singleton->height, singleton->title);
  SetExitKey(0); // Don't close on escape.
  SetTargetFPS(singleton->fps);

  // Don't know why but this flag doesn't seems like working while using
  // SetConfigFlag function.
  if (window_flags & FLAG_WINDOW_MAXIMIZED) {
    MaximizeWindow();
  }
}


bool Window::ShouldClose() {
  return Window::singleton->should_close;
}


void Window::HandleInputs() {
  // All the events for the current frame.
  std::vector<Event> events;

  if (WindowShouldClose()) {
    events.push_back(Event(Event::Type::CLOSE));
  }

  // Handle key events.
  // Note that this is not at all the proper way to associate key event with
  // cahr event. If you have a better solution propose me.
  do {

    if (IsWindowResized()) {
      Event e(Event::Type::RESIZE);
      e.resize.width = GetScreenWidth();
      e.resize.height = GetScreenHeight();
      events.push_back(e);
    }

    int key = GetKeyPressed();

    // Ignore all the ctrl, shift, alt, super keys. (handled by the modifiers).
    if (BETWEEN(KEY_LEFT_SHIFT, key, KEY_RIGHT_SUPER)) {
      continue;
    }

    int chr = GetCharPressed();

    if (key == 0 && chr == 0) break;

    Event e(Event::Type::KEY);
    e.key.key = (KeyboardKey)key;
    e.key.unicode = chr;
    e.key.alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    e.key.shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    e.key.control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    events.push_back(e);

  } while (true);

  for (Event& e : events) {
    e.handled = singleton->_HandleEvent(e);
  }
}


void Window::Update() {
  if (singleton->main_widget != nullptr) {
    singleton->main_widget->Update();
  }
}


void Window::Draw() {
  BeginDrawing();
  {
    const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();
    ClearBackground(ui.background.bg);

    Widget* main_widget = singleton->main_widget.get();
    if (main_widget != nullptr) {
      Rectangle area = singleton->_GetDrawArea();
      main_widget->Draw(nullptr, { (int) area.width, (int) area.height });
      DrawRenderTexture(main_widget->GetCanvas(), { area.x, area.y });
    }

  }
  EndDrawing();
}


void Window::Close() {
  CloseWindow();
}


void Window::SetMainWidget(std::unique_ptr<Widget> widget) {
  widget->SetFocused();
  singleton->main_widget = std::move(widget);
}


void Window::Error(std::string_view message) {

}


Rectangle Window::_GetDrawArea() const {
  return {
    (float) border_margin,
    (float) border_margin,
    (float) (GetScreenWidth() - 2 * border_margin),
    (float) (GetScreenHeight() - 2 * border_margin),
  };
}


bool Window::_HandleEvent(const Event& event) {

  if (singleton->main_widget != nullptr) {
    if (singleton->main_widget->HandleEvent(event)) return true;
  }

  if (event.type == Event::Type::CLOSE) {
    singleton->should_close = true;
    return true;
  }

  return false;
}


void Window::_LoadConfig() {

  std::filesystem::path config_path = GetApplicationDirectory();
  config_path /= "config.toml";
  std::string path_str = config_path.string();
  if (!FileExists(path_str.c_str())) return;

  toml::Result res = toml::parseFile(path_str);
  if (res.errmsg.length() > 0) {
    Error(res.errmsg);
    return;
  }

  auto conf_window = res.table->getTable("window");
  if (conf_window) {

    { auto [ok, val] = conf_window->getInt("width");
      if (ok && val > 0) width = (int) val; }

    { auto [ok, val] = conf_window->getInt("height");
      if (ok && val > 0) height = (int) val; }

    { auto [ok, val] = conf_window->getInt("fps");
      if (ok && val > 0) fps = (int) val; }

    { auto [ok, val] = conf_window->getInt("font_size");
      if (ok && val > 0) FontManager::SetFontSize((int) val); }
  }
}
