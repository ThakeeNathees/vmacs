//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "platform.hpp"

#include "resources/themes.xmacro.inl"

std::map<std::string, Json> Platform::LoadThemes() {

  // The return value.
  std::map<std::string, Json> themes;

  // TODO: log the editor it's failed (invalid json).
  #define X(theme_name, theme_source)        \
    try {                                    \
      Json data = Json::parse(theme_source); \
      themes[theme_name] = std::move(data);  \
    } catch (std::exception) {}
  THEMES(X)
  #undef X

  // std::vector<std::string> dir_items;
  // if (!ListDirectory(&dir_items, path)) return std::move(themes);

  // for (const std::string& filename : dir_items) {
  //   std::string text;
  //   if (!EndsWith(filename, ".json")) continue;
  //   if(!ReadFile(&text, path / filename)) continue; // Error reading file.
  //   try {
  //     Json data = Json::parse(text);
  //     std::string theme_name = filename.substr(0, filename.find_last_of("."));
  //     themes[theme_name] = std::move(data);
  //   } catch (std::exception) {} // TODO: log the editor it's failed (invalid json).
  // }

  return std::move(themes);
}


