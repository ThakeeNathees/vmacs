//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


void InputWindow::SetLabel(const String& label) {
  this->label = label;
}


void InputWindow::_Update() {

}


bool InputWindow::_HandleEvent(const Event& event) {
  return inputbox.HandleEvent(event);
}


void InputWindow::_Draw(FrameBuffer& buff, Position pos, Area area) {
  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  const int w_percent = 70; // Width will consume 70% of the window.
  const int h_percent = 20; // Height will consume 20% of the window.

  const int W = area.width;
  const int H = area.height;
  const int w = (W * w_percent) / 100;
  const int h = 3; // Border top, bottom and the input text box.
  const int x = pos.x + (W - w) / 2;
  const int y = pos.y + ((H - h) * h_percent) / 100;

  if (area.width <= 4) return; // We need a witdh of at least 4 cells (two borders and two for spacing).
  if (area.height < 3) return; // Need at least 3 lines to draw the input box.

  Position curr(x, y);

  // Draw the border.
  DrawRectangleLine(buff, curr, Area(w, h), theme.lines, icons, true);

  // Draw the label.
  curr.x += 2;
  curr.y += 1;
  DrawTextLine(buff, label.Data().c_str(), curr, label.Length(), theme.background, icons, false);

  // Draw the input box.
  curr.x += label.Length() + 1;
  const int width = w - (curr.x - x) - 1; // -1 for right margin.
  inputbox.Draw(buff, curr, Area(width, 1));
}


bool InputWindow::Action_CursorRight(InputWindow* self) { return self->inputbox.CursorRight(); }
bool InputWindow::Action_CursorLeft(InputWindow* self) { return self->inputbox.CursorLeft(); }
bool InputWindow::Action_CursorHome(InputWindow* self) { return self->inputbox.CursorHome(); }
bool InputWindow::Action_CursorEnd(InputWindow* self) { return self->inputbox.CursorEnd(); }
bool InputWindow::Action_Backspace(InputWindow* self) { return self->inputbox.Backspace(); }

bool InputWindow::Action_Enter(InputWindow* self) {
  // TODO: implement this.
  return false;
}

bool InputWindow::Action_Close(InputWindow* self) {
  self->SetShouldClose();
  return true;
}
