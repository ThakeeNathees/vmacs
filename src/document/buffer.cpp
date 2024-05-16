//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "document.hpp"


Buffer::Buffer(const std::string& data) : data(data) {
  OnBufferChanged();
}


const std::vector<Slice>& Lines::Get() const {
  return slices;
}


void Lines::ComputeLines(const char* text) {
  slices.clear();

  Slice slice;
  slice.start = 0;

  int i = 0;
  while (text[i] != '\0') {
    if (text[i] == '\n') {
      slice.end = i;
      slices.push_back(slice);
      slice.start = i + 1;
    }
    i++;
  }

  slice.end = i;
  slices.push_back(slice);
}


int Buffer::GetSize() const {
  return (int)data.size();
}


int Buffer::At(int index) const {
  ASSERT_INDEX(index, data.size() + 1); // +1 since we'll include '\0'.
  return (int)data[index];
}


String Buffer::GetSubString(int index, int count) const {
  if (count == 0) return "";
  ASSERT_INDEX(index, data.size());
  ASSERT_INDEX(index + count, data.size() + 1);
  return data.substr(index, count);
}


const std::string& Buffer::GetData() const {
  return data;
}


int Buffer::GetLineCount() const {
  return (int)lines.Get().size();
}


Slice Buffer::GetLine(int index) const {
  ASSERT_INDEX(index, lines.Get().size());
  return lines.Get()[index];
}


Coord Buffer::IndexToCoord(int index) const {
  ASSERT_INDEX(index, (int)data.size() + 1);

  const std::vector<Slice>& slices = lines.Get();

  // Binary search in the lines array to find the index.
  int start  = 0;
  int end    = slices.size() - 1;
  int middle = (start + end) / 2;

  while (start <= end) {
    const Slice& slice = slices[middle];
    if (slice.start <= index && index <= slice.end) {
      Coord coord;
      coord.character = index - slice.start;
      coord.line = middle;
      return coord;

    } else if (slice.end < index) {
      start = middle + 1;

    } else {
      end = middle - 1;
    }
    middle = (start + end) / 2;
  }

  UNREACHABLE();
}


int Buffer::IndexToColumn(int index) const {

  const int tabsize = GetConfig().tabsize;

  // tab_width will degrease for each character and if reached 0, will reset to
  // the tabsize again.
  int tab_width = tabsize;

  Slice line = GetLine(IndexToCoord(index).line);
  int curr_column = 0;
  for (int i = line.start; i < index; i++) {
    if (At(i) == '\t') {
      curr_column += tab_width;
      tab_width = tabsize;
    } else {
      curr_column++;
      if (--tab_width == 0) tab_width = tabsize;
    }
  }
  return curr_column;
}


int Buffer::CoordToIndex(Coord coord) const {
  return lines.Get()[coord.line].start + coord.character;
}


int Buffer::ColumnToIndex(int column, int line_num, int* col_delta) const {

  const int tabsize = GetConfig().tabsize;

  // tab_width will degrease for each character and if reached 0, will reset to
  // the tabsize again.
  int tab_width = tabsize;

  Slice line = GetLine(line_num);
  int curr_column = 0;
  for (int i = line.start; i <= line.end; i++) {
    if (curr_column == column) { if (col_delta) *col_delta = 0; return i; }
    if (curr_column > column) {
      ASSERT(column % tabsize != 0, OOPS);
      if (col_delta) *col_delta = tabsize - (curr_column - column);
      return i-1;
    }

    if (At(i) == '\t') {
      curr_column += tab_width;
      tab_width = tabsize;
    } else {
      curr_column++;
    if (--tab_width == 0) tab_width = tabsize;
    }
  }

  // The column is way beyond the end of the line so we just return the end of
  // the line index.
  if (col_delta) *col_delta = column - curr_column;
  return line.end;
}


bool Buffer::IsValidIndex(int index) const {
  return BETWEEN(0, index, GetSize()-1);
}


bool Buffer::IsValidCoord(Coord coord, int* index) const {
  if (coord.line < 0) return false;
  if (coord.character < 0) return false;
  if (coord.line >= lines.Get().size()) return false;
  Slice line = GetLine(coord.line);
  int line_len = line.end - line.start + 1;
  if (coord.character >= line_len) return false;
  if (index) *index = CoordToIndex(coord);
  return true;
}


void Buffer::InsertText(int index, const String& text) {
  ASSERT_INDEX(index, (int) data.size() + 1);
  data.insert(index, text.Data());
  OnBufferChanged();
}


void Buffer::RemoveText(int index, int count) {
  if (count == 0) return;
  ASSERT_INDEX(index, (int)data.size());
  ASSERT_INDEX(index + count, (int)data.size() + 1);
  data.erase(index, count);
  OnBufferChanged();
}


void Buffer::OnBufferChanged() {
  lines.ComputeLines(data.c_str());
  for (BufferListener* listener : listeners) {
    listener->OnBufferChanged();
  }
}


void Buffer::RegisterListener(BufferListener* listener) {
  listeners.push_back(listener);
}


void Buffer::UnRegisterListener(BufferListener* listener) {
  for (int i = 0; i < listeners.size(); i++) {
    if (listeners[i] == listener) {
      listeners.erase(listeners.begin() + i);
      return;
    }
  }
}
