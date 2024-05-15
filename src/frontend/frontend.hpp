//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include <termbox2/termbox2.h>


class Termbox2 : public IFrontEnd {
public:
  bool Initialize() override;
  std::vector<Event> GetEvents() override;
  Area GetDrawArea() override;
  void Display(FrameBuffer& buff) override;
  bool Cleanup() override;
};
