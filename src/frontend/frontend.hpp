//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <vmacs.hpp>
#include <termbox2/termbox2.h>


class Termbox2 : public FrontEnd {
public:
  virtual bool Initialize() override;
  virtual bool Cleanup() override;
  virtual std::vector<Event> GetEvents() override;

  virtual FrameBuffer_& GetDrawBuffer() override;
  virtual void Display() override;

private:
  FrameBuffer_ buff;
};


