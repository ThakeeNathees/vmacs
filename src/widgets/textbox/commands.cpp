// Copyright (c) 2023 Thakee Nathees

#include "textbox.h"


ModeList TextBox::_GetModes() {
  static ModeList modes = std::make_shared<std::vector<std::shared_ptr<Mode>>>(
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
        { "enter" , TextBox::_InsertLine },
        { "shift+alt+down" , TextBox::_AddCursorDown },
        { "shift+alt+up" , TextBox::_AddCursorUp },
      }),

    })
  );

  return modes;
}


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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
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
  t->cursors.OnChanged(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_Undo(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  Buffer* buffer = t->buffer.get();
  History& history = buffer->GetHistory();
  if (!history.HasUndo()) return;
  t->cursors = history.Undo();
  t->cursors.OnChanged(buffer);
  t->_EnsureCursorsOnView();
}


void TextBox::_Redo(Widget* w, CommandArgs args) {
  TextBox* t = static_cast<TextBox*>(w);
  History& history = t->buffer->GetHistory();
  const Buffer* buffer = t->buffer.get();
  if (!history.HasRedo()) return;
  t->cursors = history.Redo();
  t->cursors.OnChanged(buffer);
  t->_EnsureCursorsOnView();
}
