//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "common.hpp"

enum class FontIcon {
#define ICON(value, name) name = value,
#include "icons.hpp"
#undef ICON
};


// A wrapper class of raylib font that contains Regular, bold, italic fonts.
// This will take ownership of those fonts and will unload when destroy.
struct FontWrapper {
  Font regular, bold, italic;

  // Regular should not be nullptr, if bold or italic is nullptr, we'll set
  // regular to them.
  FontWrapper(Font regular, Font* bold, Font* italic);
  ~FontWrapper();

  Size GetCharSize() const;

private:
  // We shouldn't unload bold/italic font if they're referencing the regular font.
  bool unload_bold = true;
  bool unload_italic = true;
};


class FontLoader {
public:
  virtual std::unique_ptr<FontWrapper> LoadFont(int font_size) = 0;

protected:
  friend class FontManager;
  static std::vector<int> cp_ascii; // Ascii only for bold and italic fonts.
  static std::vector<int> cp_nerd;  // Contains nerd icons for regular font.
};


// The fields of this class are public, set the values before calling LoadFont
// method.
class FontLoaderMemory : public FontLoader {
public:
  const char* file_type = nullptr;
  const unsigned char* regular_data = nullptr;
  int regular_size = 0;
  const unsigned char* bold_data = nullptr;
  int bold_size = 0;
  const unsigned char* italic_data = nullptr;
  int italic_size = 0;
  std::unique_ptr<FontWrapper> LoadFont(int font_size) override;
};


// This is the class we intract with inorder to get / cache and update fonts.
class FontManager {
public:
  static void Init();
  static void SetFontLoader(std::unique_ptr<FontLoader> font_loader);
  static void SetFontSize(int font_size);

  static int GetFontSize();
  static std::shared_ptr<FontWrapper> GetFont();
  static std::shared_ptr<FontWrapper> GetFont(int font_size);
  static std::shared_ptr<FontWrapper> AddFont(int font_size, std::unique_ptr<FontWrapper> font); // Add the font and return it as a weak ref.

private:
  static int font_size;
  static std::shared_ptr<FontWrapper> font; // Default font.
  static std::unique_ptr<FontLoader> font_loader;

  static std::map<int, std::shared_ptr<FontWrapper>> cache; // Here key is the font size.
};
