//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "buffer.hpp"


Cursor::Cursor(Buffer* buffer) : buffer(buffer) {
}


int Cursor::GetIndex() const {
  return index;
}


Coord Cursor::GetCoord() const {
  return coord;
}


int Cursor::GetColumn() const {
  return column;
}


bool Cursor::HasSelection() const {
  return selection_start >= 0;
}


int Cursor::GetSelectionStart() const {
  return selection_start;
}


Slice Cursor::GetSelection() const {
  if (!HasSelection()) return { -1, -1 };
  return {
    MIN(index, selection_start),
    MAX(index, selection_start)
  };
}


void Cursor::SetIndex(int index) {
  ASSERT(buffer != nullptr, OOPS);
  this->index = index;
  this->coord = buffer->IndexToCoord(index); // Cache the coordinate.
}


void Cursor::UpdateColumn() {
  ASSERT(buffer != nullptr, OOPS);
  Slice line = buffer->GetLine(coord.row);
  ASSERT(0 <= coord.col && coord.col <= (line.end - line.start), OOPS);

  int column = 0;
  for (int i = line.start; i < index; i++) {
    int c = buffer->At(i);
    column += (c == '\t') ? TABSIZE : 1;
  }
  SetColumn(column);
}


int Cursor::ColumnToIndex(int column, int line_num) {
  ASSERT(buffer != nullptr, OOPS);

  Slice line = buffer->GetLine(line_num);

  int curr_column = 0; // Column of the current character.
  for (int i = line.start; i <= line.end; i++) {
    if (curr_column == column) return i;
    if (curr_column > column) return i-1;
    curr_column += (buffer->At(i) == '\t') ? TABSIZE : 1;
  }

  // The column is way beyond the end of the line so we just return the end of
  // the line index.
  return line.end;
}


void Cursor::SetColumn(int column) {
  this->column = column;
}


void Cursor::SetSelectionStart(int index) {
  selection_start = index;
}


void Cursor::ClearSelection() {
  selection_start = -1;
}


bool Cursor::Equals(const Cursor& other) const {
  if (index != other.index) return false;
  if (column != other.column) return false;
  if (HasSelection() != other.HasSelection()) return false;
  if (HasSelection() && selection_start != other.selection_start) return false;
  return true;
}

