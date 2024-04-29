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

  // Fixme: TEMP command:
  actions["clear_completion"] = [&] { if (this->document == nullptr) return; this->document->ClearCompletionItems(); EnsureCursorOnView(); ResetCursorBlink(); };
  
  actions["cycle_completion_list"] = [&] {
    if (this->document == nullptr) return;
    this->document->CycleCompletionList();
    this->document->SelectCompletionItem();
    EnsureCursorOnView(); ResetCursorBlink(); };
  actions["cycle_completion_list_reversed"] = [&] {
    if (this->document == nullptr) return;
    this->document->CycleCompletionListReversed();
    this->document->SelectCompletionItem();
    EnsureCursorOnView(); ResetCursorBlink(); };


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
  REGISTER_BINDING("*", "<C-n>",  "cycle_completion_list");
  REGISTER_BINDING("*", "<C-p>",  "cycle_completion_list_reversed");
  REGISTER_BINDING("*", "<esc>",  "clear_completion");
  // REGISTER_BINDING("*", "<tab>",   "cycle_completion_list");

  // Temproary event.
  REGISTER_BINDING("*", "<C-x>i", "insert_newline");
#undef REGISTER_BINDING

  keytree.SetMode("*");

}


void DocPane::SetDocument(std::shared_ptr<Document> document) {
  this->document = document;
}


void DocPane::HandleEvent(const Event& event) {
  if (this->document == nullptr) return;

  switch (event.type) {

    case Event::Type::RESIZE:
    case Event::Type::MOUSE:
    case Event::Type::WHEEL:
    case Event::Type::CLOSE:
      break;

    case Event::Type::KEY: {

      bool more = false;

      FuncAction action = keytree.ConsumeEvent(EncodeKeyEvent(event.key), &more);
      // if (action && more) { } // TODO: Timeout and perform action.

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
        ResetCursorBlink();

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


void DocPane::ResetCursorBlink() {
  cursor_blink_show = true;
  cursor_last_blink = GetElapsedTime();
}


void DocPane::EnsureCursorOnView() {

  // Note that the col of Coord is not the view column, and cursor.GetColumn()
  // will return the column it wants go to and not the column it actually is.
  const Cursor& cursor = document->cursors.GetPrimaryCursor();
  int row = cursor.GetCoord().line;
  int col = document->buffer->IndexToColumn(cursor.GetIndex());

  if (col <= view_start.col) {
    view_start.col = col;
  } else if (view_start.col + text_area.width <= col) {
    view_start.col = col - text_area.width + 1;
  }

  if (row <= view_start.row) {
    view_start.row = row;
  } else if (view_start.row + text_area.height <= row) {
    view_start.row = row - text_area.height + 1;
  }
}


void DocPane::Draw(FrameBuffer buff, Position pos, Size area) {
  DrawBuffer(buff, pos, area);
  DrawAutoCompletions(buff, pos);

  // Draw a spinning indicator yell it's re-drawn.
  // ⡿ ⣟ ⣯ ⣷ ⣾ ⣽ ⣻ ⢿
  static int curr = 0;
  int icons[] = { 0x287f, 0x28df, 0x28ef, 0x28f7, 0x28fe, 0x28fd, 0x28fb, 0x28bf };
  int icon_count = sizeof icons / sizeof *icons;
  if (curr >= icon_count) curr = 0;
  SET_CELL(buff, 0, area.height-1, icons[curr++], 0xffffff, 0, 0); // FIXME:

}


void DocPane::CheckCellStatus(int index, bool* in_cursor, bool* in_selection) {
  ASSERT(in_cursor != nullptr, OOPS);
  ASSERT(in_selection != nullptr, OOPS);
  *in_cursor = false;
  *in_selection = false;
  for (const Cursor& cursor : document->cursors.Get()) {
    if (index == cursor.GetIndex()) *in_cursor = true;
    Slice selection = cursor.GetSelection();
    if (selection.start <= index && index < selection.end) {
      *in_selection = true;
    }
  }
}


void DocPane::DrawBuffer(FrameBuffer buff, Position pos, Size area) {
  ASSERT(this->document != nullptr, OOPS);

  // FIXME: Move this to themes.
  // --------------------------------------------------------------------------
  const Theme* theme = Global::GetCurrentTheme();
  Color color_red           = 0xff0000;
  Color color_yellow        = 0xcccc2d;
  Color color_black         = 0x000000;
  Color color_text          = 0xffffff;
  Color color_tab_indicator = 0x656966;
  const Style style_default = {.fg = 0, .bg = 0xffffff, .attrib = 0};
  Style style;
  #define GET_STYLE(style_name) \
    (theme->GetStyle(&style, style_name) ? style : style_default)
  Color color_cursor_fg     = GET_STYLE("ui.cursor.primary").fg;
  Color color_cursor_bg     = GET_STYLE("ui.cursor.primary").bg;
  Color color_sel           = GET_STYLE("ui.selection").bg;
  Color color_bg            = GET_STYLE("ui.background").bg;
  // FIXME: Move this somewhere general.
  // We draw an indicator for the tab character.
  // 0x2102 : '→'
  int tab_indicator = 0x2192;
  // --------------------------------------------------------------------------

  // Update our text area so we'll know about the viewing area when we're not
  // drawing, needed to ensure the cursors are on the view area, etc.
  text_area = area;

  int line_count = document->buffer->GetLineCount(); // Total lines in the buffer.
  const std::vector<Style>& highlights = document->syntax.GetHighlights();

  // y is the current relative y coordinate from pos we're drawing.
  for (int y = 0; y < area.height; y++) {

    // Check if we're done writing the lines (buffer end reached).
    int line_index = view_start.row + y;
    if (line_index >= line_count) break;

    // Current line we're drawing.
    Slice line = document->buffer->GetLine(line_index);

    // x is the current relative x coordinate from 'pos' we're drawing.
    int x = 0;

    // If the current view_start column is either at at the middle of a tab
    // character or after the EOL of the current line. In that case we draw the
    // rest half of the tab character and start from the next character. we'll
    // use this variable to keep track of how many columns have trimmed.
    //
    // col_delta will be the number of spaces before the current view column,
    // so we'll draw the rest of the spaces which is tabsize - col_delta;
    int col_delta = 0;
    int index = document->buffer->ColumnToIndex(view_start.col, line_index, &col_delta);

    if (col_delta > 0) {
      col_delta = TABSIZE_ - col_delta;
      bool in_cursor, in_selection;
      CheckCellStatus(index, &in_cursor, &in_selection);

      // If the line starts at the middle of a tab character or after the end of
      // line draw the rest of the tab (newline will be handled as well).
      while (col_delta--) {
        // If cursor in the character  we don't want to draw the cursor for
        // the rest of the white spaces.
        int bg = (in_selection) ? color_sel : color_bg;
        SET_CELL(buff, pos.col+x, pos.row+y, ' ', color_text, color_bg, 0);
        x++;
      }
      index++;
    }


    // Iterate over the characters in the line and draw.
    while (x < area.width && index <= line.end) {

      // Current cell configuration.
      int c      = document->buffer->At(index);
      Color fg   = color_text;
      Color bg   = color_bg;
      int attrib = 0;

      // Get teh syntax highlighting of the character. Note that the background
      // color is fetched from theme (not from syntax highlighter).
      if (highlights.size() > index) {
        style = highlights[index];
        fg = style.fg;
        attrib |= style.attrib;
      }


      // Check if we're in tab character.
      bool istab = (c == '\t');
      if (istab) {
        c  = tab_indicator;
        fg = color_tab_indicator;
      } else if (isspace(c) || c == '\0') {
        c = ' ';
      }


      // We get the diagnostics and a lock so the returned pointer will be valid
      // till we're using. If the current character has diagnostic 
      const Diagnostic* diagnos = nullptr;
      std::unique_lock<std::mutex> diagnos_lock = document->GetDiagnosticAt(&diagnos, index);
      if (diagnos != nullptr) attrib |= VMACS_CELL_UNDERLINE;


      // Check if the current cell is under cursor or selection, this will override
      // all the themes above.
      bool in_cursor;
      bool in_selection;
      CheckCellStatus(index, &in_cursor, &in_selection);
      if (in_cursor && cursor_blink_show) fg = color_cursor_fg,  bg = color_cursor_bg;
      else if (in_selection) bg = color_sel;


      // Draw the character finally.
      SET_CELL(buff, pos.col+x, pos.row+y, c, fg, bg, attrib);
      x++;

      // If it's a tab character we'll draw more white spaces.
      if (istab) {
        int space_count = TABSIZE_ - document->buffer->IndexToColumn(index) % TABSIZE_;
        space_count -= 1; // Since the tab character is drawn already above.
        bg = (in_selection) ? color_sel : color_bg; // Don't draw cursor on all the cells.
        for (int _ = 0; _ < space_count; _++) {
          SET_CELL(buff, pos.col+x, pos.row+y, ' ', fg, bg, attrib);
          x++;
        }
      }


      // FIXME: Move this mess to DrawDiagnostics().
      // --------------------------------------------------------------------------
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
      // --------------------------------------------------------------------------

      index++;

    } // End of drawing a character.

  } // End of drawing a line.

}


void DocPane::ComputeCompletionPosition() {

}


// FIXME: Move this, add color value.
static const int completion_kind_nerd_icon[] = {
  0xea93, //  Text          
  0xea8c, //  Method        
  0xf0295, //  Function     󰊕
  0xea8c, //  Constructor   
  0xeb5f, //  Field         
  0xea88, //  Variable      
  0xeb5b, //  Class         
  0xeb61, //  Interface     
  0xea8b, //  Module        
  0xeb65, //  Property      
  0xea96, //  Unit          
  0xea95, //  Value         
  0xea95, //  Enum          
  0xeb62, //  Keyword       
  0xeb66, //  Snippet       
  0xeb5c, //  Color         
  0xea7b, //  File          
  0xea94, //  Reference     
  0xea83, //  Folder        
  0xea95, //  EnumMember    
  0xeb5d, //  Constant      
  0xea91, //  Struct        
  0xea86, //  Event         
  0xeb64, //  Operator      
  0xea92, //  TypeParameter 
};
static const int completion_kind_count = sizeof completion_kind_nerd_icon / sizeof *completion_kind_nerd_icon;


// FIXME: This method is not clean.
void DocPane::DrawAutoCompletions(FrameBuffer buff, Position docpos) {

  // FIXME: Cleanup this mess.-------------------------------------------------
  const Theme* theme = Global::GetCurrentTheme();
  Style style;
  Style style_default = {.fg = 0, .bg = 0xffffff, .attrib = 0};
#define GET_STYLE(style_name) \
  (theme->GetStyle(&style, style_name) ? style : style_default)
  Color popup_fg     = GET_STYLE("ui.popup").fg;
  Color popup_bg     = GET_STYLE("ui.popup").bg;
  Color param_active = GET_STYLE("type").fg;
  //---------------------------------------------------------------------------


  // Get the completion items.
  std::vector<CompletionItem>* completion_items = nullptr;
  std::unique_lock<std::mutex> lock_completions = document->GetCompletionItems(&completion_items);
  ASSERT(completion_items != nullptr, OOPS);

  // Get signature help.
  SignatureItems* signature_helps;
  std::unique_lock<std::mutex> lock_signature = document->GetSignatureHelp(&signature_helps);
  ASSERT(signature_helps != nullptr, OOPS);

  if (completion_items->size() == 0 &&
      signature_helps->signatures.size() == 0) return;

  // ---------------------------------------------------------------------------
  // Determine the size and the position to draw the list.
  // ---------------------------------------------------------------------------

  // Cursor coordinate.
  Coord cursor_coord = document->cursors.GetPrimaryCursor().GetCoord();
  int cursor_index = document->cursors.GetPrimaryCursor().GetIndex();

  // FIXME: The value is hardcoded (without a limit, the dropdown takes all the spaces).
  int count_items               = MIN(20, completion_items->size());
  int count_lines_above_cursor  = cursor_coord.line - view_start.row;
  int count_lines_bellow_cursor = view_start.row + text_area.height - cursor_coord.line - 1;

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

  // Prepare the coord for drawing the completions.
  int popup_start_index = (BETWEEN(0, document->completion_start_index, document->buffer->GetSize()))
    ? document->completion_start_index
    : document->GetWordBeforeIndex(cursor_index);
  Position popup_start;
  popup_start.col = document->buffer->IndexToColumn(popup_start_index) - view_start.col;
  popup_start.row = document->buffer->IndexToCoord(popup_start_index).line - view_start.row;

  // Current drawing position (relative to the docpos).
  Position pos = {0, 0};

  // ---------------------------------------------------------------------------
  // Drawing auto completion list.
  // ---------------------------------------------------------------------------

  pos = popup_start;
  pos.row += (drawing_bellow_cursor) ? 1 : (-count_dispaynig_items);

  for (int i = 0; i < count_dispaynig_items; i++) {
    auto& item = (*completion_items)[i];
    int icon_index = CLAMP(0, (int)item.kind-1, completion_kind_count);

    // Highlight selected item. TODO: background color.
    Color bg = (i == document->completion_selected) ? 0xff0000 : popup_bg;

    // TODO: Draw a scroll bar.
    // FIXME: The layout is hardcoded.
    SET_CELL(
        buff,
        docpos.col + pos.col,
        docpos.row + pos.row,
        completion_kind_nerd_icon[icon_index],
        popup_fg, bg, 0);
    SET_CELL(
        buff,
        docpos.col + pos.col + 1,
        docpos.row + pos.row,
        ' ',
        popup_fg, bg, 0);
    DrawTextLine(
        buff,
        item.label.c_str(),
        docpos.col + pos.col + 2, // + becase we draw the icon above.
        docpos.row + pos.row,
        max_len,
        popup_fg,
        bg,
        0,
        true);
    pos.row++;
  }

  // --------------------------------------------------------------------------
  // Drawing the signature help.
  // --------------------------------------------------------------------------

  // Draw Signature help in the other direction of the dropdown.
  if (signature_helps->signatures.size() == 0) return;

  int signature_size = signature_helps->signatures.size();
  int signature_index = CLAMP(0, signature_helps->active_signature, signature_size);
  const SignatureInformation& si = signature_helps->signatures[signature_index];

  // TODO: This will draw the signature label and draw the parameter on top of it.
  // pre mark the values when we recieve the signature help info from the other thread
  // and just draw it here.

  // TODO: Highlight current parameter and draw documentation about the parameter.
  // Prepare the coord for drawing the signature help.
  drawing_bellow_cursor = count_items == 0 || !drawing_bellow_cursor;

  // Prepare the coord for drawing the completions.
  pos = popup_start;
  pos.row += (drawing_bellow_cursor) ? 1 : -1;

  DrawTextLine(
      buff,
      si.label.c_str(),
      docpos.col + pos.col,
      docpos.row + pos.row,
      si.label.size(),
      popup_fg,
      popup_bg,
      0,
      true);

  // Draw the current parameter highlighted.
  if (si.parameters.size() == 0) return; // No parameter to highlight.
  int param_index = CLAMP(0, signature_helps->active_parameter, si.parameters.size());
  const ParameterInformation& pi = si.parameters[param_index];
  if (pi.label.start < 0 || pi.label.end >= si.label.size() || pi.label.start > pi.label.end) return;
  std::string param_label = si.label.substr(pi.label.start, pi.label.end - pi.label.start + 1);
  DrawTextLine(
      buff,
      param_label.c_str(),
      docpos.col + pos.col + pi.label.start,
      docpos.row + pos.row,
      param_label.size(),
      param_active,
      popup_bg,
      0,
      true);

}

