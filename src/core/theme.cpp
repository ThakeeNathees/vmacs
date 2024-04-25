//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include <tomlcpp.hpp>



bool Theme::GetStyle(Style* style, const std::string& capture_) const {
  std::string capture = capture_; // We need to modify.
  while (true) {
    auto iter = entries.find(capture);
    if (iter != entries.end()) {
      *style = iter->second;
      return true;
    }
    size_t pos = capture.find_last_of('.');
    if (pos == std::string::npos) return false;
    capture.erase(pos, capture.size() - pos);
  }
  UNREACHABLE();
  return false;
}

#if 0
Theme Theme::Get() {
#define X(val) RgbToXterm(val)
  Theme theme;
  theme.entries["namespace"]                 = { .fg = X(0x4EC9B0), .bg = X(0x000000), .attrib = 0  };
  theme.entries["module"]                    = { .fg = X(0x4EC9B0), .bg = X(0x000000), .attrib = 0  };
  theme.entries["type"]                      = { .fg = X(0x4EC9B0), .bg = X(0x000000), .attrib = 0  };
  theme.entries["type.builtin"]              = { .fg = X(0x4EC9B0), .bg = X(0x000000), .attrib = 0  };
  theme.entries["type.enum.variant"]         = { .fg = X(0x4FC1FF), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constructor"]               = { .fg = X(0x4EC9B0), .bg = X(0x000000), .attrib = 0  };
  theme.entries["variable.other.member"]     = { .fg = X(0x9CDCFE), .bg = X(0x000000), .attrib = 0  };
  theme.entries["keyword"]                   = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["keyword.directive"]         = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["keyword.control"]           = { .fg = X(0xC586C0), .bg = X(0x000000), .attrib = 0  };
  theme.entries["label"]                     = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["tag"]                       = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["special"]                   = { .fg = X(0xd4d4d4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["operator"]                  = { .fg = X(0xd4d4d4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["punctuation"]               = { .fg = X(0xd4d4d4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["punctuation.delimiter"]     = { .fg = X(0xd4d4d4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["variable"]                  = { .fg = X(0x9CDCFE), .bg = X(0x000000), .attrib = 0  };
  theme.entries["variable.parameter"]        = { .fg = X(0x9CDCFE), .bg = X(0x000000), .attrib = 0  };
  theme.entries["variable.builtin"]          = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant"]                  = { .fg = X(0x4FC1FF), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.builtin"]          = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["function"]                  = { .fg = X(0xDCDCAA), .bg = X(0x000000), .attrib = 0  };
  theme.entries["function.builtin"]          = { .fg = X(0xDCDCAA), .bg = X(0x000000), .attrib = 0  };
  theme.entries["function.macro"]            = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["attribute"]                 = { .fg = X(0xDCDCAA), .bg = X(0x000000), .attrib = 0  };
  theme.entries["comment"]                   = { .fg = X(0x6A9955), .bg = X(0x000000), .attrib = 0  };
  theme.entries["string"]                    = { .fg = X(0xce9178), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.character"]        = { .fg = X(0xce9178), .bg = X(0x000000), .attrib = 0  };
  theme.entries["string.regexp"]             = { .fg = X(0xd7ba7d), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.numeric"]          = { .fg = X(0xb5cea8), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.character.escape"] = { .fg = X(0xd7ba7d), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading"]            = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = VMACS_CELL_BOLD  };
  theme.entries["markup.list"]               = { .fg = X(0x6796E6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.bold"]               = { .fg = X(0x569CD6), .bg = X(0x000000), .attrib = VMACS_CELL_BOLD  };
  theme.entries["markup.italic"]             = { .fg = X(0x000000), .bg = X(0x000000), .attrib = VMACS_CELL_ITALIC  };
  theme.entries["markup.strikethrough"]      = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.link.url"]           = { .fg = X(0x000000), .bg = X(0x000000), .attrib = VMACS_CELL_UNDERLINE  };
  theme.entries["markup.link.text"]          = { .fg = X(0xce9178), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.quote"]              = { .fg = X(0x6A9955), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.raw"]                = { .fg = X(0xce9178), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diff.plus"]                 = { .fg = X(0x487e02), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diff.delta"]                = { .fg = X(0x1b81a8), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diff.minus"]                = { .fg = X(0xf14c4c), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.background"]             = { .fg = X(0xd4d4d4), .bg = X(0x1e1e1e), .attrib = 0  };
  theme.entries["ui.window"]                 = { .fg = X(0x000000), .bg = X(0x252526), .attrib = 0  };
  theme.entries["ui.popup"]                  = { .fg = X(0xd4d4d4), .bg = X(0x252526), .attrib = 0  };
  theme.entries["ui.help"]                   = { .fg = X(0xd4d4d4), .bg = X(0x252526), .attrib = 0  };
  theme.entries["ui.menu"]                   = { .fg = X(0xd4d4d4), .bg = X(0x252526), .attrib = 0  };
  theme.entries["ui.menu.selected"]          = { .fg = X(0x000000), .bg = X(0x094771), .attrib = 0  };
  theme.entries["ui.cursor"]                 = { .fg = X(0xa6a6a6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.cursor.primary"]         = { .fg = X(0xa6a6a6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.cursor.match"]           = { .fg = X(0x000000), .bg = X(0x3a3d41), .attrib = VMACS_CELL_UNDERLINE  };
  theme.entries["ui.selection"]              = { .fg = X(0x000000), .bg = X(0x3a3d41), .attrib = 0  };
  theme.entries["ui.selection.primary"]      = { .fg = X(0x000000), .bg = X(0x264f78), .attrib = 0  };
  theme.entries["ui.linenr"]                 = { .fg = X(0x858585), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.linenr.selected"]        = { .fg = X(0xc6c6c6), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.cursorline.primary"]     = { .fg = X(0x000000), .bg = X(0x282828), .attrib = 0  };
  theme.entries["ui.statusline"]             = { .fg = X(0xffffff), .bg = X(0x007acc), .attrib = 0  };
  theme.entries["ui.statusline.inactive"]    = { .fg = X(0xffffff), .bg = X(0x007acc), .attrib = 0  };
  theme.entries["ui.statusline.insert"]      = { .fg = X(0xffffff), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.statusline.select"]      = { .fg = X(0xffffff), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.bufferline"]             = { .fg = X(0xd4d4d4), .bg = X(0x252526), .attrib = 0  };
  theme.entries["ui.bufferline.active"]      = { .fg = X(0xffffff), .bg = X(0x007acc), .attrib = 0  };
  theme.entries["ui.bufferline.background"]  = { .fg = X(0x000000), .bg = X(0x1e1e1e), .attrib = 0  };
  theme.entries["ui.text"]                   = { .fg = X(0xd4d4d4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.text.focus"]             = { .fg = X(0xffffff), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.virtual.whitespace"]     = { .fg = X(0x858585), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.virtual.ruler"]          = { .fg = X(0x000000), .bg = X(0x323232), .attrib = 0  };
  theme.entries["ui.virtual.indent-guide"]   = { .fg = X(0x404040), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.virtual.inlay-hint"]     = { .fg = X(0xffffff), .bg = X(0x444444), .attrib = 0  };
  theme.entries["warning"]                   = { .fg = X(0xcca700), .bg = X(0x000000), .attrib = 0  };
  theme.entries["error"]                     = { .fg = X(0xff1212), .bg = X(0x000000), .attrib = 0  };
  theme.entries["info"]                      = { .fg = X(0x75beff), .bg = X(0x000000), .attrib = 0  };
  theme.entries["hint"]                      = { .fg = X(0xeeeeee), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diagnostic.error"]          = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diagnostic"]                = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  return theme;
}
#endif

Theme Theme::Get() {
  Theme theme;

#define X(val) RgbToXterm(val)
  theme.entries["type"] = { .fg = X(0xf9e2af), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constructor"] = { .fg = X(0x74c7ec), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant"] = { .fg = X(0xfab387), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.builtin"] = { .fg = X(0xfab387), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.character"] = { .fg = X(0x94e2d5), .bg = X(0x000000), .attrib = 0  };
  theme.entries["constant.character.escape"] = { .fg = X(0xf5c2e7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["string"] = { .fg = X(0xa6e3a1), .bg = X(0x000000), .attrib = 0  };
  theme.entries["string.regexp"] = { .fg = X(0xfab387), .bg = X(0x000000), .attrib = 0  };
  theme.entries["string.special"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = 0  };
  theme.entries["comment"] = { .fg = X(0x7f849c), .bg = X(0x000000), .attrib = VMACS_CELL_ITALIC  };
  theme.entries["variable"] = { .fg = X(0xcdd6f4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["variable.parameter"] = { .fg = X(0xeba0ac), .bg = X(0x000000), .attrib = VMACS_CELL_ITALIC  };
  theme.entries["variable.builtin"] = { .fg = X(0xf38ba8), .bg = X(0x000000), .attrib = 0  };
  theme.entries["variable.other.member"] = { .fg = X(0x94e2d5), .bg = X(0x000000), .attrib = 0  };
  theme.entries["label"] = { .fg = X(0x74c7ec), .bg = X(0x000000), .attrib = 0  };
  theme.entries["punctuation"] = { .fg = X(0x9399b2), .bg = X(0x000000), .attrib = 0  };
  theme.entries["punctuation.special"] = { .fg = X(0x89dceb), .bg = X(0x000000), .attrib = 0  };
  theme.entries["keyword"] = { .fg = X(0xcba6f7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["keyword.storage.modifier.ref"] = { .fg = X(0x94e2d5), .bg = X(0x000000), .attrib = 0  };
  theme.entries["keyword.control.conditional"] = { .fg = X(0xcba6f7), .bg = X(0x000000), .attrib = VMACS_CELL_ITALIC  };
  theme.entries["operator"] = { .fg = X(0x89dceb), .bg = X(0x000000), .attrib = 0  };
  theme.entries["function"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = 0  };
  theme.entries["function.macro"] = { .fg = X(0xcba6f7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["tag"] = { .fg = X(0xcba6f7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["attribute"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = 0  };
  theme.entries["namespace"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = VMACS_CELL_ITALIC  };
  theme.entries["special"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading.marker"] = { .fg = X(0xfab387), .bg = X(0x000000), .attrib = VMACS_CELL_BOLD  };
  theme.entries["markup.heading.1"] = { .fg = X(0xb4befe), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading.2"] = { .fg = X(0xcba6f7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading.3"] = { .fg = X(0xa6e3a1), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading.4"] = { .fg = X(0xf9e2af), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading.5"] = { .fg = X(0xf5c2e7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.heading.6"] = { .fg = X(0x94e2d5), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.list"] = { .fg = X(0xcba6f7), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.bold"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = VMACS_CELL_BOLD  };
  theme.entries["markup.italic"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = VMACS_CELL_ITALIC  };
  theme.entries["markup.strikethrough"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.link.url"] = { .fg = X(0xf5e0dc), .bg = X(0x000000), .attrib = VMACS_CELL_UNDERLINE  };
  theme.entries["markup.link.text"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = 0  };
  theme.entries["markup.raw"] = { .fg = X(0xf2cdcd), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diff.plus"] = { .fg = X(0xa6e3a1), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diff.minus"] = { .fg = X(0xf38ba8), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diff.delta"] = { .fg = X(0x89b4fa), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.background"] = { .fg = X(0xcdd6f4), .bg = X(0x1e1e2e), .attrib = 0  };
  theme.entries["ui.linenr"] = { .fg = X(0x45475a), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.linenr.selected"] = { .fg = X(0xb4befe), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.statusline"] = { .fg = X(0xbac2de), .bg = X(0x181825), .attrib = 0  };
  theme.entries["ui.statusline.inactive"] = { .fg = X(0x585b70), .bg = X(0x181825), .attrib = 0  };
  theme.entries["ui.statusline.normal"] = { .fg = X(0x1e1e2e), .bg = X(0xb4befe), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.statusline.insert"] = { .fg = X(0x1e1e2e), .bg = X(0xa6e3a1), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.statusline.select"] = { .fg = X(0x1e1e2e), .bg = X(0xf2cdcd), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.popup"] = { .fg = X(0xcdd6f4), .bg = X(0x313244), .attrib = 0  };
  theme.entries["ui.window"] = { .fg = X(0x11111b), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.help"] = { .fg = X(0x9399b2), .bg = X(0x313244), .attrib = 0  };
  theme.entries["ui.bufferline"] = { .fg = X(0xa6adc8), .bg = X(0x181825), .attrib = 0  };
  theme.entries["ui.bufferline.active"] = { .fg = X(0xcba6f7), .bg = X(0x1e1e2e), .attrib = 0  };
  theme.entries["ui.bufferline.background"] = { .fg = X(0x000000), .bg = X(0x11111b), .attrib = 0  };
  theme.entries["ui.text"] = { .fg = X(0xcdd6f4), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.text.focus"] = { .fg = X(0xcdd6f4), .bg = X(0x313244), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.text.inactive"] = { .fg = X(0x7f849c), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.virtual"] = { .fg = X(0x6c7086), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.virtual.ruler"] = { .fg = X(0x000000), .bg = X(0x313244), .attrib = 0  };
  theme.entries["ui.virtual.indent-guide"] = { .fg = X(0x313244), .bg = X(0x000000), .attrib = 0  };
  theme.entries["ui.virtual.inlay-hint"] = { .fg = X(0x6c7086), .bg = X(0x1e1e2e), .attrib = 0  };
  theme.entries["ui.virtual.jump-label"] = { .fg = X(0xf5e0dc), .bg = X(0x000000), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.selection"] = { .fg = X(0x000000), .bg = X(0x45475a), .attrib = 0  };
  theme.entries["ui.cursor"] = { .fg = X(0x1e1e2e), .bg = X(0xb5a6a8), .attrib = 0  };
  theme.entries["ui.cursor.primary"] = { .fg = X(0x1e1e2e), .bg = X(0xf5e0dc), .attrib = 0  };
  theme.entries["ui.cursor.match"] = { .fg = X(0xfab387), .bg = X(0x000000), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.cursor.primary.normal"] = { .fg = X(0x1e1e2e), .bg = X(0xb4befe), .attrib = 0  };
  theme.entries["ui.cursor.primary.insert"] = { .fg = X(0x1e1e2e), .bg = X(0xa6e3a1), .attrib = 0  };
  theme.entries["ui.cursor.primary.select"] = { .fg = X(0x1e1e2e), .bg = X(0xf2cdcd), .attrib = 0  };
  theme.entries["ui.cursor.normal"] = { .fg = X(0x1e1e2e), .bg = X(0x878ec0), .attrib = 0  };
  theme.entries["ui.cursor.insert"] = { .fg = X(0x1e1e2e), .bg = X(0x7da87e), .attrib = 0  };
  theme.entries["ui.cursor.select"] = { .fg = X(0x1e1e2e), .bg = X(0xb5a6a8), .attrib = 0  };
  theme.entries["ui.cursorline.primary"] = { .fg = X(0x000000), .bg = X(0x2a2b3c), .attrib = 0  };
  theme.entries["ui.highlight"] = { .fg = X(0x000000), .bg = X(0x45475a), .attrib = VMACS_CELL_BOLD  };
  theme.entries["ui.menu"] = { .fg = X(0x9399b2), .bg = X(0x313244), .attrib = 0  };
  theme.entries["ui.menu.selected"] = { .fg = X(0xcdd6f4), .bg = X(0x45475a), .attrib = VMACS_CELL_BOLD  };
  theme.entries["diagnostic.error"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diagnostic.warning"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diagnostic.info"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["diagnostic.hint"] = { .fg = X(0x000000), .bg = X(0x000000), .attrib = 0  };
  theme.entries["error"] = { .fg = X(0xf38ba8), .bg = X(0x000000), .attrib = 0  };
  theme.entries["warning"] = { .fg = X(0xf9e2af), .bg = X(0x000000), .attrib = 0  };
  theme.entries["info"] = { .fg = X(0x89dceb), .bg = X(0x000000), .attrib = 0  };
  theme.entries["hint"] = { .fg = X(0x94e2d5), .bg = X(0x000000), .attrib = 0  };

  return theme;
}
