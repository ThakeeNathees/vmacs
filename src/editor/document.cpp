//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT


#include "core/core.hpp"
#include "buffer/buffer.hpp"
#include "editor.hpp"


Document::Document(std::shared_ptr<Buffer> buffer)
  : buffer(buffer), cursor(buffer.get()), history(buffer) {

}


void Document::PushDiagnostics(std::vector<Diagnostic>&& diagnostics) {
  for (Diagnostic& diag : diagnostics) {
    // TODO: Ignore if the version isn't our current version.
    this->diagnostics.push_back(std::move(diag));
  }
}


// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------


void Document::InsertText(const std::string& text) {
  cursor = history.CommitInsertText(cursor, text);
}


void Document::Backspace() {
  cursor = history.CommitRemoveText(cursor, -1);
}


void Document::Undo() {
  if (!history.HasUndo()) return;
  cursor = history.Undo();
}


void Document::Redo() {
  if (!history.HasRedo()) return;
  cursor = history.Redo();
}


void Document::CursorRight() {
  int buffsz = buffer->GetSize();
  int cursor_index = cursor.GetIndex();

  if (cursor.HasSelection()) {
    int next = cursor.GetSelection().end;
    cursor.SetIndex(next);
    cursor.ClearSelection();
    return;
  }
  
  if (cursor_index >= buffsz) return;
  int index = cursor_index + 1;
  cursor.SetIndex(index);
  cursor.UpdateColumn();
}


void Document::CursorLeft() {
  int buffsz = buffer->GetSize();
  int cursor_index = cursor.GetIndex();

  if (cursor.HasSelection()) {
    int next = cursor.GetSelection().start;
    cursor.SetIndex(next);
    cursor.ClearSelection();
    return;
  }

  if (cursor_index <= 0) return;

  int index = cursor_index - 1;
  cursor.SetIndex(index);
  cursor.UpdateColumn();
}


void Document::CursorUp() {
  cursor.ClearSelection();
  Coord coord = cursor.GetCoord();
  if (coord.row == 0) {
    cursor.SetIndex(0);
    cursor.UpdateColumn();
    return;
  }

  int index = cursor.ColumnToIndex(cursor.GetColumn(), coord.row-1);
  cursor.SetIndex(index);
}


void Document::CursorDown() {
  cursor.ClearSelection();
  Coord coord = cursor.GetCoord();
  int line_count = buffer->GetLineCount();
  if (coord.row == line_count-1) {
    Slice last_line = buffer->GetLine(line_count-1);
    cursor.SetIndex(last_line.end);
    cursor.UpdateColumn();
    return;
  }

  int index = cursor.ColumnToIndex(cursor.GetColumn(), coord.row+1);
  cursor.SetIndex(index);
}


void Document::CursorHome() {
  cursor.ClearSelection();
  Coord coord = cursor.GetCoord();
  Slice line = buffer->GetLine(coord.row);
  cursor.SetIndex(line.start);
  cursor.UpdateColumn();
}


void Document::CursorEnd() {
  cursor.ClearSelection();
  Coord coord = cursor.GetCoord();
  Slice line = buffer->GetLine(coord.row);
  cursor.SetIndex(line.end);
  cursor.UpdateColumn();
}


void Document::SelectRight() {
  if (buffer->GetSize() == cursor.GetIndex()) return;
  int cursor_index = cursor.GetIndex();
  if (!cursor.HasSelection()) {
    cursor.SetSelectionStart(cursor_index);
  }
  cursor_index++;
  cursor.SetIndex(cursor_index);
  cursor.UpdateColumn();
}


void Document::SelectLeft() {
  if (buffer->GetSize() == 0) return;
  int cursor_index = cursor.GetIndex();
  if (!cursor.HasSelection()) {
    cursor.SetSelectionStart(cursor_index);
  }
  cursor_index--;
  cursor.SetIndex(cursor_index);
  cursor.UpdateColumn();
}


void Document::SelectUp() {
  Coord coord = cursor.GetCoord();
  int cursor_index = cursor.GetIndex();

  // Edge case, don't select anything just return.
  if (cursor_index == 0) return;

  if (!cursor.HasSelection()) {
    cursor.SetSelectionStart(cursor_index);
  }

  if (coord.row == 0) {
    cursor.SetIndex(0);
    cursor.UpdateColumn();
    return;
  }

  int index = cursor.ColumnToIndex(cursor.GetColumn(), coord.row-1);
  cursor.SetIndex(index);
}


void Document::SelectDown() {
  Coord coord = cursor.GetCoord();
  int line_count = buffer->GetLineCount();
  int cursor_index = cursor.GetIndex();
  int buffsz = buffer->GetSize();

  // Edge case, don't select anything just return.
  if (coord.row == line_count - 1 && cursor_index == buffsz) return;

  if (!cursor.HasSelection()) {
    cursor.SetSelectionStart(cursor_index);
  }

  if (coord.row == line_count - 1) {
    Slice last_line = buffer->GetLine(line_count-1);
    cursor.SetIndex(last_line.end);
    cursor.UpdateColumn();
    return;
  }

  int index = cursor.ColumnToIndex(cursor.GetColumn(), coord.row+1);
  cursor.SetIndex(index);
}


void Document::SelectHome() {
  Coord coord = cursor.GetCoord();
  if (coord.col == 0) return;

  int cursor_index = cursor.GetIndex();
  if (!cursor.HasSelection()) {
    cursor.SetSelectionStart(cursor_index);
  }

  Slice line = buffer->GetLine(coord.row);
  cursor.SetIndex(line.start);
  cursor.UpdateColumn();
}


void Document::SelectEnd() {
  Coord coord = cursor.GetCoord();
  Slice line = buffer->GetLine(coord.row);
  if (coord.col == (line.end-line.start)) return;

  int cursor_index = cursor.GetIndex();
  if (!cursor.HasSelection()) {
    cursor.SetSelectionStart(cursor_index);
  }

  cursor.SetIndex(line.end);
  cursor.UpdateColumn();
}
