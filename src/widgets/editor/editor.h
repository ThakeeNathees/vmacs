// Copyright (c) 2023 Thakee Nathees

#pragma once

#include "core/widget.h"
#include "core/window.h"
#include "core/cursors.h"
#include "core/file.h"

#include "widgets/textbox/textbox.h"


class TextEditor : public Widget {
public:

  TextEditor(Window* window);

private:

  void _Draw(Size area) override;
  int _DrawLineNumbers(Vector2i pos, Size area); // Returns the gutter width as character count.
  void _DrawStatusLine(Vector2i pos, Size area);

  static ModeList _GetModes();
  static void _ClearCursors(Widget* w, CommandArgs args);

  TextBox* textbox = nullptr;

  std::shared_ptr<File> file;

  // All the values are multiple of char size.
  int gutter_margin_left = 1;
  int gutter_min_width = 2;
  int text_margin_left = 1;

};