//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "buffer.hpp"


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


StringView Buffer::GetSubString(int index, int count) const {
  if (count == 0) return "";
  ASSERT_INDEX(index, data.size());
  ASSERT_INDEX(index + count, data.size() + 1);

  StringView view = data;
  return view.substr(index, count);
}


const String& Buffer::GetData() const {
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
      coord.col = index - slice.start;
      coord.row = middle;
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


int Buffer::CoordToIndex(Coord coord) const {
  return lines.Get()[coord.row].start + coord.col;
}


void Buffer::InsertText(int index, const String& text) {
  ASSERT_INDEX(index, (int)data.size() + 1);
  data.insert(index, text);
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
