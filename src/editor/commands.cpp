// Copyright (c) 2023 Thakee Nathees

#include "editor.h"


void TextEditor::_LoadModes() {
  AddMode(std::make_shared<Mode>("default", nullptr, KeyBindings::New({
#define up "k"
#define down "j"
#define left "h"
#define right "l"
#define home "a"
#define end ";"
    { "<char>"              , TextEditor::_InsertText },
    { "up"                  , TextEditor::_CursorsUp },
    { "down"                , TextEditor::_CursorsDown },
    { "left"                , TextEditor::_CursorsLeft },
    { "right"               , TextEditor::_CursorsRight },
    { "home"                , TextEditor::_CursorsHome },
    { "end"                 , TextEditor::_CursorsEnd },
    { "esc"                 , TextEditor::_ClearCursors },
    { "shift+alt+down"      , TextEditor::_AddCursorDown },
    { "shift+alt+up"        , TextEditor::_AddCursorUp },
    { "backspace"           , TextEditor::_Backspace },
    { "enter"               , TextEditor::_InsertLine },
    { "shift+right"         , TextEditor::_SelectRight },
    { "shift+left"          , TextEditor::_SelectLeft },
    { "ctrl+z"              , TextEditor::_Undo },
    { "ctrl+shift+z"        , TextEditor::_Redo },
    { "alt+" up             , TextEditor::_CursorsUp },
    { "alt+" down           , TextEditor::_CursorsDown },
    { "alt+" left           , TextEditor::_CursorsLeft },
    { "alt+" right          , TextEditor::_CursorsRight },
    { "alt+" home           , TextEditor::_CursorsHome },
    { "alt+" end            , TextEditor::_CursorsEnd },
    { "shift+alt+" right    , TextEditor::_SelectRight },
    { "shift+alt+" left     , TextEditor::_SelectLeft },
    { "ctrl+shift+alt+down" , TextEditor::_AddCursorDown },
    { "ctrl+shift+alt+up"   , TextEditor::_AddCursorUp },
#undef home
#undef end

  })), true);
}


void TextEditor::_InsertText(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  if (args == nullptr) { /*TODO: error()*/ return; }
  if (args->size() != 1) { /*TODO: error()*/ return; }
  const std::string& text = args->at(0);
  History& history = e->file->GetHistory();
  e->cursors = history.CommitInsertText(e->cursors, text);
  e->_EnsureCursorsOnView();
}


void TextEditor::_CursorsUp(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
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
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_CursorsDown(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
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
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_CursorsLeft(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
    if (cursor.HasSelection()) {
      Slice selection = cursor.GetSelection();
      cursor.SetIndex(buffer, selection.start);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() > 0) {
      cursor.SetIndex(buffer, cursor.GetIndex() - 1);
    }
    cursor.UpdateColumn();
  }
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_CursorsRight(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
    if (cursor.HasSelection()) {
      Slice selection = cursor.GetSelection();
      cursor.SetIndex(buffer, selection.end);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() < buffer->GetSize()) {
      cursor.SetIndex(buffer, cursor.GetIndex() + 1);
    }
    cursor.UpdateColumn();
  }
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_CursorsHome(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
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
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_CursorsEnd(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
    cursor.ClearSelection();
    const Slice& line = buffer->GetLine(cursor.GetCoord().row);
    cursor.SetIndex(buffer, line.end);
    cursor.UpdateColumn();
  }
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_ClearCursors(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  e->cursors.ClearMultiCursors();
  e->cursors.ClearSelections();
}


void TextEditor::_AddCursorDown(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();
  e->cursors.AddCursorDown(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_AddCursorUp(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();
  e->cursors.AddCursorUp(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_Backspace(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  e->cursors = e->file->GetHistory().CommitRemoveText(e->cursors);
  e->_EnsureCursorsOnView();
}


void TextEditor::_InsertLine(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  e->cursors = e->file->GetHistory().CommitInsertText(e->cursors, "\n");
  e->_EnsureCursorsOnView();
}


void TextEditor::_SelectRight(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
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
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_SelectLeft(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  const Buffer* buffer = e->file->GetBuffer();

  for (Cursor& cursor : e->cursors.Get()) {
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
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_Undo(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  History& history = e->file->GetHistory();
  const Buffer* buffer = e->file->GetBuffer();
  if (!history.HasUndo()) return;
  e->cursors = history.Undo();
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}


void TextEditor::_Redo(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  History& history = e->file->GetHistory();
  const Buffer* buffer = e->file->GetBuffer();
  if (!history.HasRedo()) return;
  e->cursors = history.Redo();
  e->cursors.OnChanged(buffer);
  e->_EnsureCursorsOnView();
}

