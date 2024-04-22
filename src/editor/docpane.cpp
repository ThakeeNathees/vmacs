//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "editor.hpp"


DocPane::DocPane() {

  // FIXME: This mess needs to be re-implemented better.
  actions["cursor_up"]      = [&] { if (this->document == nullptr) return; this->document->CursorUp();       EnsureCursorOnView(); };
  actions["cursor_down"]    = [&] { if (this->document == nullptr) return; this->document->CursorDown();     EnsureCursorOnView(); };
  actions["cursor_left"]    = [&] { if (this->document == nullptr) return; this->document->CursorLeft();     EnsureCursorOnView(); };
  actions["cursor_right"]   = [&] { if (this->document == nullptr) return; this->document->CursorRight();    EnsureCursorOnView(); };
  actions["cursor_end"]     = [&] { if (this->document == nullptr) return; this->document->CursorEnd();      EnsureCursorOnView(); };
  actions["cursor_home"]    = [&] { if (this->document == nullptr) return; this->document->CursorHome();     EnsureCursorOnView(); };
  actions["select_right"]   = [&] { if (this->document == nullptr) return; this->document->SelectRight();    EnsureCursorOnView(); };
  actions["select_left"]    = [&] { if (this->document == nullptr) return; this->document->SelectLeft();     EnsureCursorOnView(); };
  actions["select_up"]      = [&] { if (this->document == nullptr) return; this->document->SelectUp();       EnsureCursorOnView(); };
  actions["select_down"]    = [&] { if (this->document == nullptr) return; this->document->SelectDown();     EnsureCursorOnView(); };
  actions["select_home"]    = [&] { if (this->document == nullptr) return; this->document->SelectHome();     EnsureCursorOnView(); };
  actions["select_end"]     = [&] { if (this->document == nullptr) return; this->document->SelectEnd();      EnsureCursorOnView(); };
  actions["insert_space"]   = [&] { if (this->document == nullptr) return; this->document->InsertText(" ");  EnsureCursorOnView(); };
  actions["insert_newline"] = [&] { if (this->document == nullptr) return; this->document->InsertText("\n"); EnsureCursorOnView(); };
  actions["insert_tab"]     = [&] { if (this->document == nullptr) return; this->document->InsertText("\t"); EnsureCursorOnView(); };
  actions["backspace"]      = [&] { if (this->document == nullptr) return; this->document->Backspace();      EnsureCursorOnView(); };
  actions["undo"]           = [&] { if (this->document == nullptr) return; this->document->Undo();           EnsureCursorOnView(); };
  actions["redo"]           = [&] { if (this->document == nullptr) return; this->document->Redo();           EnsureCursorOnView(); };

  auto get_binding = [&] (const char* name) {
    auto it = actions.find(name);
    if (it == actions.end()) return (FuncAction) [] {};
    return it->second;
  };

  std::vector<event_t> events;
  
#define REGISTER_BINDING(mode, key_combination, action_name)       \
  ASSERT(ParseKeyBindingString(events, key_combination), OOPS);    \
  keytree.RegisterBinding(mode, events, get_binding(action_name)); \
  events.clear()

  REGISTER_BINDING("*", "<up>",        "cursor_up");
  REGISTER_BINDING("*", "<down>",      "cursor_down");
  REGISTER_BINDING("*", "<left>",      "cursor_left");
  REGISTER_BINDING("*", "<right>",     "cursor_right");
  REGISTER_BINDING("*", "<home>",      "cursor_home");
  REGISTER_BINDING("*", "<end>",       "cursor_end");
  REGISTER_BINDING("*", "<S-right>",   "select_right");
  REGISTER_BINDING("*", "<S-left>",    "select_left");
  REGISTER_BINDING("*", "<S-up>",      "select_up");
  REGISTER_BINDING("*", "<S-down>",    "select_down");
  REGISTER_BINDING("*", "<S-home>",    "select_home");
  REGISTER_BINDING("*", "<S-end>",     "select_end");
  REGISTER_BINDING("*", "<space>",     "insert_space");
  REGISTER_BINDING("*", "<enter>",     "insert_newline");
  REGISTER_BINDING("*", "<tab>",       "insert_tab");
  REGISTER_BINDING("*", "<backspace>", "backspace");
  REGISTER_BINDING("*", "<C-z>",       "undo");
  REGISTER_BINDING("*", "<C-y>",       "redo");

  // Temproary event.
  REGISTER_BINDING("*", "<C-x>i", "insert_newline");
#undef REGISTER_BINDING

  keytree.SetMode("*");

}


void DocPane::SetDocument(std::shared_ptr<Document> document) {
  this->document = document;
}


void DocPane::HandleEvent(const Event& event) {

  // FIXME:
  if (this->document == nullptr) return;

  switch (event.type) {

    // TODO: Handle.
    case Event::Type::RESIZE:
    case Event::Type::MOUSE:
    case Event::Type::WHEEL:
    case Event::Type::CLOSE:
      break;

    case Event::Type::KEY: {

      bool more = false;

      FuncAction action = keytree.ConsumeEvent(EncodeKeyEvent(event.key), &more);

      // if (action && more) { } // TODO: timeout and perform action.

      if (action) {
        action();
        keytree.ResetCursor();

      } else if (more) {
        // Don't do anything, just wait for the next keystroke and perform on it.

      } else if (!keytree.IsCursorRoot()) {
        // Sequence is not registered, reset and listen from start.
        keytree.ResetCursor();

      } else if (event.key.unicode != 0) {
        char c = (char) event.key.unicode;

        document->InsertText(std::string(1, c));
        EnsureCursorOnView();

        keytree.ResetCursor();
      }

    } break;
  }
}


void DocPane::Draw(DrawBuffer buff, Coord pos, Size area) {

  // FIXME:
  if (this->document == nullptr) return;

  uint8_t color_black         = RgbToXterm(0x000000);
  uint8_t color_cursor        = RgbToXterm(0x1b4f8f);
  uint8_t color_text          = RgbToXterm(0xffffff);
  uint8_t color_sel           = RgbToXterm(0x87898c);
  uint8_t color_bg            = RgbToXterm(0x272829);
  uint8_t color_tab_indicator = RgbToXterm(0x656966);

  int line_count   = document->buffer->GetLineCount();
  Slice selection  = document->cursor.GetSelection();
  int cursor_index = document->cursor.GetIndex();

  // TODO: 
  text_area = area;

  // y is the current relative y coordinate from pos we're drawing.
  for (int y = 0; y < area.height; y++) {

    int line_index = view_start.row + y;
    if (line_index >= line_count) return;

    Slice line = document->buffer->GetLine(line_index);

    // x is the current relative x coordinate from pos we're drawing.
    int x = 0;

    // Index of the current character we're displaying.
    int index = line.start + view_start.col;

    while (x < area.width && index <= line.end) {

      int c = document->buffer->At(index);

      // FIXME: cleanup this mess.
      bool underline = false;
      
      for (auto& diag : document->diagnostics) {
        Coord coord = document->buffer->IndexToCoord(index);

        do {
          if (coord.row < diag.start.row || (coord.row == diag.start.row && coord.col < diag.start.col)) break;
          if (coord.row > diag.end.row || (coord.row == diag.end.row && coord.col >= diag.end.col)) break;
          underline = true;
        } while (false);
      }
      int attrib = 0;
      if (underline) attrib |= VMACS_CELL_UNDERLINE;

      // Color for the cell.
      uint8_t fg = color_text;
      uint8_t bg = color_bg;

      bool in_selection = selection.start <= index && index < selection.end;
      bool in_cursor = index == cursor_index;

      if (in_cursor) bg = color_cursor;
      else if (in_selection) bg = color_sel;

      bool istab = (c == '\t');
      if (isspace(c) || c == '\0') c = ' ';

      if (istab) {
        // TODO: Clean up this mess. If front end is raylib load this codepoint
        // from the glyph.
        // First cell we draw an indecator and draw cursor only in the first cell.
        // 0x2102 : '→'
        SET_CELL(buff, pos.col+x, pos.row+y, 0x2192, color_tab_indicator, bg, attrib);
        x++;

        bg = (in_selection) ? color_sel : color_bg;
        for (int _ = 0; _ < TABSIZE-1; _++) {
          SET_CELL(buff, pos.col+x, pos.row+y, ' ', fg, bg, attrib);
          x++;
        }

      } else {
        SET_CELL(buff, pos.col+x, pos.row+y, c, fg, bg, attrib);
        x++;
      }

      index++;
    }

  }
  return;

}


void DocPane::EnsureCursorOnView() {

  Coord coord = document->cursor.GetCoord();

  if (coord.col <= view_start.col) {
    view_start.col = coord.col;
  } else if (view_start.col + text_area.width <= coord.col) {
    view_start.col = coord.col - text_area.width + 1;
  }

  if (coord.row <= view_start.row) {
    view_start.row = coord.row;
  } else if (view_start.row + text_area.height <= coord.row) {
    view_start.row = coord.row - text_area.height + 1;
  }
}

