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
#include "resources/lang.xmacro.inl"

#define X(id, q_highlight, loader) \
  extern "C" const TSLanguage* loader(void);
  LANGUAGE(X)
#undef X


std::map<std::string, Json> Platform::LoadThemes() {

  // The return value.
  std::map<std::string, Json> themes;

  // TODO: log the editor it's failed (invalid json).
  #define X(theme_name, theme_source)        \
    try {                                    \
      Json data = Json::parse(theme_source); \
      themes[theme_name] = std::move(data);  \
    } catch (std::exception) {}
  THEMES(X);
  #undef X

  return std::move(themes);
}


// TODO: Implement dynamic loading of ".so", ".dll" files in the runtime to
// support custom langauges outside of what we ship.
std::vector<LanguageLoadResult> Platform::LoadLanguages() {

  std::vector<LanguageLoadResult> languages;

  #define X(id, q_highlight, loader)       \
  do {                                     \
    LanguageLoadResult lang;               \
    lang.language_id        = id;          \
    lang.query_highlight    = q_highlight; \
    lang.tree_sitter_loader = loader;      \
    languages.push_back(lang);             \
  } while (false);
    LANGUAGE(X);
  #undef X

  return std::move(languages);
}

