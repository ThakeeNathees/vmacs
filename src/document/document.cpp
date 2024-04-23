//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT


#include "core/core.hpp"
#include "document.hpp"


Document::Document(const Uri& uri, std::shared_ptr<Buffer> buffer)
  : uri(uri), buffer(buffer), cursors(buffer.get()), history(buffer)
{
  history.RegisterListener(this);
}


Document::~Document() {
  history.UnRegisterListener(this);
}


const std::string& Document::GetLanguage() const {
  return language;
}


bool Document::IsReadOnly() const {
  return readonly;
}


void Document::SetReadOnly(bool readonly) {
  this->readonly = readonly;
}


void Document::SetLanguage(const std::string& language) {
  this->language = language;
}


void Document::SetLspClient(std::shared_ptr<LspClient> client) {
  lsp_client = client;
  lsp_client->DidOpen(uri, buffer->GetData(), language);
}


void Document::PushDiagnostics(std::vector<Diagnostic>&& diagnostics) {

  // TOOD: check if the version of the incomming diagnostics are matching the
  // current version. This could be a very delaied reply from the server and
  // we've gone so far.
  this->diagnostics.clear();

  for (Diagnostic& diag : diagnostics) {
    // TODO: Ignore if the version isn't our current version.
    this->diagnostics.push_back(std::move(diag));
  }
}


void Document::OnHistoryChanged(const std::vector<DocumentChange>& changes) {
  // TODO: Consider moving the diagnostics coordinates a little till the next
  // diagnostics comes from the server or maybe consider removing the current
  // diagnostics after a certain time in the update() method.

  if (lsp_client) {
    lsp_client->DidChange(uri, history.GetVersion(), changes);
  }
}


// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------


void Document::InsertText(const std::string& text) {
  cursors = history.CommitInsertText(cursors, text);
}


void Document::Backspace() {
  cursors = history.CommitRemoveText(cursors, -1);
}


void Document::Undo() {
  if (!history.HasUndo()) return;
  cursors = history.Undo();
}


void Document::Redo() {
  if (!history.HasRedo()) return;
  cursors = history.Redo();
}


void Document::CursorRight() {
  for (Cursor& cursor : cursors.Get()) {

    if (cursor.HasSelection()) {
      const Slice selection = cursor.GetSelection();
      cursor.SetIndex(selection.end);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() < buffer->GetSize()) {
      cursor.SetIndex(cursor.GetIndex() + 1);
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
}


void Document::CursorLeft() {
  for (Cursor& cursor : cursors.Get()) {

    if (cursor.HasSelection()) {
      const Slice selection = cursor.GetSelection();
      cursor.SetIndex(selection.start);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() > 0) {
      cursor.SetIndex(cursor.GetIndex() - 1);
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
}


void Document::CursorUp() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    const Coord coord = cursor.GetCoord();

    if (coord.row == 0) {
      cursor.SetIndex(0);
      cursor.SetColumn(0);
    } else {
      const int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row-1);
      cursor.SetIndex(index);
    }

  }
  cursors.Changed();
}


void Document::CursorDown() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    const Coord coord = cursor.GetCoord();
    const int line_count = buffer->GetLineCount();

    if (coord.row == buffer->GetLineCount() - 1) {
      cursor.SetIndex(buffer->GetLine(line_count-1).end);
      cursor.UpdateColumn();
    } else {
      int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row+1);
      cursor.SetIndex(index);
    }

  }
  cursors.Changed();
}


void Document::CursorHome() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    const Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);

    // "ptr" will be the index of first non whitespace character.
    int ptr = line.start, buffsize = buffer->GetSize();
    for (; ptr <= line.end && ptr < buffsize; ptr++) {
      if (buffer->At(ptr) != ' ' && buffer->At(ptr) != '\t') break;
    }

    if (coord.col == 0 || coord.col > ptr - line.start) {
      cursor.SetIndex(ptr);
    } else {
      cursor.SetIndex(line.start);
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
}


void Document::CursorEnd() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);
    cursor.SetIndex(line.end);
    cursor.UpdateColumn();
  }
  cursors.Changed();
}


void Document::SelectRight() {
  for (Cursor& cursor : cursors.Get()) {
    const int index = cursor.GetIndex();

    if (index < buffer->GetSize()) {
      if (!cursor.HasSelection()) cursor.SetSelectionStart(index);
      cursor.SetIndex(index + 1);
      if (cursor.GetSelectionStart() == cursor.GetIndex()) {
        cursor.ClearSelection();
      }
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
}


void Document::SelectLeft() {
  for (Cursor& cursor : cursors.Get()) {
    int index = cursor.GetIndex();

    if (index > 0) {
      if (!cursor.HasSelection()) cursor.SetSelectionStart(index);
      cursor.SetIndex(index - 1);
      if (cursor.GetSelectionStart() == cursor.GetIndex()) {
        cursor.ClearSelection();
      }
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
}


void Document::SelectUp() {
  for (Cursor& cursor : cursors.Get()) {
    Coord coord = cursor.GetCoord();
    const int cursor_index = cursor.GetIndex();

    // Edge case, don't select anything just return.
    if (cursor_index == 0) continue;

    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(cursor_index);
    }

    if (coord.row == 0) {
      cursor.SetIndex(0);
      cursor.UpdateColumn();
      continue;
    }

    int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row-1);
    cursor.SetIndex(index);

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }

  cursors.Changed();
}


void Document::SelectDown() {
  for (Cursor& cursor : cursors.Get()) {
    Coord coord = cursor.GetCoord();
    int line_count = buffer->GetLineCount();
    int cursor_index = cursor.GetIndex();
    int buffsz = buffer->GetSize();

    // Edge case, don't select anything just return.
    if (coord.row == line_count - 1 && cursor_index == buffsz) continue;

    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(cursor_index);
    }

    if (coord.row == line_count - 1) {
      Slice last_line = buffer->GetLine(line_count-1);
      cursor.SetIndex(last_line.end);
      cursor.UpdateColumn();
      continue;
    }

    int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row+1);
    cursor.SetIndex(index);

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }

  cursors.Changed();
}


void Document::SelectHome() {
  for (Cursor& cursor : cursors.Get()) {
    const Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);
    const int index = cursor.GetIndex();

    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(index);
    }

    // "ptr" will be the index of first non whitespace character.
    int ptr = line.start, buffsize = buffer->GetSize();
    for (; ptr <= line.end && ptr < buffsize; ptr++) {
      if (buffer->At(ptr) != ' ' && buffer->At(ptr) != '\t') break;
    }

    if (coord.col == 0 || coord.col > ptr - line.start) {
      cursor.SetIndex(ptr);
    } else {
      cursor.SetIndex(line.start);
    }

    cursor.UpdateColumn();

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }
  cursors.Changed();
}


void Document::SelectEnd() {
  for (Cursor& cursor : cursors.Get()) {
    Coord coord = cursor.GetCoord();
    Slice line = buffer->GetLine(coord.row);

    if (coord.col == (line.end-line.start)) continue;

    int cursor_index = cursor.GetIndex();
    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(cursor_index);
    }

    cursor.SetIndex(line.end);
    cursor.UpdateColumn();

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }
}


void Document::AddCursorDown() {
  cursors.AddCursorDown();
}


void Document::AddCursorUp() {
  cursors.AddCursorUp();
}

