//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


KeyTree IniWindow::keytree;


IniWindow::IniWindow() : Window(&keytree) {
  SetMode("*"); // FIXME(mess);
}


void IniWindow::_Update() {}


bool IniWindow::_HandleEvent(const Event& event) {
  return EventHandler::HandleEvent(event);
}


void IniWindow::_Draw(FrameBuffer buff, Position pos, Size area) {
  // FIXME: Move this to somewhere else.
  // --------------------------------------------------------------------------
  const Theme* theme = Editor::GetCurrentTheme();
  // TODO: Use ui.cursor for secondary cursor same as selection.
  Style style_text       = theme->GetStyle("ui.text");
  Style style_bg         = theme->GetStyle("ui.background");
  Style style_whitespace = theme->GetStyle("ui.virtual.whitespace");
  Style style_cursor     = theme->GetStyle("ui.cursor.primary");
  Style style_selection  = theme->GetStyle("ui.selection.primary");
  Style style_error      = theme->GetStyle("error");
  Style style_warning    = theme->GetStyle("warning");

  // --------------------------------------------------------------------------
  Style style = style_bg.Apply(style_whitespace);

  //  : 0xe68f 
  // 󰈚 : 0xf021a
  // 󰈭 : 0xf022d 
  //  : 0xf02e
  //  : 0xe22b

  const int w = 30;
  std::string find_files = Utf8UnicodeToString(0xe68f) + " Find Files";
  std::string recent_files = Utf8UnicodeToString(0xf021a) + " Recent Files";
  std::string find_word  = Utf8UnicodeToString(0xf022d) + " Find Word";
  std::string bookmarks  = Utf8UnicodeToString(0xf02e) + " Bookmakrs";
  std::string themes     = Utf8UnicodeToString(0xe22b) + " Themes";

  const int col = 20;
  DrawTextLine(buff, find_files.c_str(), pos.col, pos.row, area.width, style, false);
  DrawTextLine(buff, "Spc f f", pos.col + col, pos.row, area.width, style, false);

  DrawTextLine(buff, recent_files.c_str(), pos.col, pos.row + 2, area.width, style, false);
  DrawTextLine(buff, "Spc f o", pos.col + col, pos.row+2, area.width, style, false);

  DrawTextLine(buff, find_word.c_str(), pos.col, pos.row+4, area.width, style, false);
  DrawTextLine(buff, "Spc f w", pos.col + col, pos.row+4, area.width, style, false);

  DrawTextLine(buff, bookmarks.c_str(), pos.col, pos.row+6, area.width, style, false);
  DrawTextLine(buff, "Spc b m", pos.col + col, pos.row+6, area.width, style, false);

  DrawTextLine(buff, themes.c_str(), pos.col, pos.row+8, area.width, style, false);
  DrawTextLine(buff, "Spc t m", pos.col + col, pos.row+8, area.width, style, false);

  // DrawRectangleFill(buff, pos.col, pos.row, area.width, area.height, s);
}

