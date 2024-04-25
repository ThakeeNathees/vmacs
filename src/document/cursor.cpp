//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "document.hpp"


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


/******************************************************************************
 * MultiCursor
 ******************************************************************************/


MultiCursor::MultiCursor(Buffer* buffer) : buffer(buffer) {
  cursors.push_back(Cursor(buffer));
}


std::vector<Cursor>& MultiCursor::Get() {
  return cursors;
}


const Cursor& MultiCursor::GetPrimaryCursor() const {
  ASSERT(cursors.size() >= 1, OOPS);
  if (reversed) return cursors[0];
  return cursors[cursors.size() - 1];
}


void MultiCursor::AddCursor(const Cursor& cursor) {
  for (const Cursor& curs : cursors) {
    if (curs.GetIndex() == cursor.GetIndex()) {
      return;
    }
  }
  cursors.push_back(cursor);
  SortCursors();
}


void MultiCursor::AddCursorDown() {
  if (reversed) {
    if (cursors.size() == 1) reversed = false;
    else { // Just remove the first cursor.
      cursors.erase(cursors.begin(), cursors.begin() + 1);
      return;
    }
  }

  // Coordinate of the last cursor.
  Coord coord = cursors[cursors.size() - 1].GetCoord();
  if (coord.row == buffer->GetLineCount() - 1) return;

  int intended_col = cursors[0].GetColumn();
  const Slice& next = buffer->GetLine(coord.row + 1);
  int index = buffer->ColumnToIndex(intended_col, coord.row+1);
  
  Cursor new_cursor(buffer);
  new_cursor.SetIndex(index);
  new_cursor.UpdateColumn();
  AddCursor(new_cursor);

  return;
}


void MultiCursor::AddCursorUp() {
  if (!reversed) {
    if (cursors.size() == 1) reversed = true;
    else { // Just remove the last cursor.
      cursors.erase(cursors.end() - 1, cursors.end());
      return;
    }
  }

  // Coordinate of the first cursor.
  Coord coord = cursors[0].GetCoord();
  if (coord.row == 0) return;

  int intended_col = cursors[cursors.size() - 1].GetColumn();
  const Slice& prev = buffer->GetLine(coord.row - 1);
  int index = buffer->ColumnToIndex(intended_col, coord.row-1);

  Cursor new_cursor(buffer);
  new_cursor.SetIndex(index);
  new_cursor.UpdateColumn();
  AddCursor(new_cursor);

  return;
}


void MultiCursor::ClearMultiCursors() {
  if (reversed) {
    cursors.erase(cursors.begin(), cursors.end() - 1);
  } else {
    cursors.erase(cursors.begin() + 1, cursors.end());
  }
}


void MultiCursor::ClearSelections() {
  for (Cursor& cursor : cursors) {
    cursor.ClearSelection();
  }
}


void MultiCursor::Changed() {
  // Clamp the cursor indexes if it goes out of range. This is possible if we
  // the buffer changed without our influence.
  for (Cursor& cursor : cursors) {
    int cursor_index = cursor.GetIndex();
    if (cursor_index < 0) {
      cursor.SetIndex(0);
    } else if (cursor_index > buffer->GetSize()) {
      cursor.SetIndex(buffer->GetSize());
    }
  }

  // Sort the cursors BEFORE we remove duplicate since it requires a sorted list.
  SortCursors();

  // Remove consecutive (adjacent) duplicates.
  // https://en.cppreference.com/w/cpp/algorithm/unique
  auto last = std::unique(cursors.begin(), cursors.end(), [](const Cursor& l, const Cursor& r) {
    return l.GetIndex() == r.GetIndex();
  });
  cursors.erase(last, cursors.end());

  // Merge selections: if two cursors have selection intersecion we'll merge
  // them into a single cursor with a long selection.
  if (cursors.size() > 1) {

    auto it = cursors.begin() + 1;
    while (it != cursors.end()) {
      Cursor& c1 = *(it - 1);
      Cursor& c2 = *it;

      if (!c1.HasSelection() || !c2.HasSelection()) {
        ++it;
        continue;
      }

      Slice s1 = c1.GetSelection();
      Slice s2 = c2.GetSelection();

      ASSERT(s1.start < s2.start && s1.end < s2.end, OOPS);
      if (s2.start < s1.end) { // Has collision.
        if (c1.GetIndex() == s1.end) {
          c1.SetIndex(s2.end);
          c1.SetColumn(c2.GetColumn());
          ASSERT(c1.GetSelectionStart() == s1.start, OOPS);
        } else {
          ASSERT(c1.GetIndex() == s1.start, OOPS);
          c1.SetSelectionStart(s2.end);
        }
        it = cursors.erase(it);

      } else {
        ++it;
      }
    }
  } // Merge cursors.

}


void MultiCursor::SortCursors() {
  // Sort the cursors depend on the index of them.
  std::sort(cursors.begin(), cursors.end(),
    [](const Cursor& l, const Cursor& r) {
      return l.GetIndex() < r.GetIndex();
  });
}
