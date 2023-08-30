//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "core/widget.hpp"
#include "editor/buffer.hpp"
#include "editor/file.hpp"

#include "widgets/textbox/textbox.hpp"


class TextEditor : public Widget {

public:
  TextEditor(std::shared_ptr<File> file);

  std::unique_ptr<Widget> Copy() const override;

private:

  void _Draw(Size area) override;
  int _DrawLineNumbers(Vector2 pos, Size area); // Returns the gutter width as character count.
  void _DrawStatusLine(Vector2 pos, Size area);

  static std::shared_ptr<ModeList> _GetModes();
  static void _ClearCursors(Widget* w, CommandArgs args);

  TextBox* textbox = nullptr; // Child widget.
  std::shared_ptr<File> file;

  // TODO: Make it static.
  // All the values are multiple of char size.
  int gutter_margin_left = 1;
  int gutter_min_width = 2;
  int text_margin_left = 1;
};
