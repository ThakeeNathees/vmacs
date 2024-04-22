//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <raylib.h>
#include <vmacs.hpp>

class Raylib : public FrontEnd {
public:
  virtual bool Initialize() override;
  virtual bool Cleanup() override;
  virtual std::vector<Event> GetEvents() override;

  virtual DrawBuffer GetDrawBuffer() override;
  virtual void Display(uint32_t clear_color) override;

private:
  int font_size = 40;
  Font font;

  DrawBuffer draw_buffer;
  std::vector<Cell> cells;
};

