// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "core/widget.hpp"
#include "widgets/textbox/textbox.hpp"


class MiniBuffer : public Widget {
public:
  MiniBuffer();

  void Clean();

private:
  void _Draw(Size area) override;

  TextBox* textbox; // Child widget.
};