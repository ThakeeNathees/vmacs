// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <vector>
#include <raylib.h>

#include "core/common.hpp"

class Buffer;


class Cursor {

public:
  Cursor() {}

  int GetIndex() const { return index; }
  int GetColumn() const { return column; }
  const Coord& GetCoord() const { return coord; }
  bool HasSelection() const { return selection_start >= 0; }
  int GetSelectionStart() { return selection_start; }

  // I don't want to use operator overloading, cause it's implicit.
  bool Equals(const Cursor& other) const;

  void UpdateColumn() { column = coord.col; }
  void ClearSelection() { selection_start = -1; }
  void SetIndex(const Buffer* buffer, int index);
  void SetColumn(int col) { column = col; }
  void SetSelectionStart(const Buffer* buffer, int index);
  void StartSelection() { selection_start = index; }

  // Note: (selection.end - 1) = last selected character in the buffer.
  // 
  //          .--------. (selected text)
  // f  o  o  b  a  r  b  a  z
  // 0  1  2  3  4  5  6  7  8 
  //
  // selection = (3, 7) but buff[7] is not selected.
  //
  Slice GetSelection() const;

private:
  int index = 0;
  Coord coord = { 0, 0 };
  int column = 0; // The column it wants to go if exists.

  // If the cursor has a selection this will be the start index.
  // If selection start is negative value, that means there is no
  // selection.
  int selection_start = -1;
};


class Cursors {

public:
  std::vector<Cursor>& Get() { return cursors; }

  void AddCursor(const Cursor& cursor);
  void AddCursorDown(const Buffer* buffer);
  void AddCursorUp(const Buffer* buffer);
  void ClearMultiCursors();
  void ClearSelections();
  const Cursor& GetPrimaryCursor() const;

  bool Equals(const Cursors& other) const;

  // This should be called every time the cursors changed.
  void OnMoved(const Buffer* buffer);

  bool IsBlinkShow() const { return is_blink_show; }
  void BlinkUpdate();
  void ResetBlinkTimer();

private:
  void _SortCursors();
  void _RemoveDuplicates();
  void _MergeSelections(const Buffer* buffer);

  std::vector<Cursor> cursors = { Cursor() };

  // If the cursors were added from bottom to up, the last cursor is the main
  // cursor and when we cleanup we remove all the other ones.
  bool is_multicursor_reversed = false;
  bool is_multicursor_adding = false;

  // We set it to the time it was blinked last time and
  // if (time_now - last_blink > blink_period) we blink again.
  double last_blink = 0.0;
  double blink_period = 0.5; // Blinks every .5 seconds.
  bool is_blink_show = true; // If false it won't be rendered (hidden part of the blink).

};
