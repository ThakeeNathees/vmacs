//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include "core/font.hpp"
#include "core/theme.hpp"
#include "editor/language.hpp"

#include "res/fonts/UbuntuMonoNerdFont-Regular.ttf.gen.h"
#include "res/fonts/UbuntuMonoNerdFont-Bold.ttf.gen.h"
#include "res/fonts/UbuntuMonoNerdFont-Italic.ttf.gen.h"


extern "C" const TSLanguage * tree_sitter_c(void);
extern "C" const TSLanguage * tree_sitter_python(void);


void LoadResources() {

  // Load fonts.
  std::unique_ptr<FontLoaderMemory> loader = std::make_unique<FontLoaderMemory>();
  loader->file_type = ".ttf";
  loader->regular_data = fonts_UbuntuMonoNerdFont_Regular_ttf;
  loader->regular_size = fonts_UbuntuMonoNerdFont_Regular_ttf_len;
  loader->bold_data = fonts_UbuntuMonoNerdFont_Bold_ttf;
  loader->bold_size = fonts_UbuntuMonoNerdFont_Bold_ttf_len;
  loader->italic_data = fonts_UbuntuMonoNerdFont_Italic_ttf;
  loader->italic_size = fonts_UbuntuMonoNerdFont_Italic_ttf_len;
  FontManager::SetFontLoader(std::move(loader));

  // Load themes.
  std::unique_ptr<Theme> theme = Theme::New("dark_plus");
  theme->LoadFromMemory(
    #include "res/themes/dark_plus.toml.hpp"
  );
  ThemeManager::AddTheme(std::move(theme));
  ThemeManager::SetTheme("dark_plus");

  // Load languages.
  std::unique_ptr<Language> c = Language::New("c");
  c->SetData(tree_sitter_c());
  c->SetQuery(
    #include "res/queries/c.scm.hpp"
  );
  LanguageManager::AddLanguage(std::move(c));
}
