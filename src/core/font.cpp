//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "font.hpp"

#include "window.hpp"


std::vector<int> FontLoader::cp_ascii;
std::vector<int> FontLoader::cp_nerd;

int FontManager::font_size = 32;
std::shared_ptr<FontWrapper> FontManager::font = nullptr;
std::unique_ptr<FontLoader> FontManager::font_loader;
std::map<int, std::shared_ptr<FontWrapper>> FontManager::cache;


void FontManager::Init() {

  // Nerdfont range:
  // https://github.com/ryanoasis/nerd-fonts/wiki/Glyph-Sets-and-Code-Points
  // WARNING: Only Load requierd icons otherwise it'll take a lot of memory and
  // time to load the font.
#define ADD_CODEPOINTS(cp_name, start, end) \
  for (int i = start; i <= end; i++) FontLoader::cp_name.push_back(i)
  ADD_CODEPOINTS(cp_ascii, 32, 127); // Ascii characters.
  ADD_CODEPOINTS(cp_nerd, 32, 127);  // Ascii characters.
#undef ADD_CODEPOINTS

  #define ICON(value, name) FontLoader::cp_nerd.push_back(value);
  #include "icons.hpp"
  #undef ICON
}



FontWrapper::FontWrapper(Font regular, Font* bold, Font* italic) {

  ASSERT(IsFontReady(regular), "Regular font was not loaded.");
  this->regular = regular;

  if (bold != nullptr) this->bold = *bold;
  else { this->bold = regular; unload_bold = false; }

  if (italic != nullptr) this->italic = *italic;
  else { this->italic = regular; unload_italic = false; }
}


FontWrapper::~FontWrapper() {
  UnloadFont(regular);
  if (unload_bold) UnloadFont(bold);
  if (unload_italic) UnloadFont(italic);
}


Size FontWrapper::GetCharSize() const {
  // TODO: These should be runtime errors.
  ASSERT(IsFontReady(regular), "Font was not loaded.");
  ASSERT(regular.glyphCount > 0, "No glyph were in the font.");

  // TODO: using any other index than 0 return different (small) size
  // in a mono font, investigate.
  return {
    (int) (regular.recs[0].width ),
    (int) (regular.recs[0].height)
  };
}


std::unique_ptr<FontWrapper> FontLoaderMemory::LoadFont(int font_size) {
  ASSERT(file_type != nullptr, OOPS);
  ASSERT(regular_data != nullptr, OOPS);

  Font regular = { 0 }, bold = { 0 }, italic = { 0 };

  regular = LoadFontFromMemory(
    file_type,
    regular_data,
    regular_size,
    font_size,
    cp_nerd.data(),
    (int) cp_nerd.size());

  if (bold_data != nullptr) {
    bold = LoadFontFromMemory(
      file_type,
      bold_data,
      bold_size,
      font_size,
      cp_ascii.data(),
      (int) cp_ascii.size());
  }

  if (italic_data != nullptr) {
    italic = LoadFontFromMemory(
      file_type,
      italic_data,
      italic_size,
      font_size,
      cp_ascii.data(),
      (int) cp_ascii.size());
  }

  return std::make_unique<FontWrapper>(
    regular,
    (IsFontReady(bold)) ? &bold : nullptr,
    (IsFontReady(italic)) ? &italic : nullptr
  );
}


void FontManager::SetFontLoader(std::unique_ptr<FontLoader> font_loader) {
  if (font_loader == nullptr) return;
  FontManager::font_loader = std::move(font_loader);
  // TODO: Since the font is changed, we have to clean all the cached fonts.
  font = GetFont(font_size);
}


void FontManager::SetFontSize(int font_size) {
  FontManager::font_size = font_size;
  font = GetFont(font_size);
}


int FontManager::GetFontSize() {
  return FontManager::font_size;
}


std::shared_ptr<FontWrapper> FontManager::AddFont(int font_size, std::unique_ptr<FontWrapper> font) {
  // TODO: Remove the most un-used font from the cache if cache is filled.
  cache[font_size] = std::move(font);
  return cache[font_size];
}


std::shared_ptr<FontWrapper> FontManager::GetFont() {
  if (font != nullptr) return font;
  font = GetFont(font_size);
  if (font != nullptr) return font;

  // Load default font.
  font = std::make_shared<FontWrapper>(GetFontDefault(), nullptr, nullptr);

  return font;
}


std::shared_ptr<FontWrapper> FontManager::GetFont(int font_size) {

  auto it = cache.find(font_size);
  if (it != cache.end()) return it->second;

  if (font_loader == nullptr) return nullptr;
  auto fptr = font_loader->LoadFont(font_size);
  if (fptr != nullptr) return AddFont(font_size, std::move(fptr));

  Window::Error("Loading font was failed.");
  return nullptr;
}
