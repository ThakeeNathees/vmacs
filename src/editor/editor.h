// Copyright (c) 2023 Thakee Nathees

#pragma once

#include "core/widget.h"
#include "core/window.h"
#include "core/cursors.h"
#include "core/file.h"

#include "textbox/textbox.h"


class TextEditor : public Widget {
public:

  TextEditor(Window* window);

  bool HandleEvent(const Event& event);
  void Update() override;

private:

  void _Draw(Size area) override;
  int _DrawLineNumbers(Vector2i pos, Size area); // Returns the gutter width as character count.
  void _DrawStatusLine(Vector2i pos, Size area);

  std::unique_ptr<TextBox> textbox = nullptr;

  // All the values are multiple of char size.
  int gutter_margin_left = 1;
  int gutter_min_width = 2;
  int text_margin_left = 1;

};