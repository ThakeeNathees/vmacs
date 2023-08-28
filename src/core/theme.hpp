//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "common.hpp"

class Theme;

class ThemeListener {
public:
  virtual void OnThemeChanged(const Theme* theme) = 0;
};


struct ThemeEntry {
  Color fg;
  Color bg;
  uint8_t modifier;
};


struct UiThemeCache {
#define _ BLANK
  ThemeEntry background     = { _,        WHITE,     0 };
  ThemeEntry text           = { BLACK,    _,         0 };
  ThemeEntry cursor_primary = { GRAY,     _,         0 };
  ThemeEntry selection      = { _,        LIGHTGRAY, 0 };
  ThemeEntry linenr         = { GRAY,     _,         0 };
  ThemeEntry statusline     = { WHITE,    GRAY,      0 };
#undef _
};


class Theme {

  // Only create this object as a unique pointer.
public: static std::unique_ptr<Theme> New(const std::string& name) { return std::unique_ptr<Theme>(new Theme(name)); }
private: Theme(const std::string& name) : name(name) {}

public:
  enum Modifier {
    NONE      = 0,
    BOLD      = 1 << 1,
    ITALIC    = 1 << 2,
    UNDERLINE = 1 << 3,
    CROSSED   = 1 << 4,
    DIM       = 1 << 5,
    REVERSED  = 1 << 6,
  };

  const ThemeEntry* GetEntry(std::string name) const;
  const std::string& GetName() const;

  const UiThemeCache& GetUiEntries() const;

  void LoadFromMemory(const char* toml_source);
  void LoadFromFile(const char* toml_file_path);

  // Convert a string like "#58a6ff" to a color, returns true
  // if success and update the color value.
  static bool StringToColor(const char* str, Color* color);

private:
  void _CacheUiEntries();
  bool _GetColorFromString(const std::string& name, Color* color) const;

  std::string name;
  std::unordered_map<std::string, Color> palette_table;
  std::unordered_map<std::string, ThemeEntry> entries;

  UiThemeCache ui; // "cached" entries.
};



class ThemeManager {
public:

  static void Init();

  static const Theme* GetCurrentTheme();
  static const Theme* GetTheme(const std::string& name);

  static void AddTheme(std::unique_ptr<Theme> theme);
  static void SetTheme(const std::string& name);

  static void RegisterListener(ThemeListener* listener);
  static void UnregisterListener(ThemeListener* listener);

private:
  static void _OnThemeChanged();

  static const Theme* theme;
  static std::unordered_map<std::string, std::unique_ptr<Theme>> themes;
  static std::vector<ThemeListener*> listeners;
};
