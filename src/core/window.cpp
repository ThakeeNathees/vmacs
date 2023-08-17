// Copyright (c) 2023 Thakee Nathees

#include <memory>

#include "window.h"

#include "res/fonts/NerdDroidSansMono.otf.gen.h"
#include "res/themes/darcula.toml.gen.h"
#include "res/themes/jellybeans.toml.gen.h"
#include "res/themes/dark_plus.toml.gen.h"
#include "res/themes/everforest_dark.toml.gen.h"


void Window::Init() {

  // FIXME: These values are hardcoded.
  int screen_size_x = 800;
  int screen_size_y = 450;
  const char* title = "vmacs";
  uint32_t window_flags = FLAG_WINDOW_RESIZABLE; // | FLAG_WINDOW_MAXIMIZED;
  int fps = 60;


  SetConfigFlags(window_flags);
  InitWindow(screen_size_x, screen_size_y, title);
  SetExitKey(0); // Don't close on escape.
  SetTargetFPS(fps);

  // Don't know why but this flag doesn't seems like working while using
  // SetConfigFlag function.
  if (window_flags & FLAG_WINDOW_MAXIMIZED) {
    MaximizeWindow();
  }


  // FIXME: This is ugly and temp.
  font = LoadFontFromMemory(
    ".otf",
    fonts_NerdDroidSansMono_otf,
    fonts_NerdDroidSansMono_otf_len,
    font_size,
    NULL,
    0
  );

  {
    std::unique_ptr<Theme> t = Theme::New("darcula");
    t->LoadFromMemory(darcula_toml);
    theme_manager.AddTheme(std::move(t), true);
  }

  {
    std::unique_ptr<Theme> t = Theme::New("dark_plus");
    t->LoadFromMemory(dark_plus_toml);
    theme_manager.AddTheme(std::move(t), false);
  }

  {
    std::unique_ptr<Theme> t = Theme::New("jellybeans");
    t->LoadFromMemory(jellybeans_toml);
    theme_manager.AddTheme(std::move(t), false);
  }

  {
    std::unique_ptr<Theme> t = Theme::New("everforest_dark");
    t->LoadFromMemory(everforest_dark_toml);
    theme_manager.AddTheme(std::move(t), false);
  }



  if (widget != nullptr) widget->Init();
}


bool Window::ShouldClose() const {
  return should_close;
}


void Window::Close() {
  if (widget != nullptr) widget->Cleanup();
  CloseWindow();
}


void Window::HandleInputs() {
  // All the events for the current frame.
  std::vector<Event> events;

  if (WindowShouldClose()) {
    events.push_back(Event(Event::Type::CLOSE));
    should_close = true;
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
    e.handled = HandleEvent(e);
  }
}


void Window::Update() {
  if (widget != nullptr) widget->Update();
}


void Window::Draw() {
  
  const Theme* theme = theme_manager.GetCurrentTheme();
  ASSERT(theme != nullptr, OOPS);

  const UiThemeCache& ui = theme->GetUiEntries();
  ClearBackground(ui.background.bg);

  if (widget != nullptr) {
    Rectanglei area = GetDrawArea();
    widget->Draw(nullptr, { area.width, area.height });
    DrawRenderTexture(widget->GetCanvas(), { (float)area.x, (float)area.y });
  }
}


Rectanglei Window::GetDrawArea() const {
  return {
    border_margin,
    border_margin,
    GetScreenWidth() - 2 * border_margin,
    GetScreenHeight() - 2 * border_margin,
  };
}


Vector2i Window::GetFontCharSize() const {
  // TODO: These should be runtime errors.
  ASSERT(IsFontReady(font), "Font was not loaded.");
  ASSERT(font.glyphCount > 0, "No glyph were in the font.");
  return { (int) font.recs[0].width, (int) font.recs[0].height };
}


bool Window::HandleEvent(const Event& event) {
  if (widget != nullptr) {
    if (widget->HandleEvent(event)) return true;
  }
  return false;
}

