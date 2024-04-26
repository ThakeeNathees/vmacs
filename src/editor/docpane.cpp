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
  actions["cursor_up"]      = [&] { if (this->document == nullptr) return; this->document->CursorUp();       EnsureCursorOnView(); ResetCursorBlink(); };
  actions["cursor_down"]    = [&] { if (this->document == nullptr) return; this->document->CursorDown();     EnsureCursorOnView(); ResetCursorBlink(); };
  actions["cursor_left"]    = [&] { if (this->document == nullptr) return; this->document->CursorLeft();     EnsureCursorOnView(); ResetCursorBlink(); };
  actions["cursor_right"]   = [&] { if (this->document == nullptr) return; this->document->CursorRight();    EnsureCursorOnView(); ResetCursorBlink(); };
  actions["cursor_end"]     = [&] { if (this->document == nullptr) return; this->document->CursorEnd();      EnsureCursorOnView(); ResetCursorBlink(); };
  actions["cursor_home"]    = [&] { if (this->document == nullptr) return; this->document->CursorHome();     EnsureCursorOnView(); ResetCursorBlink(); };
  actions["select_right"]   = [&] { if (this->document == nullptr) return; this->document->SelectRight();    EnsureCursorOnView(); ResetCursorBlink(); };
  actions["select_left"]    = [&] { if (this->document == nullptr) return; this->document->SelectLeft();     EnsureCursorOnView(); ResetCursorBlink(); };
  actions["select_up"]      = [&] { if (this->document == nullptr) return; this->document->SelectUp();       EnsureCursorOnView(); ResetCursorBlink(); };
  actions["select_down"]    = [&] { if (this->document == nullptr) return; this->document->SelectDown();     EnsureCursorOnView(); ResetCursorBlink(); };
  actions["select_home"]    = [&] { if (this->document == nullptr) return; this->document->SelectHome();     EnsureCursorOnView(); ResetCursorBlink(); };
  actions["select_end"]     = [&] { if (this->document == nullptr) return; this->document->SelectEnd();      EnsureCursorOnView(); ResetCursorBlink(); };

  actions["add_cursor_down"] = [&] { if (this->document == nullptr) return; this->document->AddCursorDown(); EnsureCursorOnView(); ResetCursorBlink(); };
  actions["add_cursor_up"]   = [&] { if (this->document == nullptr) return; this->document->AddCursorUp();   EnsureCursorOnView(); ResetCursorBlink(); };

  actions["insert_space"]   = [&] { if (this->document == nullptr) return; this->document->EnterCharacter(' ');  EnsureCursorOnView(); ResetCursorBlink(); };
  actions["insert_newline"] = [&] { if (this->document == nullptr) return; this->document->EnterCharacter('\n'); EnsureCursorOnView(); ResetCursorBlink(); };
  actions["insert_tab"]     = [&] { if (this->document == nullptr) return; this->document->EnterCharacter('\t'); EnsureCursorOnView(); ResetCursorBlink(); };
  actions["backspace"]      = [&] { if (this->document == nullptr) return; this->document->Backspace();      EnsureCursorOnView(); ResetCursorBlink(); };
  actions["undo"]           = [&] { if (this->document == nullptr) return; this->document->Undo();           EnsureCursorOnView(); ResetCursorBlink(); };
  actions["redo"]           = [&] { if (this->document == nullptr) return; this->document->Redo();           EnsureCursorOnView(); ResetCursorBlink(); };

  actions["trigger_completion"] = [&] { if (this->document == nullptr) return; this->document->TriggerCompletion(); EnsureCursorOnView(); ResetCursorBlink(); };


  auto get_binding = [this] (const char* name) {
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

  REGISTER_BINDING("*", "<M-down>",    "add_cursor_down");
  REGISTER_BINDING("*", "<M-up>",      "add_cursor_up");

  REGISTER_BINDING("*", "<space>",     "insert_space");
  REGISTER_BINDING("*", "<enter>",     "insert_newline");
  REGISTER_BINDING("*", "<tab>",       "insert_tab");
  REGISTER_BINDING("*", "<backspace>", "backspace");
  REGISTER_BINDING("*", "<C-z>",       "undo");
  REGISTER_BINDING("*", "<C-y>",       "redo");

  REGISTER_BINDING("*", "<C-x><C-k>",  "trigger_completion");

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

        document->EnterCharacter(c);
        EnsureCursorOnView();

        keytree.ResetCursor();
      }

    } break;
  }
}


void DocPane::Update() {
  // Update the cursor blink.
  if (cursor_blink_period > 0) {
    int now = GetElapsedTime();
    if (now - cursor_last_blink >= cursor_blink_period) {
      cursor_last_blink = now;
      cursor_blink_show = !cursor_blink_show;
    }
  }
}


void DocPane::Draw(DrawBuffer buff, Coord pos, Size area) {

  ASSERT(this->document != nullptr, OOPS);

  // Update our text area so we'll know about the viewing area when we're not
  // drawing, needed to ensure the cursors are on the view area, etc.
  text_area = area;

  // FIXME: Move this to themes.
  Color color_red           = 0xff0000;
  Color color_yellow        = 0xcccc2d;
  Color color_black         = 0x000000;
  Color color_text          = 0xffffff;
  Color color_sel           = 0x87898c;
  Color color_bg            = 0x272829;
  Color color_tab_indicator = 0x656966;
  // Color color_cursor     = 0x1b4f8f;
  Color color_cursor_fg = Theme::Get()->entries["ui.cursor.primary"].fg;
  Color color_cursor_bg = Theme::Get()->entries["ui.cursor.primary"].bg;
  color_sel = Theme::Get()->entries["ui.selection"].bg;
  color_bg = Theme::Get()->entries["ui.background"].bg;

  // FIXME: Move this somewhere general.
  // We draw an indicator for the tab character.
  // 0x2102 : '→'
  int tab_indicator = 0x2192;

  int line_count   = document->buffer->GetLineCount();

  // y is the current relative y coordinate from pos we're drawing.
  for (int y = 0; y < area.height; y++) {

    int line_index = view_start.row + y;
    if (line_index >= line_count) break;

    Slice line = document->buffer->GetLine(line_index);

    // x is the current relative x coordinate from pos we're drawing.
    int x = 0;

    // Index of the current character we're displaying.
    int index = line.start + view_start.col;

    while (x < area.width && index <= line.end) {

      // Current cell configuration.
      int c = document->buffer->At(index);
      Color fg = color_text;
      Color bg = color_bg;
      int attrib = 0;

      // We get the diagnostics and a lock so the returned pointer will be valid
      // till we're using.
      const Diagnostic* diagnos = nullptr;
      std::unique_lock<std::mutex> diagnos_lock = document->GetDiagnosticAt(&diagnos, index);

      if (diagnos != nullptr) attrib |= VMACS_CELL_UNDERLINE;

      // Check if current cell is in cursor / selection.
      bool in_selection = false;
      bool in_cursor    = false;
      for (const Cursor& cursor : document->cursors.Get()) {
        if (index == cursor.GetIndex()) in_cursor = true;
        Slice selection = cursor.GetSelection();
        if (selection.start <= index && index < selection.end) {
          in_selection = true;
        }
        if (in_selection && in_cursor) break;
      }

      if (in_cursor && cursor_blink_show) fg = color_cursor_fg,  bg = color_cursor_bg;
      else if (in_selection) bg = color_sel;

      // Handle tab character.
      bool istab = (c == '\t');
      if (isspace(c) || c == '\0') c = ' ';

      if (istab) {
        // First cell we draw an indecator and draw cursor only in the first cell.
        SET_CELL(buff, pos.col+x, pos.row+y, tab_indicator, color_tab_indicator, bg, attrib);
        x++;

        bg = (in_selection) ? color_sel : color_bg;
        for (int _ = 0; _ < TABSIZE-1; _++) {
          SET_CELL(buff, pos.col+x, pos.row+y, ' ', fg, bg, attrib);
          x++;
        }

      } else {

        Style style = { .fg = fg, .bg = bg, .attrib = (uint8_t) attrib };
        auto& highlights = document->syntax.GetHighlights();
        if (highlights.size() > index) style = highlights[index];
        if (in_cursor) style.fg = color_cursor_fg, style.bg = color_cursor_bg;
        // or'ing the bellow attrib for diagnostic underline.
        SET_CELL(buff, pos.col+x, pos.row+y, c, style.fg, bg, style.attrib | attrib);
        x++;
      }

      // FIXME: This is temproary mess.
      // Draw diagnostic text.
      if (diagnos && index == document->cursors.GetPrimaryCursor().GetIndex()) {
        Color color = (diagnos->severity == 1) ? color_red : color_yellow;

        // Draw the message.
        int width = MIN(diagnos->message.size(), buff.width);
        int x = buff.width - width;
        DrawTextLine(buff, diagnos->message.c_str(), x, 0, width, color, color_bg, 0, false);

        // Draw the code + source.
        std::string code_source = diagnos->source + ": " + diagnos->code;
        width = MIN(code_source.size(), buff.width);
        x = buff.width - width;
        DrawTextLine(buff, code_source.c_str(), x, 1, width, color, color_bg, 0, false);
      }

      index++;
    } // End of drawing a character.

  } // End of drawing a line.

  DrawAutoCompletions(buff);

  // Draw a spinning indicator yell it's re-drawn.
  // ⡿ ⣟ ⣯ ⣷ ⣾ ⣽ ⣻ ⢿
  static int curr = 0;
  int icons[] = { 0x287f, 0x28df, 0x28ef, 0x28f7, 0x28fe, 0x28fd, 0x28fb, 0x28bf };
  int icon_count = sizeof icons / sizeof *icons;
  if (curr >= icon_count) curr = 0;
  SET_CELL(buff, 0, area.height-1, icons[curr++], color_text, color_bg, 0);
}


void DocPane::DrawAutoCompletions(DrawBuffer buff) {
  std::vector<CompletionItem>* completion_items = nullptr;
  std::unique_lock<std::mutex> lock_completions = document->GetCompletionItems(&completion_items);
  ASSERT(completion_items != nullptr, OOPS);
  if (completion_items->size() == 0) return;

  // FIXME: Move this.
  Color popup_fg = Theme::Get()->entries["ui.popup"].fg;
  Color popup_bg = Theme::Get()->entries["ui.popup"].bg;


  // TODO: Currently we trigger completion every time a triggering character is
  // pressed, so no need to filter the match ourself. like this:
  //
  //   word_under_cursor = document.GetWordUnderCursor();
  //   for (completion_item : completion_items) {
  //     if (!FuzzyMatch(word_under_cursor, completion_item.text)) {
  //       completion_items.remove(completion_item);
  //     }
  //   }
  //

  // Cursor coordinate.
  Coord cursor_coord = document->cursors.GetPrimaryCursor().GetCoord();
  int cursor_index = document->cursors.GetPrimaryCursor().GetIndex();

  int count_items = completion_items->size();
  int count_lines_above_cursor = cursor_coord.row - view_start.row;
  int count_lines_bellow_cursor = view_start.row + text_area.height - cursor_coord.row - 1;

  ASSERT(count_lines_above_cursor >= 0, OOPS);
  ASSERT(count_lines_bellow_cursor >= 0, OOPS);

  // Determine where we'll be drawgin the popup.
  bool drawing_bellow_cursor = true;
  if (count_items > count_lines_bellow_cursor) {
    drawing_bellow_cursor = (count_lines_bellow_cursor >= count_lines_above_cursor);
  }

  // Determine how many out of those completion items we're displaying.
  int count_dispaynig_items = 0;
  if (drawing_bellow_cursor) {
    count_dispaynig_items = MIN(count_items, count_lines_bellow_cursor);
  } else {
    count_dispaynig_items = MIN(count_items, count_lines_above_cursor);
  }

  // Compute the maximum width for the labels, (not considering the width of the window).
  int max_len = 0;
  for (int i = 0; i < count_dispaynig_items; i++) {
    int item_len = Utf8Strlen((*completion_items)[i].label.c_str());
    max_len = MAX(max_len, item_len);
  }

  int word_index = document->GetWordBeforeIndex(cursor_index);
  Coord coord = document->buffer->IndexToCoord(word_index);

  coord.row += (drawing_bellow_cursor) ? 1 : -1;
  for (auto& item : *completion_items) {
    DrawTextLine(
        buff,
        item.label.c_str(),
        coord.col,
        coord.row,
        max_len,
        popup_fg,
        popup_bg,
        0,
        true);
    coord.row += (drawing_bellow_cursor) ? 1 : -1;
  }
}


void DocPane::ResetCursorBlink() {
  cursor_blink_show = true;
  cursor_last_blink = GetElapsedTime();
}


void DocPane::EnsureCursorOnView() {

  Coord coord = document->cursors.GetPrimaryCursor().GetCoord();

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
