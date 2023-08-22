// Copyright (c) 2023 Thakee Nathees

#include "buffer.h"
#include "utf8.h"


void Lines::ComputeLines(const char* buff) {
  slices.clear();

  Slice slice;
  slice.start = 0;

  int i = 0;
  while (buff[i] != '\0') {
    if (buff[i] == '\n') {
      slice.end = i;
      slices.push_back(slice);
      slice.start = i + 1;
    }
    i++;
  }

  slice.end = i;
  slices.push_back(slice);
}


int Buffer::At(int i) const {
  ASSERT_INDEX(i, data.size() + 1); // +1 since we'll include '\0'.
  return (int) data[i];
}


const char* Buffer::GetSource() const {
  return data.c_str();
}


int Buffer::GetSize() const {
  return (int) data.size();
}


std::string Buffer::GetSubstring(int index, int count) const {
  if (count == 0) return "";
  ASSERT_INDEX(index, (int) data.size());
  ASSERT_INDEX(index + count, (int) data.size() + 1);

  return data.substr(index, count);
}


Slice Buffer::GetLine(int index) const {
  const std::vector<Slice>& slices = lines.Get();
  if (index < 0) index += (int)slices.size();
  ASSERT_INDEX(index, slices.size());
  return slices[index];
}


int Buffer::GetLineCount() const {
  int count = (int)lines.Get().size();
  ASSERT(count >= 1, OOPS);
  return count;
}


bool Buffer::HasNullLine() const {
  if (data.size() == 0) return true;
  if (data[data.size() - 1] == '\n') return true;
  return GetLine(-1).start >= data.size();
}


Coord Buffer::IndexToCoord(int index) const {
  ASSERT_INDEX(index, (int) data.size() + 1);

  // TODO: This could be optimized with binary search.
  const std::vector<Slice>& slices = lines.Get();
  for (int i = 0; i < (int)slices.size(); i++) {
    const Slice& slice = slices[i];
    if (index <= slice.end) {
      Coord coord;
      coord.col = index - slice.start;
      coord.row = i;
      return coord;
    }
  }

  UNREACHABLE();
}


void Buffer::RegisterListener(BufferListener* listener) {
  listeners.push_back(listener);
}


void Buffer::RemoveText(int index, int count) {
  if (count == 0) return;

  ASSERT_INDEX(index, (int) data.size());
  ASSERT_INDEX(index + count, (int) data.size() + 1);
  data.erase(index, count);
  _OnDataChanged();
}


void Buffer::InsertText(int index, const std::string& text) {
  ASSERT_INDEX(index, (int)data.size() + 1);
  data.insert(index, text);
  _OnDataChanged();
}


void Buffer::_OnDataChanged() {
  lines.ComputeLines(data.c_str());

  // Brodcast message.
  for (BufferListener* listener : listeners) {
    listener->OnBufferChanged(this);
  }

}
