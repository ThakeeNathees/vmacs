//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core.hpp"

#include <stdio.h>
#include <stdint.h>
#include <raylib.h>

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <memory>


typedef struct {
  int row;
  int col;
} Coord;


typedef struct {
  int start;
  int end;
} Slice;


typedef struct {
  int width;
  int height;
} Size;


unsigned constexpr ConstHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) + 33 * ConstHash(input + 1) : 5381;
}

std::size_t constexpr operator "" _hash(const char* s, size_t) {
  return ConstHash(s);
}
