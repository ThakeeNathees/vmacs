//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


// Static type initialization.
KeyTree DocPane::keytree;


DocPane::DocPane() : DocPane(std::make_shared<Document>()) {}


DocPane::DocPane(std::shared_ptr<Document> document)
  : Pane(&keytree), document(document) {
  SetMode("*"); // FIXME:
}


bool DocPane::_HandleEvent(const Event& event) {
  if (EventHandler::HandleEvent(event)) return true;

  if (event.type == Event::Type::KEY && event.key.unicode != 0) {
    char c = (char) event.key.unicode;
    document->EnterCharacter(c);
    EnsureCursorOnView();
    ResetCursorBlink();
    return true;

  } else if (event.type == Event::Type::MOUSE) {
    if (event.mouse.button == Event::MouseButton::MOUSE_WHEEL_UP) {
      view_start.row = MAX(0, view_start.row-3); // FIXME: 3 is hardcoded here.
      return true;
    }
    if (event.mouse.button == Event::MouseButton::MOUSE_WHEEL_DOWN) {
      int lines_count = document->buffer->GetLineCount();
      // FIXME: 3 (scroll speed), 2 (visible lines at end) is hardcoded here.
      view_start.row = MIN(MAX(lines_count-2, 0), view_start.row + 3);
      return true;
    }
  }
  return false;
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


void DocPane::OnDocumentChanged() {
  Editor::ReDraw();
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


void DocPane::_Draw(FrameBuffer buff, Position pos, Size area) {
  DrawBuffer(buff, pos, area);
  if (IsActive()) DrawAutoCompletions(buff, pos, area);
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
  // If it's not active we don't draw the cursor.
  if (!IsActive()) *in_cursor = false;
}


void DocPane::DrawBuffer(FrameBuffer buff, Position pos, Size area) {
  ASSERT(this->document != nullptr, OOPS);

  // FIXME: Move this to themes.
  // --------------------------------------------------------------------------
  const Theme* theme = Editor::GetCurrentTheme();
  // TODO: Use ui.cursor for secondary cursor same as selection.
  Style style_text       = theme->GetStyle("ui.text");
  Style style_whitespace = theme->GetStyle("ui.virtual.whitespace");
  Style style_cursor     = theme->GetStyle("ui.cursor.primary");
  Style style_selection  = theme->GetStyle("ui.selection.primary");
  Style style_bg         = theme->GetStyle("ui.background");
  Style style_error      = theme->GetStyle("error");
  Style style_warning    = theme->GetStyle("warning");

  // We draw an indicator for the tab character.
  // 0x2192 : '→'
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

    if (col_delta > 0 && index < line.end) {
      col_delta = TABSIZE_ - col_delta;
      bool in_cursor, in_selection;
      CheckCellStatus(index, &in_cursor, &in_selection);

      // If the line starts at the middle of a tab character or after the end of
      // line draw the rest of the tab (newline will be handled as well).
      while (col_delta--) {
        Style style = style_bg.Apply(style_text);
        if (in_selection) style.ApplyInplace(style_selection);
        // If cursor in the character  we don't want to draw the cursor for
        // the rest of the white spaces.
        SET_CELL(buff, pos.col+x, pos.row+y, ' ', style);
        x++;
      }
      index++;
    }


    // Iterate over the characters in the line and draw.
    while (x < area.width && index <= line.end) {

      // Current cell configuration.
      int c       = document->buffer->At(index);
      Style style = style_bg.Apply(style_text);

      // Get teh syntax highlighting of the character. Note that the background
      // color is fetched from theme (not from syntax highlighter).
      if (highlights.size() > index) {
        style.ApplyInplace(highlights[index]);
      }


      // Check if we're in tab character.
      bool istab = (c == '\t');
      if (istab) {
        style.ApplyInplace(style_whitespace);
        c = tab_indicator;
      } else if (isspace(c) || c == '\0') {
        c = ' ';
      }


      // We get the diagnostics and a lock so the returned pointer will be valid
      // till we're using. If the current character has diagnostic 
      const Diagnostic* diagnos = nullptr;
      std::unique_lock<std::mutex> diagnos_lock = document->GetDiagnosticAt(&diagnos, index);
      if (diagnos != nullptr) {
        style.attrib |= VMACS_CELL_UNDERLINE;
      }


      // Check if the current cell is under cursor or selection, this will override
      // all the themes above.
      bool in_cursor;
      bool in_selection;
      CheckCellStatus(index, &in_cursor, &in_selection);
      if (in_cursor && cursor_blink_show) style.ApplyInplace(style_cursor);
      else if (in_selection) style.ApplyInplace(style_selection);


      // Draw the character finally.
      SET_CELL(buff, pos.col+x, pos.row+y, c, style);
      x++;

      // If it's a tab character we'll draw more white spaces.
      if (istab) {
        int space_count = TABSIZE_ - document->buffer->IndexToColumn(index) % TABSIZE_;
        space_count -= 1; // Since the tab character is drawn already above.
        Style empty = style_bg.Apply(in_selection ? style_selection : style_text);
        for (int _ = 0; _ < space_count; _++) {
          SET_CELL(buff, pos.col+x, pos.row+y, ' ', empty);
          x++;
        }
      }


      // FIXME: Move this mess to DrawDiagnostics().
      // --------------------------------------------------------------------------
      // Draw diagnostic text.
      if (diagnos && index == document->cursors.GetPrimaryCursor().GetIndex()) {
        Style style_diag = style_bg.Apply(diagnos->severity == 1 ? style_error : style_warning);

        // Draw the message.
        int width = MIN(diagnos->message.size(), buff.width);
        int x = buff.width - width;
        DrawTextLine(buff, diagnos->message.c_str(), x, 0, width, style_diag, false);

        // Draw the code + source.
        std::string code_source = diagnos->source + ": " + diagnos->code;
        width = MIN(code_source.size(), buff.width);
        x = buff.width - width;
        DrawTextLine(buff, code_source.c_str(), x, 1, width, style_diag, false);
      }
      // --------------------------------------------------------------------------

      index++;

    } // End of drawing a character.

  } // End of drawing a line.

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
void DocPane::DrawAutoCompletions(FrameBuffer buff, Position docpos, Size docarea) {

  // FIXME: Cleanup this mess.-------------------------------------------------
  const Theme* theme = Editor::GetCurrentTheme();
  Style style_menu          = theme->GetStyle("ui.menu");
  Style style_menu_selected = theme->GetStyle("ui.menu.selected");
  Style style_active_param  = theme->GetStyle("type"); // FIXME: Not the correct one.
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
  int menu_start_index = (BETWEEN(0, document->completion_start_index, document->buffer->GetSize()))
    ? document->completion_start_index
    : document->GetWordBeforeIndex(cursor_index);
  Position menu_start;
  menu_start.col = document->buffer->IndexToColumn(menu_start_index) - view_start.col;
  menu_start.row = document->buffer->IndexToCoord(menu_start_index).line - view_start.row;

  // Current drawing position (relative to the docpos).
  Position pos = {0, 0};

  // ---------------------------------------------------------------------------
  // Drawing auto completion list.
  // ---------------------------------------------------------------------------

  pos = menu_start;
  pos.row += (drawing_bellow_cursor) ? 1 : (-count_dispaynig_items);

  // Adjust the max len depends on available space.
  max_len = MIN(max_len, docarea.width - (pos.col-docpos.col));

  for (int i = 0; i < count_dispaynig_items; i++) {
    auto& item = (*completion_items)[i];
    int icon_index = CLAMP(0, (int)item.kind-1, completion_kind_count);

    Style style = (i == document->completion_selected) ? style_menu_selected : style_menu;

    // TODO: Draw a scroll bar.
    // FIXME: The layout is hardcoded. handle this properly (+2s)

    SET_CELL(
        buff,
        docpos.col + pos.col,
        docpos.row + pos.row,
        completion_kind_nerd_icon[icon_index],
        style);
    SET_CELL(
        buff,
        docpos.col + pos.col + 1,
        docpos.row + pos.row,
        ' ',
        style);
    DrawTextLine(
        buff,
        item.label.c_str(),
        docpos.col + pos.col + 2, // +2 becase we draw the icon above.
        docpos.row + pos.row,
        max_len - 2,              // -2 because we draw the icon above.
        style,
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
  pos = menu_start;
  pos.row += (drawing_bellow_cursor) ? 1 : -1;

  // Adjust the max len depends on available space.
  int label_max_len = MIN(si.label.size(), docarea.width - (pos.col-docpos.col));

  // Draw the label.
  DrawTextLine(
      buff,
      si.label.c_str(),
      docpos.col + pos.col,
      docpos.row + pos.row,
      label_max_len,
      style_menu,
      true);

  // Draw the current parameter highlighted.
  if (si.parameters.size() == 0) return; // No parameter to highlight.
  int param_index = CLAMP(0, signature_helps->active_parameter, si.parameters.size());

  const ParameterInformation& pi = si.parameters[param_index];
  if (pi.label.start < 0 || pi.label.end >= si.label.size() || pi.label.start > pi.label.end) return;

  // Label starts after the width.
  if (docarea.width <= pos.col+pi.label.start-docpos.col) return;

  std::string param_label = si.label.substr(pi.label.start, pi.label.end - pi.label.start + 1);
  label_max_len = MIN(param_label.size(), docarea.width - (pos.col+pi.label.start-docpos.col));

  DrawTextLine(
      buff,
      param_label.c_str(),
      docpos.col + pos.col + pi.label.start,
      docpos.row + pos.row,
      label_max_len,
      style_menu.Apply(style_active_param),
      true);
}

// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------


// The bellow code is common for all the bellow actions so defined here.
#define COMMON_ACTION_END()                                            \
  do {                                                                 \
    self->EnsureCursorOnView(); self->ResetCursorBlink(); return true; \
  } while (false)

bool DocPane::Action_CursorUp(DocPane* self) { self->document->CursorUp(); COMMON_ACTION_END(); }
bool DocPane::Action_CursorDown(DocPane* self) { self->document->CursorDown(); COMMON_ACTION_END(); }
bool DocPane::Action_CursorLeft(DocPane* self) { self->document->CursorLeft(); COMMON_ACTION_END(); }
bool DocPane::Action_CursorRight(DocPane* self) { self->document->CursorRight(); COMMON_ACTION_END(); }
bool DocPane::Action_CursorEnd(DocPane* self) { self->document->CursorEnd(); COMMON_ACTION_END(); }
bool DocPane::Action_CursorHome(DocPane* self) { self->document->CursorHome(); COMMON_ACTION_END(); }
bool DocPane::Action_SelectRight(DocPane* self) { self->document->SelectRight(); COMMON_ACTION_END(); }
bool DocPane::Action_SelectLeft(DocPane* self) { self->document->SelectLeft(); COMMON_ACTION_END(); }
bool DocPane::Action_SelectUp(DocPane* self) { self->document->SelectUp(); COMMON_ACTION_END(); }
bool DocPane::Action_SelectDown(DocPane* self) { self->document->SelectDown(); COMMON_ACTION_END(); }
bool DocPane::Action_SelectHome(DocPane* self) { self->document->SelectHome(); COMMON_ACTION_END(); }
bool DocPane::Action_SelectEnd(DocPane* self) { self->document->SelectEnd(); COMMON_ACTION_END(); }
bool DocPane::Action_AddCursor_down(DocPane* self) { self->document->AddCursorDown(); COMMON_ACTION_END(); }
bool DocPane::Action_AddCursor_up(DocPane* self) { self->document->AddCursorUp(); COMMON_ACTION_END(); }
bool DocPane::Action_InsertSpace(DocPane* self) { self->document->EnterCharacter(' '); COMMON_ACTION_END(); }
bool DocPane::Action_InsertNewline(DocPane* self) { self->document->EnterCharacter('\n'); COMMON_ACTION_END(); }
bool DocPane::Action_InsertTab(DocPane* self) { self->document->EnterCharacter('\t'); COMMON_ACTION_END(); }
bool DocPane::Action_Backspace(DocPane* self) { self->document->Backspace(); COMMON_ACTION_END(); }
bool DocPane::Action_Undo(DocPane* self) { self->document->Undo(); COMMON_ACTION_END(); }
bool DocPane::Action_Redo(DocPane* self) { self->document->Redo(); COMMON_ACTION_END(); }
bool DocPane::Action_TriggerCompletion(DocPane* self) { self->document->TriggerCompletion(); COMMON_ACTION_END(); }
bool DocPane::Action_CycleCompletionList(DocPane* self) { self->document->CycleCompletionList(); self->document->SelectCompletionItem(); COMMON_ACTION_END(); }
bool DocPane::Action_CycleCompletionListReversed(DocPane* self) { self->document->CycleCompletionListReversed(); self->document->SelectCompletionItem(); COMMON_ACTION_END(); }
bool DocPane::Action_Clear(DocPane* self) { self->document->ClearCompletionItems(); self->document->cursors.ClearMultiCursors(); COMMON_ACTION_END(); }

