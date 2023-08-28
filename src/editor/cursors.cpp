// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include <algorithm>

#include "cursors.hpp"
#include "buffer.hpp"


bool Cursor::Equals(const Cursor& other) const {
  if (index != other.index) return false;
  if (column != other.column) return false;
  if (HasSelection() != other.HasSelection()) return false;
  if (HasSelection() && selection_start != other.selection_start) return false;
  return true;
}


Slice Cursor::GetSelection() const {
  if (!HasSelection()) return { -1, -1 };
  return {
    MIN(index, selection_start),
    MAX(index, selection_start)
  };
}


void Cursor::SetSelectionStart(const Buffer* buffer, int index) {
  ASSERT_INDEX(index, buffer->GetSize());
  selection_start = index;
}


void Cursor::SetIndex(const Buffer* buffer, int index) {
  this->index = index;
  this->coord = buffer->IndexToCoord(index);
}


void Cursors::AddCursor(const Cursor& cursor) {
  for (const Cursor& curs : cursors) {
    if (curs.GetIndex() == cursor.GetIndex()) {
      return;
    }
  }
  cursors.push_back(cursor);
  _SortCursors();
}


void Cursors::AddCursorDown(const Buffer* buffer) {
#define return_defer goto l_defer
#define return ? // Dont use return in this function.

  if (!is_multicursor_adding) {
    ClearMultiCursors();
    is_multicursor_adding = true;
    is_multicursor_reversed = false;
  }

  if (is_multicursor_reversed) {
    // This means all the reversed cursors were removed by the down key
    // and now we'll switch back to non-reversed.
    if (cursors.size() == 1) {
      is_multicursor_reversed = false;
    } else { // Just remove the first cursor.
      cursors.erase(cursors.begin(), cursors.begin() + 1);
      return_defer;
    }
  }

  // Coordinate of the last cursor.
  Coord coord = cursors[cursors.size() - 1].GetCoord();
  if (coord.row == buffer->GetLineCount() - 1) return_defer;

  {
    Cursor new_cursor;
    int intended_col = cursors[0].GetColumn();
    const Slice& next = buffer->GetLine(coord.row + 1);
    int col = next.end - next.start;
    if (col < intended_col) {
      new_cursor.SetIndex(buffer, next.start + col);
    } else {
      new_cursor.SetIndex(buffer, next.start + intended_col);
    }
    new_cursor.UpdateColumn();
    AddCursor(new_cursor);
  }

#undef return_defer
#undef return
  l_defer:
  ResetBlinkTimer();
}


void Cursors::AddCursorUp(const Buffer* buffer) {
#define return_defer goto l_defer
#define return ? // Dont use return in this function.

  if (!is_multicursor_adding) {
    ClearMultiCursors();
    is_multicursor_adding = true;
    is_multicursor_reversed = true;
  }

  if (!is_multicursor_reversed) {
    // This means all the non-reversed cursors were removed by the up key
    // and now we'll switch back to reversed.
    if (cursors.size() == 1) {
      is_multicursor_reversed = true;
    } else { // Just remove the last cursor.
      cursors.erase(cursors.end() - 1, cursors.end());
      return_defer;
    }
  }

  // Coordinate of the first cursor.
  Coord coord = cursors[0].GetCoord();
  if (coord.row == 0) { // We're at the first line.
    return_defer;
  }

  {
    Cursor new_cursor;
    int intended_col = cursors[cursors.size() - 1].GetCoord().col;
    const Slice& prev = buffer->GetLine(coord.row - 1);
    int col = prev.end - prev.start;
    if (col < intended_col) {
      new_cursor.SetIndex(buffer, prev.start + col);
    } else {
      new_cursor.SetIndex(buffer, prev.start + intended_col);
    }
    new_cursor.UpdateColumn();
    AddCursor(new_cursor);
  }

#undef return_defer
#undef return
  l_defer:
  ResetBlinkTimer();
}


void Cursors::ClearSelections() {
  for (Cursor& cursor : cursors) {
    cursor.ClearSelection();
  }
  ResetBlinkTimer();
}


const Cursor& Cursors::GetPrimaryCursor() const {
  ASSERT(cursors.size() >= 1, OOPS);
  if (is_multicursor_reversed) {
    return cursors[0];
  }
  return cursors[cursors.size() - 1];
}


bool Cursors::Equals(const Cursors& other) const {
  if (is_multicursor_reversed != other.is_multicursor_reversed) return false;
  if (is_multicursor_adding != other.is_multicursor_adding) return false;
  if (cursors.size() != other.cursors.size()) return false;

  for (int i = 0; i < cursors.size(); i++) {
    const Cursor& c1 = cursors[i];
    const Cursor& c2 = other.cursors[i];
    if (!c1.Equals(c2)) return false;
  }

  return true;
}


void Cursors::ClearMultiCursors() {
  if (is_multicursor_reversed) {
    cursors.erase(cursors.begin(), cursors.end() - 1);
  } else {
    cursors.erase(cursors.begin() + 1, cursors.end());
  }
  ResetBlinkTimer();
}


void Cursors::OnMoved(const Buffer* buffer) {
  ResetBlinkTimer();
  _RemoveDuplicates();
  _MergeSelections(buffer);
}


void Cursors::BlinkUpdate() {
  double now = GetTime();
  if (now - last_blink >= blink_period) {
    last_blink = now;
    is_blink_show = !is_blink_show;
  }
}


void Cursors::ResetBlinkTimer() {
  is_blink_show = true;
  last_blink = GetTime();
}


void Cursors::_SortCursors() {
  // Sort the cursors depend on the index of them.
  std::sort(cursors.begin(), cursors.end(),
    [](const Cursor& l, const Cursor& r) {
      return l.GetIndex() < r.GetIndex();
  });
}


void Cursors::_RemoveDuplicates() {
  _SortCursors();
  // Remove consecutive (adjacent) duplicates.
  // https://en.cppreference.com/w/cpp/algorithm/unique
  auto last = std::unique(cursors.begin(), cursors.end(), [](const Cursor& l, const Cursor& r) {
    return l.GetIndex() == r.GetIndex();
  });
  cursors.erase(last, cursors.end());
}


void Cursors::_MergeSelections(const Buffer* buffer) {
  if (cursors.size() <= 1) return;

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
        c1.SetIndex(buffer, s2.end);
        c1.SetColumn(c2.GetColumn());
        c1.SetSelectionStart(buffer, s1.start);
      } else {
        c1.SetIndex(buffer, s1.start);
        c1.SetSelectionStart(buffer, s2.end);
      }
      it = cursors.erase(it);

    } else {
      ++it;
    }
  }
}


