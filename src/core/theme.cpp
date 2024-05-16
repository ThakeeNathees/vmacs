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


Theme::Theme(const Json& json) {
  ExtractJson(json);
  UpdateUiEntries();
}


// FIXME: Comup with a proper set of ui entries this one only rely on helix.
void Theme::ExtractJson(const Json& json) {
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
              // TODO: error("Invalid modifier value (%s) for theme %s.", mod, key);
            }
          }
        }
        style.attrib = modifier;
      }

    }
  }

}


void Theme::UpdateUiEntries() {
  text             = GetStyle("ui.text");
  background       = GetStyle("ui.background");
  style            = background.Apply(text);
  error            = GetStyle("error");
  warning          = GetStyle("warning");
  lines            = style.Apply(GetStyle("ui.background.separator"));
  whitespace       = GetStyle("ui.virtual.whitespace");
  menu             = GetStyle("ui.menu");
  menu_selected    = GetStyle("ui.menu.selected");
  tabbar           = menu.Apply(whitespace);
  tabbar_active    = menu_selected.Apply(background);
  statusline       = GetStyle("ui.statusline");
  linenr           = style.Apply(GetStyle("ui.linenr")); // FIXME: check what happens without Apply().
  linenr_selected  = style.Apply(GetStyle("ui.linenr.selected"));
  cursor           = GetStyle("ui.cursor.primary");
  selection        = GetStyle("ui.selection.primary");
  signature_active = GetStyle("type"); // FIXME: Should be something else.
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


Style Theme::GetCompletionItemStyle(int kind_index) const {
  Style ret = text;
  // FIXME: We cannot use the lsp module here since we shouldn't depend on it
  // here find a way to have the same list here as well (sync).
  if (kind_index < 0 || kind_index >= COMPLETION_ITEM_KIND_COUNT) return ret;

  // I'm trying to match the color values with vscode (dark+) since we're using it's icons.
  switch (kind_index + 1) { // Convert the index to kind.
    case 1:  /*   Text */        break;
    case 2:  /*   Method */      ret = GetStyle("function.method"); break;
    case 3:  /* 󰊕  Function */    ret = GetStyle("function.builtin"); break;
    case 4:  /*   Constructor */ ret = GetStyle("keyword.control"); break;
    case 5:  /*   Field */       ret = GetStyle("variable.other.member"); break;
    case 6:  /*   Variable */    ret = GetStyle("variable"); break;
    case 7:  /*   Class */       ret = GetStyle("constant.character.escape"); break;
    case 8:  /*   Interface */   ret = GetStyle("variable"); break;
    case 9:  /*   Module */      break;
    case 10: /*   Property */    ret = GetStyle("property"); break;
    case 11: /*   Unit */        break;
    case 12: /*   Value */       break;
    case 13: /*   Enum */        ret = GetStyle("keyword.storage"); break;
    case 14: /*   Keyword */     ret = GetStyle("keyword"); break;
    case 15: /*   Snippet */     break;
    case 16: /*   Color */       break;
    case 17: /*   File */        break;
    case 18: /*   Reference */   break;
    case 19: /*   Folder */      break;
    case 20: /*   EnumMember */  ret = GetStyle("type.enum.variant"); break;
    case 21: /*   Constant */    ret = GetStyle("constant"); break;
    case 22: /*   Struct */      ret = GetStyle("type"); break;
    case 23: /*   Event */       break;
    case 24: /*   Operator */    ret = GetStyle("operator"); break;
    case 25: /*   TypeParameter */ break;
  }
  return text.Apply(ret);
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

