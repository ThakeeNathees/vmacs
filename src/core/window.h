// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <string>
#include <vector>

#include <raylib.h>

#include "event.h"
#include "common.h"
#include "theme.h"
#include "widget.h"


class Window {
public:
  void Init();
  bool ShouldClose() const;
  void HandleInputs();
  bool HandleEvent(const Event& event);
  void Update();
  void Draw();
  void Close();

  Rectanglei GetDrawArea() const;
  const Font& GetFont() const { return font; }
  int GetFontSize() const { return font_size; }
  const ThemeManager& GetThemeManager() const { return theme_manager; }
  ThemeManager& GetThemeManager() { return theme_manager; }

  Vector2i GetFontCharSize() const;

private:
  bool should_close = false;
  int font_size = 64;
  Font font;
  int border_margin = 5;

  ThemeManager theme_manager;

public: // For debugging.
  Widget* widget = nullptr;

};
