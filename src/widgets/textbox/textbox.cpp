//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include "textbox.hpp"
#include "core/font.hpp"
#include "core/theme.hpp"


std::shared_ptr<ModeList> TextBox::_GetModes() {
  static std::shared_ptr<ModeList> modes = std::make_shared<ModeList>(
    std::initializer_list<std::shared_ptr<Mode>>({

      Mode::New("single_line", {
        { "<char>"              , TextBox::_InsertText },
        { "up"                  , TextBox::_CursorsUp },
        { "down"                , TextBox::_CursorsDown },
        { "left"                , TextBox::_CursorsLeft },
        { "right"               , TextBox::_CursorsRight },
        { "home"                , TextBox::_CursorsHome },
        { "end"                 , TextBox::_CursorsEnd },
        { "backspace"           , TextBox::_Backspace },
        { "shift+right"         , TextBox::_SelectRight },
        { "shift+left"          , TextBox::_SelectLeft },
        { "ctrl+z"              , TextBox::_Undo },
        { "ctrl+shift+z"        , TextBox::_Redo },
        { "alt+h"               , TextBox::_CursorsLeft },
        { "alt+j"               , TextBox::_CursorsDown },
        { "alt+k"               , TextBox::_CursorsUp },
        { "alt+l"               , TextBox::_CursorsRight },
        { "alt+a"               , TextBox::_CursorsHome },
        { "alt+;"               , TextBox::_CursorsEnd },
        { "shift+alt+l"         , TextBox::_SelectRight },
        { "shift+alt+h"         , TextBox::_SelectLeft },
        { "ctrl+shift+alt+down" , TextBox::_AddCursorDown },
        { "ctrl+shift+alt+up"   , TextBox::_AddCursorUp },
      }),

      Mode::New("multi_line", "single_line", {
        { "enter"          , TextBox::_InsertLine },
        { "shift+alt+down" , TextBox::_AddCursorDown },
        { "shift+alt+up"   , TextBox::_AddCursorUp },
      }),

    })
  );

  return modes;
}


TextBox::TextBox(bool multiline, std::shared_ptr<Buffer> buffer)
  : multiline(multiline), buffer(buffer) {

  _SetModes(_GetModes());
  if (multiline) SetMode("multi_line");
  else SetMode("single_line");
}


void TextBox::CopyValues(const TextBox& other) {
  multiline = other.multiline;
  buffer = other.buffer;
  highlighter = other.highlighter;
  cursors = other.cursors;
  view_start = other.view_start;
  text_area = other.text_area;
}


void TextBox::SetHighlighter(std::shared_ptr<Highlighter> highlighter) {
  this->highlighter = highlighter;
}


void TextBox::Update() {
  cursors.BlinkUpdate();
}


void TextBox::OnFocusChanged() {
  cursors.ResetBlinkTimer();
  if (IsFocused()) _EnsureCursorsOnView();
}


Buffer* TextBox::GetBuffer() const {
  return buffer.get();
}


Coord TextBox::GetViewStart() const {
  return view_start;
}


void TextBox::ClearCursors() {
  cursors.ClearMultiCursors();
  cursors.ClearSelections();
}


void TextBox::Clean() {
  int buff_size = buffer->GetSize();
  buffer->RemoveText(0, buff_size);
  History& history = buffer->GetHistory();
  history.Clean();
  ClearCursors();
  cursors.Get()[0].SetIndex(buffer.get(), 0);
}


void TextBox::_EnsureCursorsOnView() {
  Coord coord = cursors.GetPrimaryCursor().GetCoord();

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


// This entire logic was stolen from:
// https://github.com/focus-editor/focus/blob/086d13876940949e4f7681e1466c4cf33ac0477c/src/editors.jai#L374
//
// TODO: Implement glue point.
void TextBox::OnHistoryChanged(History* history, bool unod, const Action* action) {

  for (auto& delta : action->text_deltas) {

    int offset = delta->index;
    int change = (int)(delta->added.size() - delta->removed.size());

    for (Cursor& cursor : cursors.Get()) {
      if (offset <= cursor.GetIndex()) {
        cursor.SetIndex(buffer.get(), MAX(cursor.GetIndex() + change, offset));
      }
      if (!cursor.HasSelection()) continue;
      if (offset <= cursor.GetSelectionStart()) {
        cursor.SetSelectionStart(buffer.get(), MAX(cursor.GetSelectionStart() + change, offset));
      }
    }
  }
  cursors.OnMoved(buffer.get());

}


void TextBox::_DrawSelections(Size area) {
  // TODO: This is in efficient and run inside a hot loop, refactor.

  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();
  Size char_size = FontManager::GetFont()->GetCharSize();

  int line_count = buffer->GetLineCount();
  int first_line = view_start.row;
  int last_line = MIN(view_start.row + (area.height / char_size.height) - 1, line_count - 1);

  for (int i = first_line; i <= last_line; i++) {
    Slice line = buffer->GetLine(i);

    int row = i - view_start.row; // Current row.

    for (int j = line.start; j <= line.end; j++) {

      int col = (j - line.start) - view_start.col;
      if (col < 0 || col >(area.width / char_size.width)) continue;

      for (const Cursor& cursor : cursors.Get()) {
        Slice selection = cursor.GetSelection();
        if (BETWEEN(selection.start, j, selection.end - 1)) {

          DrawRectangle(
            col * char_size.width,
            row * char_size.height,
            char_size.width,
            char_size.height,
            ui.selection.bg);
        }
      }
    }
  }
}


void TextBox::_DrawCursors(Size area) {
  if (!IsFocused() && !multiline) return;
  if (IsFocused() && !cursors.IsBlinkShow()) return;

  Size char_size = FontManager::GetFont()->GetCharSize();
  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();

  for (const Cursor& cursor : cursors.Get()) {

    // Get the coordinate and make it relative to view.
    Coord coord = cursor.GetCoord();
    coord.col -= view_start.col;
    coord.row -= view_start.row;

    // If out of the area skip.
    if (coord.col < 0 || coord.row < 0) continue;
    if (coord.col > area.width / char_size.width ||
        coord.row > area.height / char_size.height) continue;

    int draw_x = coord.col * char_size.width;
    int draw_y = coord.row * char_size.height;

    typedef void (*DrawCursorFn)(int posX, int posY, int width, int height, Color color);
    DrawCursorFn draw_cursor = (IsFocused()) ? DrawRectangle : DrawRectangleLines;
    draw_cursor(
      draw_x,
      draw_y,
      char_size.width,
      char_size.height,
      ui.cursor_primary.fg
    );

    if (!IsFocused()) continue;

    int codepoint = buffer->At(cursor.GetIndex());
    if (codepoint != '\0' && codepoint != '\n') {
      DrawTextCodepoint(
        FontManager::GetFont()->regular,
        codepoint,
        { (float)draw_x, (float)draw_y },
        (float) FontManager::GetFontSize(),
        ui.background.bg);
    }
  }
}


void TextBox::_DrawText(Size area) {

  Size char_size = FontManager::GetFont()->GetCharSize();
  int font_size = FontManager::GetFontSize();
  Font font = FontManager::GetFont()->regular;

  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();

  const std::vector<Themelet>* themelets = (highlighter != nullptr)
    ? &highlighter->GetCache()->GetThemeles()
    : nullptr;

  // We add 1 and let the last character cropped out at the middle.
  int visible_line_count = (area.height / char_size.height) + 1;
  int total_line_count = buffer->GetLineCount();

  Vector2 curr = { 0, 0 };
  for (int i = 0; i <= visible_line_count; i++) {

    int line_index = view_start.row + i;
    if (line_index >= total_line_count) break;

    Slice line = buffer->GetLine(line_index);

    int line_len = line.end - line.start;
    if (line_len >= view_start.col) {

      // How many visible characters in this line.
      int char_count = line_len - view_start.col;

      for (int j = 0; j < char_count; j++) {
        int index = line.start + view_start.col + j;
        int c = buffer->At(index);

        switch (c) {

          default: {
            Color color = ((themelets != nullptr && index < themelets->size())
                           ? (*themelets)[index].color
                           : ui.text.fg);
            DrawTextCodepoint(font, c, curr, (float)font_size, color);
            curr.x += char_size.width;
          } break;

          case '\0':
          case '\n':
          UNREACHABLE();
          break;

          // TODO: \t
        }
      }
    }

    curr.y += char_size.height;
    curr.x = 0;
  }
}


void TextBox::_Draw(Size area) {

  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();
  Size char_size = FontManager::GetFont()->GetCharSize();

  // "Cache" the text area dimentions.
  text_area = {
     area.width / char_size.width,
     area.height / char_size.height
  };

  ClearBackground(ui.background.bg);

  _DrawSelections(area);
  _DrawText(area);
  _DrawCursors(area);
}


// ----------------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------------


void TextBox::_InsertText(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  if (args == nullptr) { /*TODO: error()*/ return; }
  if (args->size() != 1) { /*TODO: error()*/ return; }
  const std::string& text = args->at(0);
  History& history = t->buffer->GetHistory();
  t->cursors = history.CommitInsertText(t->cursors, text);
  t->_EnsureCursorsOnView();
}


void TextBox::_InsertLine(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  History& history = t->buffer->GetHistory();
  t->cursors = history.CommitInsertText(t->cursors, "\n");
  t->_EnsureCursorsOnView();
}


void TextBox::_CursorsUp(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    cursor.ClearSelection();
    Coord coord = cursor.GetCoord();
    if (coord.row == 0) {
      cursor.SetIndex(buffer, 0);
      cursor.SetColumn(0);
    } else {
      Slice prev = buffer->GetLine(coord.row - 1);
      int col = prev.end - prev.start;
      int index = prev.start + MIN(col, cursor.GetColumn());
      cursor.SetIndex(buffer, index);
    }
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_CursorsDown(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    cursor.ClearSelection();
    Coord coord = cursor.GetCoord();
    if (coord.row == buffer->GetLineCount() - 1) {
      cursor.SetIndex(buffer, buffer->GetLine(-1).end);
      cursor.UpdateColumn();
    } else {
      Slice next = buffer->GetLine(coord.row + 1);
      int col = next.end - next.start;
      int index = next.start + MIN(col, cursor.GetColumn());
      cursor.SetIndex(buffer, index);
    }
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_CursorsLeft(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    if (cursor.HasSelection()) {
      Slice selection = cursor.GetSelection();
      cursor.SetIndex(buffer, selection.start);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() > 0) {
      cursor.SetIndex(buffer, cursor.GetIndex() - 1);
    }
    cursor.UpdateColumn();
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_CursorsRight(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    if (cursor.HasSelection()) {
      Slice selection = cursor.GetSelection();
      cursor.SetIndex(buffer, selection.end);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() < buffer->GetSize()) {
      cursor.SetIndex(buffer, cursor.GetIndex() + 1);
    }
    cursor.UpdateColumn();
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_CursorsHome(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    cursor.ClearSelection();
    Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);

    // "ptr" will be the index of first non whitespace character.
    int ptr = line.start, buffsize = buffer->GetSize();
    for (; ptr <= line.end && ptr < buffsize; ptr++) {
      if (buffer->At(ptr) != ' ' && buffer->At(ptr) != '\t') break;
    }

    if (coord.col == 0 || coord.col > ptr - line.start) {
      cursor.SetIndex(buffer, ptr);
    } else {
      cursor.SetIndex(buffer, line.start);
    }

    cursor.UpdateColumn();
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_CursorsEnd(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    cursor.ClearSelection();
    const Slice& line = buffer->GetLine(cursor.GetCoord().row);
    cursor.SetIndex(buffer, line.end);
    cursor.UpdateColumn();
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_AddCursorDown(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();
  t->cursors.AddCursorDown(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_AddCursorUp(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();
  t->cursors.AddCursorUp(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_Backspace(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  History& history = t->buffer->GetHistory();
  t->cursors = history.CommitRemoveText(t->cursors);
  t->_EnsureCursorsOnView();
}


void TextBox::_SelectRight(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    int index = cursor.GetIndex();
    if (index < buffer->GetSize()) {
      if (!cursor.HasSelection()) cursor.StartSelection();
      cursor.SetIndex(buffer, index + 1);
      if (cursor.GetSelectionStart() == cursor.GetIndex()) {
        cursor.ClearSelection();
      }
    }
    cursor.UpdateColumn();
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_SelectLeft(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  const Buffer* buffer = t->buffer.get();

  for (Cursor& cursor : t->cursors.Get()) {
    int index = cursor.GetIndex();
    if (index > 0) {
      if (!cursor.HasSelection()) cursor.StartSelection();
      cursor.SetIndex(buffer, index - 1);
      if (cursor.GetSelectionStart() == cursor.GetIndex()) {
        cursor.ClearSelection();
      }
    }
    cursor.UpdateColumn();
  }
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_Undo(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  Buffer* buffer = t->buffer.get();
  History& history = buffer->GetHistory();
  if (!history.HasUndo()) return;
  t->cursors = history.Undo();
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_Redo(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  History& history = t->buffer->GetHistory();
  const Buffer* buffer = t->buffer.get();
  if (!history.HasRedo()) return;
  t->cursors = history.Redo();
  t->cursors.OnMoved(buffer);
  t->_EnsureCursorsOnView();
}

