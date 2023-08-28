// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "history.hpp"

class Buffer;


class BufferListener {
public:
  virtual void OnBufferChanged(Buffer* buffer) = 0;
};


// class Lines (example):
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


// TODO: Consider UTF8 encoding, if such, we cannot iterate over the bytes, the
// length would vary.
class Buffer {

  // Only create this object as a unique pointer.
public: static std::unique_ptr<Buffer> New() { return std::unique_ptr<Buffer>(new Buffer); }
private: Buffer();

public:
  int At(int i) const; // Returns the codepoint if it's unicode.
  const char* GetSource() const;

  int GetSize() const; // Number of (utf8) characters in the buffer.
  Slice GetLine(int index) const;
  int GetLineCount() const;
  bool HasNullLine() const;

  Coord IndexToCoord(int index) const;
  std::string_view GetSubstring(int index, int count) const;

  void RegisterListener(BufferListener* listener);
  void UnregisterListener(BufferListener* listener);

  History& GetHistory();

  // These methods will change the buffer data and should call _OnDataChanged()
  // method at the end. Don't call these method directly to change the buffer.
  // Insetad commit action through history.
  void RemoveText(int index, int count);
  void InsertText(int index, const std::string& text);

private:
  void _OnDataChanged();

  std::string data;
  Lines lines;

  std::unique_ptr<History> history = History::New();
  std::vector<BufferListener*> listeners;
};
