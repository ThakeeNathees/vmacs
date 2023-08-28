// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "buffer.hpp"
#include "language.hpp"


class Highlighter;


// The of a single character, we'll cache this value in a buffer and re-use this
// instead of calculating the value over and over 60 frames per second. Note that
// the size of this struct is smaller than a 64 bit integer.
struct Themelet {
  Color color;
  uint8_t modifiers;
};


class HighlightCache {

  // Only create this object as a unique pointer.
public: static std::unique_ptr<HighlightCache> New() { return std::unique_ptr<HighlightCache>(new HighlightCache()); }
private: HighlightCache() {}

public:
  void CacheThemelets(Buffer* buffer, const Highlighter* highlighter);
  const std::vector<Themelet>& GetThemeles() const;

private:
  std::vector<Themelet> themelets;

};


// Each slice is a substring (or a token) of the buffer and we capture that
// string with a capture string.
//
// Example: in C
//   string = "while"
//   capture = "keyword.control.repeat"
// 
//   string = "if"
//   capture = "keyword.control.conditional"
//
// We apply theme based on the capture.
struct HighlightSlice {
  int start;
  int end;
  const char* capture;
};


class Highlighter : public BufferListener {

  // Only create this object as a unique pointer.
public: static std::unique_ptr<Highlighter> New() { return std::unique_ptr<Highlighter>(new Highlighter); }
private: Highlighter();

public:
  ~Highlighter();
  void Highlight(const char* source, int size);
  void SetLanguage(const Language* lang);

  const std::vector<HighlightSlice>& GetHighlightSlices() const { return slices; }
  const HighlightSlice* GetHighlightSlice(int index) const;
  const HighlightCache* GetCache() const;

  void OnBufferChanged(Buffer* buffer) override;

private:

  //void _SortSlices();

  // TODO: Write a better datastructure and optimize.
  //
  // Consider '"a\nb"' here we have two slices:
  // 1. string (0, 6)
  // 2. escape_seequence (2, 4)
  //
  // We'll sort the slices in a way that smaller slice comes first.
  std::vector<HighlightSlice> slices;

  std::unique_ptr<HighlightCache> cache = HighlightCache::New();

  TSParser* parser = nullptr;
  TSTree* tree = nullptr;
  const Language* lang = nullptr;
};
