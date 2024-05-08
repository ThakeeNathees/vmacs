//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include <tomlcpp.hpp>


static bool ResolveColorFromString(
    const std::map<std::string, Color>& palette,
    const std::string& name,
    Color* color) {

  if (name.size() >= 1 && name.at(0) == '#') {
    return Theme::StringToColor(name.c_str(), color);
  }

  auto iter = palette.find(name);
  if (iter == palette.end()) return false;
  *color = iter->second;
  return true;
}


Style Style::Apply(const Style& other) const {
  Style ret = *this;
  if (other.fg != std::nullopt) ret.fg = other.fg;
  if (other.bg != std::nullopt) ret.bg = other.bg;
  ret.attrib |= other.attrib;
  return ret;
}


void Style::ApplyInplace(const Style& other) {
  if (other.fg != std::nullopt) fg = other.fg;
  if (other.bg != std::nullopt) bg = other.bg;
  attrib |= other.attrib;
}


// TODO: Comup with a proper set of ui entries this one only rely on helix.
Theme::Theme(const Json& json) {
  std::map<std::string, Color> palette;

  if (json.contains("palette")) {
    const Json& json_palette = json["palette"];
    for (auto it = json_palette.begin(); it != json_palette.end(); ++it) {
      if (!it.value().is_string()) {
        // TODO: error("Invalid config value for palette key %s. Expected a string color", it.key());
        continue;
      }
      Color rgb;
      if (!StringToColor(it.value().template get<std::string>().c_str(), &rgb)) {
        // TODO: error("Invalid color value (%s) for palette key %s.", value, it.key());
        continue;
      }
      palette[it.key()] = rgb;
    }
  } // Populate palette.

  for (auto it = json.begin(); it != json.end(); ++it) {

    // Value can be either color string or a table.

    { // Check if string.

      if (it.value().is_string()) {
        std::string name = it.value().template get<std::string>();
        Color rgb;
        if (!ResolveColorFromString(palette, name, &rgb)) {
          // TODO: error("Invalid palette key / or color value (%s) for theme %s.", value, key);
        } else {
          Style& style = entries[it.key()];
          style.fg = rgb;
        }

        continue;
      }
    }

    { // Must be table then.
      if (!it.value().is_object()) {
        // TODO: error("Invalid theme entry (%s) for theme %s.", value, key);
        continue;
      }

      const Json& table = it.value();

      Style& style = entries[it.key()];
      Color rgb;

      // fg.
      {
        if (table.contains("fg") && table["fg"].is_string()) {
          std::string name = table["fg"];
          if (!ResolveColorFromString(palette, name, &rgb)) {
            // TODO: error("Invalid palette key / or color value (%s) for theme %s.", value, key);
          } else {
            style.fg = rgb;
          }
        }
      }

      // bg.
      {
        if (table.contains("bg") && table["bg"].is_string()) {
          std::string name = table["bg"];
          if (!ResolveColorFromString(palette, name, &rgb)) {
            // TODO: error("Invalid palette key / or color value (%s) for theme %s.", value, key);
          } else {
            style.bg = rgb;
          }
        }
      }

      // modifiers.
      {
        uint8_t modifier = 0;
        if (table.contains("modifiers") && table["modifiers"].is_array()) {
          for (auto& it : table["modifiers"]) {
            if (!it.is_string()) continue;
            std::string mod = it.template get<std::string>();
            if (mod == "bold")             modifier |= VMACS_CELL_BOLD;
            else if (mod == "italic")      modifier |= VMACS_CELL_ITALIC;
            else if (mod == "underline")   modifier |= VMACS_CELL_UNDERLINE;
            // else if (mod == "crossed_out") modifier |= ;
            // else if (mod == "dim")         modifier |= ;
            else if (mod == "reversed")    modifier |= VMACS_CELL_REVERSE;
            else {
              // error("Invalid modifier value (%s) for theme %s.", mod, key);
            }
          }
        }
        style.attrib = modifier;
      }

    }
  }

}


Style Theme::GetStyle(const std::string& capture_) const {
  std::string capture = capture_; // We need to modify.
  while (true) {
    auto iter = entries.find(capture);
    if (iter != entries.end()) {
      return iter->second;
    }
    size_t pos = capture.find_last_of('.');
    if (pos == std::string::npos) return Style();
    capture.erase(pos, capture.size() - pos);
  }
  UNREACHABLE();
  return Style();
}


bool Theme::StringToColor(const char* str, Color* rgb) {
  ASSERT(str != nullptr && rgb != nullptr, OOPS);
  if (*str != '#') return false;

  str++; // Skip the '#'.

  for (int i = 0; i < 6; i++) {
    char c = str[i];
    if (!(BETWEEN('0', c, '9') || BETWEEN('a', c, 'f') || BETWEEN('A', c, 'F'))) return false;
  }

#define HEX_TO_INT(c)         \
  BETWEEN('0', (c), '9')      \
    ? ((c) - '0')             \
    : (BETWEEN('a', (c), 'f') \
        ? ((c) - 'a' + 10)    \
        : ((c) - 'A' + 10))

  uint8_t hex[6];
  for (int i = 0; i < 6; i++) hex[i] = HEX_TO_INT(str[i]);
#undef HEX_TO_INT

  uint8_t r = hex[0] << 4 | hex[1];
  uint8_t g = hex[2] << 4 | hex[3];
  uint8_t b = hex[4] << 4 | hex[5];

  *rgb = (r<<16) | (g<<8) | b;

  return true;
}

