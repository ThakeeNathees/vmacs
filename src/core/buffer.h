// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <raylib.h>

#include "highligther.h"


class Buffer;

class BufferListener {
public:
  virtual void OnBufferChanged(Buffer* buffer) = 0;
};


// Example:
//
// text    = "hello\nworld"
// buffer  = {"h", "e", "l", "l", "o", "\n", "w", "o", "r", "l", "d", "\0"}
// indexes =   0    1    2    3    4     5    6    7    8    9    10    11
// Lines   = { (0, 5), (6, 10) }
// 
// text    = "hello\nworld\n"  <-- Note that a new line at the end.
// buffer  = {"h", "e", "l", "l", "o", "\n", "w", "o", "r", "l", "d", "\n", "\0"}
// indexes =   0    1    2    3    4     5    6    7    8    9    10    11    12
// Lines   = { (0, 5), (6, 11), (12, 12) }
//
// Note That in the second example we have a slice after the end of the
// buffer since the buffer is ending with a new line we call this "null line".
// 
// text    = ""
// buffer  = {"\0"}
// indexes =   0
// Lines   = { (0, 0) }
//
// Note that even if the buffer is empty we'll have one "null line", when you open
// a new file on a text editor the very first line exists but there isn't anything
// in the file buffer.
//
class Lines {

public:
  void ComputeLines(const char* buff);
  const std::vector<Slice>& Get() const { return slices; }

private:
  std::vector<Slice> slices = { {0, 0} };
};


// TODO: If the data is utf8 encoded use an int array to make the read write faster.

class Buffer {

  // Only create this object as a unique pointer.
public: static std::unique_ptr<Buffer> New() { return std::unique_ptr<Buffer>(new Buffer); }
private: Buffer() {}

public:

  // Returns the codepoint if it's unicode.
  int At(int i) const;

  int GetSize() const;
  Slice GetLine(int index) const;
  int GetLineCount() const;
  bool HasNullLine() const;

  Highlighter& GetHighlighter() { return highlighter; }

  Coord IndexToCoord(int index) const;
  std::string GetSubstring(int index, int count) const;

  void RegisterListener(BufferListener* listener);

  // These methods will change the buffer data and should call _OnDataChanged()
  // method at the end.
  void RemoveText(int index, int count);
  void InsertText(int index, const std::string& text);

private:
  void _OnDataChanged();

  // TODO: if it's unicode we should have an int array here.
  std::string data;

  Lines lines;
  Highlighter highlighter;

  // FIXME: I should use a std::shared_ptr<> for safty, but assuming the
  // listener will always exists while the buffer is exists.
  std::vector<BufferListener*> listeners;
};
