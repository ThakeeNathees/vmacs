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
#include "editor/highlighter.hpp"


class TextBox : public Widget {

public:
  TextBox(bool multiline, std::shared_ptr<Buffer> buffer);

  void SetHighlighter(std::shared_ptr<Highlighter> highlighter);

  void Update();
  void OnFocusChanged();

  Buffer* GetBuffer() const;
  Coord GetViewStart() const;

  void ClearCursors();
  void Clean();


private:
  void _EnsureCursorsOnView();

  void _Draw(Size area) override;
  void _DrawSelections(Size area);
  void _DrawCursors(Size area);
  void _DrawText(Size area);

  static std::shared_ptr<ModeList> _GetModes();
  static void _InsertText(Widget* w, CommandArgs args);
  static void _InsertLine(Widget* w, CommandArgs args);
  static void _CursorsUp(Widget* w, CommandArgs args);
  static void _CursorsDown(Widget* w, CommandArgs args);
  static void _CursorsLeft(Widget* w, CommandArgs args);
  static void _CursorsRight(Widget* w, CommandArgs args);
  static void _CursorsHome(Widget* w, CommandArgs args);
  static void _CursorsEnd(Widget* w, CommandArgs args);
  static void _AddCursorDown(Widget* w, CommandArgs args);
  static void _AddCursorUp(Widget* w, CommandArgs args);
  static void _Backspace(Widget* w, CommandArgs args);
  static void _SelectRight(Widget* w, CommandArgs args);
  static void _SelectLeft(Widget* w, CommandArgs args);
  static void _Undo(Widget* w, CommandArgs args);
  static void _Redo(Widget* w, CommandArgs args);

  bool multiline;
  std::shared_ptr<Buffer> buffer;
  std::shared_ptr<Highlighter> highlighter;

  Cursors cursors;
  Coord view_start = { 0 };
  Size text_area = { 0 }; // We store the size of the text area here to adjust the view.

};
