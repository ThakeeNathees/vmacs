//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "document.hpp"


Buffer::Buffer(std::vector<char>&& data) : buffer(data) {
  OnBufferChanged();
}


const std::vector<Slice>& Lines::Get() const {
  return slices;
}


void Lines::ComputeLines(const char* text, size_t size) {
  slices.clear();

  Slice slice;
  slice.start = 0;

  int i = 0;
  while (i < size) {
    if (text[i] == '\n') {
      slice.end = i;
      slices.push_back(slice);
      slice.start = i + 1;
    }
    i++;
  }
  ASSERT(i == size, OOPS);

  slice.end = i;
  slices.push_back(slice);
}


size_t Buffer::GetSize() const {
  return buffer.size();
}


const char* Buffer::GetData() const {
  return buffer.data();
}


uint32_t Buffer::At(int index) const {
  ASSERT_INDEX(index, buffer.size() + 1); // +1 since we'll include '\0'.
  // Can't directly cast char to unit32_t since chars are signed and has negative
  // values which will overflow the byte range if we directly cast.
  return (uint8_t) buffer[index];
}


String Buffer::GetSubString(int index, int count) const {
  if (count == 0) return "";
  ASSERT_INDEX(index, buffer.size());
  ASSERT_INDEX(index + count, buffer.size() + 1);
  return String(std::string(buffer.data() + index, count));
}


int Buffer::GetLineCount() const {
  return (int) lines.Get().size();
}


Slice Buffer::GetLine(int index) const {
  ASSERT_INDEX(index, lines.Get().size());
  return lines.Get()[index];
}


Coord Buffer::IndexToCoord(size_t index) const {
  ASSERT_INDEX(index, buffer.size() + 1);

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


int Buffer::IndexToColumn(size_t index) const {

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


size_t Buffer::CoordToIndex(Coord coord) const {
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


bool Buffer::IsValidIndex(size_t index) const {
  return BETWEEN(0, index, GetSize()-1);
}


bool Buffer::IsValidCoord(Coord coord, size_t* index) const {
  if (coord.line < 0) return false;
  if (coord.character < 0) return false;
  if (coord.line >= lines.Get().size()) return false;
  Slice line = GetLine(coord.line);
  int line_len = line.end - line.start + 1;
  if (coord.character >= line_len) return false;
  if (index) *index = CoordToIndex(coord);
  return true;
}


void Buffer::InsertText(size_t index, const String& text) {
  ASSERT_INDEX(index, (int) buffer.size() + 1);
  auto begin = text.Data().begin();
  auto end   = text.Data().end();
  buffer.insert(buffer.begin() + index, begin, end);
  OnBufferChanged();
}


void Buffer::RemoveText(size_t index, int count) {
  if (count == 0) return;
  ASSERT_INDEX(index, buffer.size());
  ASSERT_INDEX(index + count, (int)buffer.size() + 1);
  buffer.erase(buffer.begin() + index, buffer.begin() + index + count);
  OnBufferChanged();
}


void Buffer::OnBufferChanged() {
  lines.ComputeLines(buffer.data(), buffer.size());
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
