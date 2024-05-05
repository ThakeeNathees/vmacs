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
// Pane.
// -----------------------------------------------------------------------------

// Note that since Pane is a subtype of event handler. And HandleEvent() is
// already defined.
class Pane : public EventHandler {

public:
  Pane(const KeyTree* keytree); // The static key tree registry of the child class.
  virtual ~Pane() = default;

  // The handler should return true if the event is consumed by the pane.
  virtual void Update() = 0;

  // This will internally call _Draw(), and the subclasses should override it.
  void Draw(FrameBuffer buff, Position pos, Size area);
  bool HandleEvent(const Event& event) final override;

private:
  Position pos = {0};
  Size area    = {0};

  virtual void _Draw(FrameBuffer buff, Position pos, Size area) = 0;
  virtual bool _HandleEvent(const Event& event) = 0;
};


// -----------------------------------------------------------------------------
// Split.
// -----------------------------------------------------------------------------

// Split is a tree of child splits and the leaf node contains a pane.
// If the split type is Vertical all of it's children would be horizontal splits
// or leaf nodes and vice versa.
//
// Note that the split is just the view of the panes in the 'tab.panes' array.
class Split {

public:
  enum class Type {
    LEAF       = 0,
    VERTICAL   = 1,
    HORIZONTAL = 2,
  };

private:
  Type type = Type::LEAF;

  // TODO: There is no size (w, h) for the split and will take equally at the
  // moment, fix it.

  Split* parent = nullptr; // If parent is nullptr, its the root of the tab.
  std::vector<std::unique_ptr<Split>> children;

  // A leaf node will have 0 children and the pane* will be available. However
  // it could be nullptr if the pane is not initialized yet.
  Pane* pane = nullptr;

  friend class Tab;
};


// -----------------------------------------------------------------------------
// Tab.
// -----------------------------------------------------------------------------


class Tab : public EventHandler {
public:
  Tab();

  bool HandleEvent(const Event& event) override;
  void Update();
  void Draw(FrameBuffer buff, Position pos, Size area);

  // "Constructors".
  static std::unique_ptr<Tab> FromPane(std::unique_ptr<Pane> pane);

  // Key tree is public so we can register action and bind to keys outside. I
  // don't like the OOP getters and setters (what's the point)?
  static KeyTree keytree;

private:
  Split root;

  // The panes in the splits, where each split will have a weak reference to the
  // pane in this list.
  std::vector<std::unique_ptr<Pane>> panes;

  // Currently active pane in the split tree. If the popup is not nullptr, we'll
  // prioratize the popup to handle the events and ignore the active pane.
  Pane* active = nullptr;

private:
  void DrawSplit(FrameBuffer buff, Split* split, Position pos, Size area);

};


// -----------------------------------------------------------------------------
// Window.
// -----------------------------------------------------------------------------

// WARNING:
//
// Note that since Window's first parent is not EventHander we cannot safly do
// the bellow case:
//
//   EventHandler* e = (EventHander*)(ptr_of_window); // This will fail.
//
// And that's why we're using EventHander* in the Action methods bellow. Unlike
// other child classes of event handers.
class Window : public IWindow, public EventHandler {
public:
  Window();

  bool HandleEvent(const Event& event);
  void Update();
  void Draw(FrameBuffer buff);

  void Info(const std::string& error);
  void Success(const std::string& error);
  void Warning(const std::string& error);
  void Error(const std::string& error);

  // TODO: Support insert tab with a name.
  void AddTab(std::unique_ptr<Tab> tab);

  static KeyTree keytree;

private:
  std::unique_ptr<Tab> tab; // TODO: Make this a vector.
  std::unique_ptr<Pane> popup;

  // FIXME(grep): This is temproary.
  std::string info_bar_text;

public: // Actions.
  static bool Action_ClosePopup(EventHandler* self);
  static bool Action_PopupFilesFinder(EventHandler* self);
};


// -----------------------------------------------------------------------------
// Document Pane.
// -----------------------------------------------------------------------------

// BufferPane is the Pane that handles events and display the undeling buffer
// it's more of a text editor with number line and scroll bar etc.
class DocPane : public Pane, public DocumentListener {

public:
  DocPane();
  DocPane(std::shared_ptr<Document> document);

  void Update() override;

  // Document listener methods.
  void OnDocumentChanged() override;

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


