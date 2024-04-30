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
#include "finder.hpp"


class Pane {

public:
  virtual ~Pane() = default;

  virtual void HandleEvent(const Event& event) = 0; 
  virtual void Update() = 0;
  virtual void Draw(FrameBuffer buff, Position pos, Size area) = 0;

  void RegisterAction(const std::string& action_name, FuncAction action);
  void RegisterBinding(const std::string& mode, const std::string& key_combination, const std::string& action_name);
  void SetMode(const std::string& mode);

  // Try to execute the event with the bindings, if success returns true.
  bool TryEvent(const Event& event);

private:
  KeyTree keytree;
  std::unordered_map<String, FuncAction> actions;
};


// BufferPane is the Pane that handles events and display the undeling buffer
// it's more of a text editor with number line and scroll bar etc.
class DocPane : public Pane {

public:
  DocPane();

  void SetDocument(std::shared_ptr<Document> document);

  void HandleEvent(const Event& event) override;
  void Update() override;
  void Draw(FrameBuffer buff, Position pos, Size area) override;


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
  void ResetCursorBlink();
  void EnsureCursorOnView();

  void DrawBuffer(FrameBuffer buff, Position pos, Size area);
  void DrawAutoCompletions(FrameBuffer buff, Position docpos);

  // Check the given index is within selection or inside the cursor and sets the pointers.
  // This is needed to set the background color of a cell and re-usable.
  void CheckCellStatus(int index, bool* in_cursor, bool* in_selection);
};


class FindPane : public Pane {

public:
  FindPane(std::unique_ptr<Finder> finder);

  void HandleEvent(const Event& event) override;
  void Update() override;
  void Draw(FrameBuffer buff, Position pos, Size area) override;

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

  void EnsureSelectionOnView();
  void CycleItems();
  void CycleItemsReversed();
  void OnFilteredItemsChanged();

  // Draw a list of items in the given area (xywh).
  void DrawItems(FrameBuffer buff,
                 int x, int y, int w, int h,
                 const std::vector<std::string>* items);
};


