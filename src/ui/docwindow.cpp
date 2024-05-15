//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


DocumentWindow::DocumentWindow() : DocumentWindow(std::make_shared<Document>()) {}


DocumentWindow::DocumentWindow(std::shared_ptr<Document> document_)
  : document(document_), cursors_backup(document->buffer.get()) {

  cursors_backup = document->cursors;
  document->RegisterListener(this);

  // FIXME: Get the default mode from the config.
  SetMode("insert");
}


DocumentWindow::~DocumentWindow() {
  document->UnRegisterListener(this);
}


Window::Type DocumentWindow::GetType() const {
  return Type::DOCUMENT;
}


std::shared_ptr<Document> DocumentWindow::GetDocument() const {
  return document;
}


bool DocumentWindow::_HandleEvent(const Event& event) {
  if (event.type == Event::Type::KEY && event.key.unicode != 0) {
    if (GetMode() == "insert") { // FIXME: Modename is hardcoded.
      char c = (char) event.key.unicode;
      document->EnterCharacter(c);
      EnsureCursorOnView();
      ResetCursorBlink();
      return true;
    }

  } else if (event.type == Event::Type::MOUSE) {

    // TODO: These are hardcoded values.
    const int scroll_speed = 3; // 3 lines when scrolling.
    const int visible_end  = 2; // 2 lines visible at the bottom after scroll.

    if (event.mouse.button == Event::MouseButton::MOUSE_WHEEL_UP) {
      view_start.row = MAX(0, view_start.row-scroll_speed);
      return true;
    }
    if (event.mouse.button == Event::MouseButton::MOUSE_WHEEL_DOWN) {
      int lines_count = document->buffer->GetLineCount();
      view_start.row = MIN(MAX(lines_count-visible_end, 0), view_start.row + scroll_speed);
      return true;
    }
  }
  return false;
}


void DocumentWindow::_Update() {
  // Update the cursor blink.
  if (cursor_blink_period > 0) {
    int now = GetElapsedTime();
    if (now - cursor_last_blink >= cursor_blink_period) {
      cursor_last_blink = now;
      cursor_blink_show = !cursor_blink_show;
      Editor::ReDraw();
    }
  }
}


void DocumentWindow::OnDocumentChanged() {
  Editor::ReDraw();
}


void DocumentWindow::OnFocusChanged(bool focus) {

  ResetCursorBlink();
  document->ClearCompletionItems();

  // If we lost focus, take a backup of the cursors.
  if (!focus) {
    cursors_backup = document->GetCursors();
  } else {
    document->SetCursors(cursors_backup);
    EnsureCursorOnView();
  }
}


void DocumentWindow::ResetCursorBlink() {
  cursor_blink_show = true;
  cursor_last_blink = GetElapsedTime();
}


void DocumentWindow::EnsureCursorOnView() {

  // Note that the col of Coord is not the view column, and cursor.GetColumn()
  // will return the column it wants go to and not the column it actually is.
  const Cursor& cursor = document->cursors.GetPrimaryCursor();
  int lines_count = document->buffer->GetLineCount();
  int row = cursor.GetCoord().line;
  int col = document->buffer->IndexToColumn(cursor.GetIndex());

  int scrolloff = GetConfig().scrolloff;
  scrolloff = MAX(0, scrolloff);
  scrolloff = CLAMP(0, scrolloff, buffer_area.height / 2);

  if (col <= view_start.col) {
    view_start.col = col;
  } else if (view_start.col + buffer_area.width <= col) {
    view_start.col = col - MAX(0, buffer_area.width - 1);
  }

  if ((row - scrolloff) <= view_start.row) {
    view_start.row = MAX(0, row - scrolloff);
  } else if (view_start.row + buffer_area.height <= (row + scrolloff)) {
    view_start.row = row + scrolloff - MAX(0, buffer_area.height - 1);
  }
}


std::unique_ptr<Window> DocumentWindow::Copy() const {
  std::unique_ptr<DocumentWindow> ret = std::make_unique<DocumentWindow>(*this);
  ret->cursors_backup = this->document->cursors;
  return std::move(ret);
}


void DocumentWindow::JumpTo(const Coord& coord) {

  Buffer* buff = document->buffer.get();
  ASSERT(buff != nullptr, OOPS);

  int index = 0;
  if (buff->IsValidCoord(coord, &index)) {
    document->cursors.ClearMultiCursors();
    document->cursors.ClearSelections();

    Cursor& cursor = document->cursors.GetPrimaryCursor();
    cursor.SetIndex(index);
    cursors_backup = document->cursors;
  }

  // TODO: Make sure the cursor is at the middle of the view.
  EnsureCursorOnView();
}


void DocumentWindow::_Draw(FrameBuffer& buff, Position pos, Area area) {

  if (Editor::GetConfig().show_linenum) {
    int width = DrawLineNums(buff, pos, area);
    pos.x      += width;
    area.width -= width;
  }

  DrawBuffer(buff, pos, area);
  if (IsActive()) DrawAutoCompletions(pos);
}


int DocumentWindow::DrawLineNums(FrameBuffer& buff, Position pos, Area area) {

  // TODO: Draw selected line in a different color.

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();
  const Config& config = Editor::GetConfig();

  // FIXME: Move this to themes.
  // --------------------------------------------------------------------------
  // TODO: Use ui.cursor for secondary cursor same as selection.
  Style style_text       = theme.GetStyle("ui.text");
  Style style_bg         = theme.GetStyle("ui.background");
  Style style_linenr     = theme.GetStyle("ui.linenr");
  Style style            = style_bg.Apply(style_text).Apply(style_linenr);
  // --------------------------------------------------------------------------

  int line_count = document->buffer->GetLineCount(); // Total lines in the buffer.

  // Calculate the width of the line gutter.
  const int end_line = MIN(view_start.row + area.height, line_count);
  const int margin_right = 1; // Leaving 1 space right for the buffer.
  const int width = MAX(3, std::to_string(end_line).size()) + margin_right;

  // y is the current relative y coordinate from pos we're drawing.
  for (int y = 0; y < area.height; y++) {

    // Check if we're done writing the lines (buffer end reached).
    int line_index = view_start.row + y;
    if (line_index >= line_count) break;

    // Add spacing before the number to make all the numbers align right.
    std::string linenr = std::to_string(line_index+1);
    if (linenr.size() < (width - margin_right)) {
      linenr = std::string(width - margin_right - linenr.size(), ' ') + linenr;
    }

    DrawTextLine(buff, linenr.c_str(), Position(pos.x, pos.y+y), (width-margin_right), style, icons, true);
  }

  return width;
}


void DocumentWindow::DrawBuffer(FrameBuffer& buff, Position pos, Area area) {
  ASSERT(this->document != nullptr, OOPS);

  buffer_area = area;

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();
  const Config& config = Editor::GetConfig();

  // FIXME: Move this to themes.
  // --------------------------------------------------------------------------
  // TODO: Use ui.cursor for secondary cursor same as selection.
  Style style_text       = theme.GetStyle("ui.text");
  Style style_whitespace = theme.GetStyle("ui.virtual.whitespace");
  Style style_cursor     = theme.GetStyle("ui.cursor.primary");
  Style style_selection  = theme.GetStyle("ui.selection.primary");
  Style style_bg         = theme.GetStyle("ui.background");
  Style style_error      = theme.GetStyle("error");
  Style style_warning    = theme.GetStyle("warning");
  // --------------------------------------------------------------------------

  int whitespace_tag = icons.whitespace_tab;

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
      col_delta = config.tabsize - col_delta;
      bool in_cursor, in_selection;
      CheckCellStatusForDrawing(index, &in_cursor, &in_selection);

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
        c = whitespace_tag;
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
      CheckCellStatusForDrawing(index, &in_cursor, &in_selection);
      if (in_cursor && cursor_blink_show) style.ApplyInplace(style_cursor);
      else if (in_selection) style.ApplyInplace(style_selection);


      // Draw the character finally.
      SET_CELL(buff, pos.col+x, pos.row+y, c, style);
      x++;

      // If it's a tab character we'll draw more white spaces.
      if (istab) {
        int space_count = config.tabsize - document->buffer->IndexToColumn(index) % config.tabsize;
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
        DrawTextLine(buff, diagnos->message.c_str(), Position(x, 0), width, style_diag, icons, false);

        // Draw the code + source.
        std::string code_source = diagnos->source + ": " + diagnos->code;
        width = MIN(code_source.size(), buff.width);
        x = buff.width - width;
        DrawTextLine(buff, code_source.c_str(), Position(x, 1), width, style_diag, icons, false);
      }
      // --------------------------------------------------------------------------

      index++;

    } // End of drawing a character.

  } // End of drawing a line.

}


void DocumentWindow::DrawAutoCompletions(Position pos) {

  const Icons& icons = Editor::GetIcons();
  const Theme& theme = Editor::GetTheme();
  Ui* ui = GETUI();

  // FIXME: Cleanup this mess.-------------------------------------------------
  Style style_menu          = theme.GetStyle("ui.menu");
  Style style_menu_selected = theme.GetStyle("ui.menu.selected");
  Style style_active_param  = theme.GetStyle("type"); // FIXME: Not the correct one.
  style_menu_selected = style_menu.Apply(style_menu_selected); // ui.menu.selected doesn't have bg.
  //---------------------------------------------------------------------------

  const int completion_kind_count = sizeof icons.completion_kind / sizeof * icons.completion_kind;

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

  Area area = Editor::Singleton()->GetDrawArea();

  // Cursor coordinate.
  Coord cursor_coord = document->cursors.GetPrimaryCursor().GetCoord();
  int cursor_index = document->cursors.GetPrimaryCursor().GetIndex();

  // FIXME: The value is hardcoded (without a limit, the dropdown takes all the spaces).
  int count_items               = MIN(20, completion_items->size());
  int count_lines_above_cursor  = cursor_coord.line - view_start.row;
  int count_lines_bellow_cursor = view_start.row + area.height - cursor_coord.line - 1;

  // If we've scrolled, the values become < 0, and we aren't drawing the completion.
  if (count_lines_above_cursor < 0 || count_lines_bellow_cursor < 0) return;

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

  // Compute the maximum width for the labels this does not incudes the padding and
  // icons which we'll add later bellow.
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

  // The draw position of the completion list.
  Position drawpos;

  // ---------------------------------------------------------------------------
  // Drawing auto completion list.
  // ---------------------------------------------------------------------------

  drawpos = Position(pos.x + menu_start.x, pos.y + menu_start.y);
  drawpos.y += (drawing_bellow_cursor) ? 1 : (-count_dispaynig_items);

  // Adding 4 because  1 for icons, 2 for padding at both edge, 1 for spacing
  // between icon and the label.
  max_len += 4;

  FrameBuffer buff_compl = Editor::NewFrameBuffer(Area(max_len, count_dispaynig_items));
  Position curr(0, 0); // Draw position relative to the buffer overlay.

  for (int i = 0; i < count_dispaynig_items; i++) {
    auto& item = (*completion_items)[i];
    int icon_index = CLAMP(0, (int)item.kind-1, completion_kind_count);

    Style style = (i == document->completion_selected) ? style_menu_selected : style_menu;

    // TODO: Draw a scroll bar.

    std::string completion_icon = Utf8UnicodeToString(icons.completion_kind[icon_index]);
    std::string completion_item_line = " " + completion_icon + " " + item.label + " ";
    DrawTextLine(
        buff_compl,
        completion_item_line.c_str(),
        curr,
        max_len,
        style,
        icons,
        true);
    curr.row++;
  }

  ui->PushOverlay(drawpos, std::move(buff_compl));

  // --------------------------------------------------------------------------
  // Drawing the signature help.
  // --------------------------------------------------------------------------

  // Draw Signature help in the other direction of the dropdown.
  if (signature_helps->signatures.size() == 0) return;

  int signature_size = signature_helps->signatures.size();
  int signature_index = CLAMP(0, signature_helps->active_signature, signature_size);
  const SignatureInformation& si = signature_helps->signatures[signature_index];

  int label_size = si.label.size();
  FrameBuffer buff_label = Editor::NewFrameBuffer(Area(label_size, 1));

  // This do block exists so we can jump out of the block to the end immediately
  // like goto with break statement.
  do {
    // Defining return to ? to enforce not to use return insde this do block.
    #define return ?

    // TODO: This will draw the signature label and draw the parameter on top of it.
    // pre mark the values when we recieve the signature help info from the other thread
    // and just draw it here.

    // TODO: Highlight current parameter and draw documentation about the parameter.
    // Prepare the coord for drawing the signature help.
    drawing_bellow_cursor = count_items == 0 || !drawing_bellow_cursor;

    // Prepare the coord for drawing the completions.
    drawpos = Position(pos.x + menu_start.x, pos.y + menu_start.y);
    drawpos.row += (drawing_bellow_cursor) ? 1 : -1;

    DrawTextLine(
      buff_label,
      si.label.c_str(),
      Position(0, 0),
      label_size,
      style_menu,
      icons,
      true);

    // Draw the current parameter highlighted.
    if (si.parameters.size() == 0) break; // No parameter to highlight.
    int param_index = CLAMP(0, signature_helps->active_parameter, si.parameters.size());

    const ParameterInformation& pi = si.parameters[param_index];
    if (pi.label.start < 0 || pi.label.end >= si.label.size() || pi.label.start > pi.label.end) break;

    std::string param_label = si.label.substr(pi.label.start, pi.label.end - pi.label.start + 1);
    label_size = param_label.size();

    DrawTextLine(
      buff_label,
      param_label.c_str(),
      Position(pi.label.start, 0),
      label_size,
      style_menu.Apply(style_active_param),
      icons,
      true);

    #undef return
  } while (false);

  ui->PushOverlay(drawpos, std::move(buff_label));
}


void DocumentWindow::CheckCellStatusForDrawing(int index, bool* in_cursor, bool* in_selection) {
  ASSERT(in_cursor != nullptr, OOPS);
  ASSERT(in_selection != nullptr, OOPS);
  *in_cursor = false;
  *in_selection = false;

  // If it's not active we don't draw the cursor or selection.
  if (!IsActive()) return;

  for (const Cursor& cursor : document->cursors.Get()) {
    if (index == cursor.GetIndex()) *in_cursor = true;
    Slice selection = cursor.GetSelection();
    if (selection.start <= index && index < selection.end) {
      *in_selection = true;
    }
  }
}




// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------


// The bellow code is common for all the bellow actions so defined here.
#define COMMON_ACTION_END()                                            \
  do {                                                                 \
    self->EnsureCursorOnView(); self->ResetCursorBlink(); return true; \
  } while (false)

bool DocumentWindow::Action_NormalMode(DocumentWindow* self) { self->SetMode("normal"); COMMON_ACTION_END(); }
bool DocumentWindow::Action_InsertMode(DocumentWindow* self) { self->SetMode("insert"); COMMON_ACTION_END(); }
bool DocumentWindow::Action_CursorUp(DocumentWindow* self) { self->document->CursorUp(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_CursorDown(DocumentWindow* self) { self->document->CursorDown(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_CursorLeft(DocumentWindow* self) { self->document->CursorLeft(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_CursorRight(DocumentWindow* self) { self->document->CursorRight(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_CursorEnd(DocumentWindow* self) { self->document->CursorEnd(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_CursorHome(DocumentWindow* self) { self->document->CursorHome(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_SelectRight(DocumentWindow* self) { self->document->SelectRight(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_SelectLeft(DocumentWindow* self) { self->document->SelectLeft(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_SelectUp(DocumentWindow* self) { self->document->SelectUp(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_SelectDown(DocumentWindow* self) { self->document->SelectDown(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_SelectHome(DocumentWindow* self) { self->document->SelectHome(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_SelectEnd(DocumentWindow* self) { self->document->SelectEnd(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_AddCursor_down(DocumentWindow* self) { self->document->AddCursorDown(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_AddCursor_up(DocumentWindow* self) { self->document->AddCursorUp(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_InsertSpace(DocumentWindow* self) { self->document->EnterCharacter(' '); COMMON_ACTION_END(); }
bool DocumentWindow::Action_InsertNewline(DocumentWindow* self) { self->document->EnterCharacter('\n'); COMMON_ACTION_END(); }
bool DocumentWindow::Action_InsertTab(DocumentWindow* self) { self->document->EnterCharacter('\t'); COMMON_ACTION_END(); }
bool DocumentWindow::Action_Backspace(DocumentWindow* self) { self->document->Backspace(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_Undo(DocumentWindow* self) { self->document->Undo(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_Redo(DocumentWindow* self) { self->document->Redo(); COMMON_ACTION_END(); }
bool DocumentWindow::Action_TriggerCompletion(DocumentWindow* self) { self->document->TriggerCompletion(); COMMON_ACTION_END(); }

bool DocumentWindow::Action_CycleCompletionList(DocumentWindow* self) {
  if (!self->document->CycleCompletionList()) return false;
  self->document->SelectCompletionItem();
  COMMON_ACTION_END();
}

bool DocumentWindow::Action_CycleCompletionListReversed(DocumentWindow* self) {
  if (!self->document->CycleCompletionListReversed()) return false;
  self->document->SelectCompletionItem();
  COMMON_ACTION_END();
}

bool DocumentWindow::Action_Clear(DocumentWindow* self) {
  self->document->ClearCompletionItems();
  self->document->cursors.ClearMultiCursors();
  self->document->cursors.ClearSelections();
  COMMON_ACTION_END();
}

