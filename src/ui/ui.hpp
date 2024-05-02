//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "document/document.hpp"
#include "editor/editor.hpp"
#include "finder.hpp"

// -----------------------------------------------------------------------------
// Document Pane.
// -----------------------------------------------------------------------------

// BufferPane is the Pane that handles events and display the undeling buffer
// it's more of a text editor with number line and scroll bar etc.
class DocPane : public Pane {

public:
  DocPane();
  DocPane(std::shared_ptr<Document> document);

  void Update() override;

private:
  // The document we're editing on this pane.
  std::shared_ptr<Document> document;

  // The coordinate where we start drawing the buffer from, this will change
  // after h-scroll and v-scroll.
  Position view_start = {0, 0};

  // We "cache" the draw area for the text needed in some method.
  Size text_area = {0, 0};

  // We set it to the time it was blinked last time and
  // if (time_now - last_blink > blink_period) we blink again.
  // if cursor_blink_period <= 0, that means we won't blink.
  int cursor_blink_period = 0;  // Time in milliseconds.
  int cursor_last_blink   = 0;
  bool cursor_blink_show  = true;

private:
  bool _HandleEvent(const Event& event) override;
  void _Draw(FrameBuffer buff, Position pos, Size area) override;

  void ResetCursorBlink();
  void EnsureCursorOnView();

  void DrawBuffer(FrameBuffer buff, Position pos, Size area);
  void DrawAutoCompletions(FrameBuffer buff, Position docpos);

  // Check the given index is within selection or inside the cursor and sets the pointers.
  // This is needed to set the background color of a cell and re-usable.
  void CheckCellStatus(int index, bool* in_cursor, bool* in_selection);

public: // Actions.
  static KeyTree keytree;
  static bool Action_CursorUp(DocPane* self);
  static bool Action_CursorDown(DocPane* self);
  static bool Action_CursorLeft(DocPane* self);
  static bool Action_CursorRight(DocPane* self);
  static bool Action_CursorEnd(DocPane* self);
  static bool Action_CursorHome(DocPane* self);
  static bool Action_SelectRight(DocPane* self);
  static bool Action_SelectLeft(DocPane* self);
  static bool Action_SelectUp(DocPane* self);
  static bool Action_SelectDown(DocPane* self);
  static bool Action_SelectHome(DocPane* self);
  static bool Action_SelectEnd(DocPane* self);
  static bool Action_AddCursor_down(DocPane* self);
  static bool Action_AddCursor_up(DocPane* self);
  static bool Action_InsertSpace(DocPane* self);
  static bool Action_InsertNewline(DocPane* self);
  static bool Action_InsertTab(DocPane* self);
  static bool Action_Backspace(DocPane* self);
  static bool Action_Undo(DocPane* self);
  static bool Action_Redo(DocPane* self);
  static bool Action_TriggerCompletion(DocPane* self);
  static bool Action_CycleCompletionList(DocPane* self);
  static bool Action_CycleCompletionListReversed(DocPane* self);
  static bool Action_Clear(DocPane* self);
};


// -----------------------------------------------------------------------------
// Find Pane.
// -----------------------------------------------------------------------------


class FindPane : public Pane {

public:
  FindPane(std::unique_ptr<Finder> finder);

  void Update() override;

private:
  std::unique_ptr<Finder> finder;

  // The cursor inside the search bar.
  int cursor_index = 0;
  std::string input_text; // The text that was inputted.

  // The index of the selected item in the filtered list. By default it'll be
  // 0 if there is anything in the filter list.
  int selected_index   = -1;
  int view_start_index = 0; // The index at which the filters will be drawn.
  int selection_height = 0; // The height of the view area, needed to ensure the
                            // selection is on the view.

private:
  bool _HandleEvent(const Event& event) override;
  void _Draw(FrameBuffer buff, Position pos, Size area) override;

  void EnsureSelectionOnView();
  void OnFilteredItemsChanged();

  // Will return empty string if nothing is selected or the index is exceeded
  // because of IPC changed the list and index become invalid.
  std::string GetSelectedItem();

  // Draw a list of items in the given area (xywh).
  void DrawItems(FrameBuffer buff,
                 int x, int y, int w, int h,
                 const std::vector<std::string>* items);

public: // Actions.
  static KeyTree keytree;
  static bool Action_CursorRight(FindPane* self);
  static bool Action_CursorLeft(FindPane* self);
  static bool Action_CursorHome(FindPane* self);
  static bool Action_CursorEnd(FindPane* self);
  static bool Action_Backspace(FindPane* self);
  static bool Action_CycleSelection(FindPane* self);
  static bool Action_CycleSelectionReversed(FindPane* self);
  static bool Action_AcceptSelection(FindPane* self);

};


