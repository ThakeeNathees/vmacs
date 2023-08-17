// Copyright (c) 2023 Thakee Nathees

#pragma once

#include "core/window.h"

#include "file.h"
#include "cursors.h"


class TextEditor : public Widget {
public:

  TextEditor(Window* window);
  void Update() override;

private:
  void _LoadModes();

  static void _InsertText(Widget* w, CommandArgs args);
  static void _CursorsUp(Widget* w, CommandArgs args);
  static void _CursorsDown(Widget* w, CommandArgs args);
  static void _CursorsLeft(Widget* w, CommandArgs args);
  static void _CursorsRight(Widget* w, CommandArgs args);
  static void _CursorsHome(Widget* w, CommandArgs args);
  static void _CursorsEnd(Widget* w, CommandArgs args);
  static void _ClearCursors(Widget* w, CommandArgs args);
  static void _AddCursorDown(Widget* w, CommandArgs args);
  static void _AddCursorUp(Widget* w, CommandArgs args);
  static void _Backspace(Widget* w, CommandArgs args);
  static void _InsertLine(Widget* w, CommandArgs args);
  static void _SelectRight(Widget* w, CommandArgs args);
  static void _SelectLeft(Widget* w, CommandArgs args);
  static void _Undo(Widget* w, CommandArgs args);
  static void _Redo(Widget* w, CommandArgs args);

  void _EnsureCursorsOnView();

  void _Draw(Size area) override;
  int _DrawLineNumbers(Vector2i pos, Size area); // Returns the gutter width as character count.
  void _DrawSelections(Vector2i pos, Size area);
  void _DrawCursors(Vector2i pos, Size area);
  void _DrawText(Vector2i pos, Size area);
  void _DrawStatusLine(Vector2i pos, Size area);

  Cursors cursors;
  Coord view_start = { 0 };
  Size text_area = { 0 }; // We store the size of the text area here to adjust the view.

  std::unique_ptr<File> file = File::New();

  //// Make _COUNT enum of modes and use indexing from a vector instead of a map.
  //Mode* mode; // Pointer to the current mode in modes.
  //std::unordered_map<Mode::Type, Mode> modes;

  // All the values are multiple of char size.
  int gutter_margin_left = 1;
  int gutter_min_width = 2;
  int text_margin_left = 1;

};