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

#define GETUI() static_cast<Ui*>(Editor::Singleton()->GetUi());

// -----------------------------------------------------------------------------
// Window.
// -----------------------------------------------------------------------------


class Split;
class Tab;
class Ui;
class DocumentWindow;
class FindWindow;
class FileTree;
class FileTreeWindow;


// Note that since Window is a subtype of event handler. And HandleEvent() is
// already defined.
class Window : public ActionExecutor {

public:

  // TODO: Maybe remove this and implement IsDocWindow() since nothing else matters.
  // We'll use this enum to type case from Window* to the curresponding child type.
  enum class Type {
    DOCUMENT,
    FINDER,
    FILETREE,
    OTHER,
  };

  virtual ~Window() = default;

  // By default it'll return OTHER, override this if needed to match for specific type..
  virtual Type GetType() const;

  // This will internally call _Draw(), and the subclasses should override it.
  bool HandleEvent(const Event& event);
  void Update();
  void Draw(FrameBuffer& buff, Position pos, Area area);

  // Set if this window is active in it's tab, this will be called if the window
  // gained or lost focus.
  void SetActive(bool active);
  bool IsActive() const;

  // Setter and getter for for should_close.
  void SetShouldClose();
  bool IsShouldClose() const;

  // Returns the position and area of the current window.
  const Position& GetPosition() const;
  const Area& GetArea() const;
  bool IsPointIncluded(const Position& point) const;

  // This will return a copy of the current window to display in a split.
  // If the window is not "copiable" it'll return nullptr and the caller should
  // handle it.
  virtual std::unique_ptr<Window> Copy() const;

private:

  // The position and area of the current window.
  Position pos = {0};
  Area area    = {0};

  // Set this to true if this window has to be closed by the owner of this
  // window.
  bool should_close = false;

private:
  // The handler should return true if the event is consumed by the window.
  virtual bool _HandleEvent(const Event& event) = 0;
  virtual void _Update() = 0;
  virtual void _Draw(FrameBuffer& buff, Position pos, Area area) = 0;

  // Override this to handled this event.
  virtual void OnFocusChanged(bool focus);
};


// -----------------------------------------------------------------------------
// Split.
// -----------------------------------------------------------------------------


// Split is a tree of child splits and the leaf node contains a window.
// If the split type is Vertical all of it's children would be horizontal splits
// or leaf nodes and vice versa.
//
// Note that the split is just the view of the window in the 'tab.window' array.
class Split {

public:
  enum class Type {
    LEAF       = 0,
    VERTICAL   = 1,
    HORIZONTAL = 2,
  };

  // Split iterator.
  class Iterator {
  public:
    Iterator(Split* root);
    Split* Get() const; // Return the current leaf we're in.
    void Next();        // Increment current node internaly.
  private:
    Split* LeftMostLeaf(Split* node) const; // It'll fall to the left most leaf of the given split.
    Split* GetNextChild(Split* node) const; // Returns the next child from a node.
    Split* curr = nullptr; // Current leaf node we're in (we always land on a leaf node).
  };

  // Returns the type of the split, hsplit, vsplit, or leaf.
  Split::Type GetType() const;

  // The split method, make sure to call this on leaf nodes otherwise this will
  // fail an assertion. This will return a raw pointer to the created split instance.
  Split* Vsplit(bool right);
  Split* Hsplit(bool bottom);

  int GetChildCount() const;
  Split* GetChild(int index) const;
  int GetIndexInParent() const;

  void RemoveChild(int index);
  void SetWindow(std::unique_ptr<Window> window);

  // Return the undeling window if available, otherwise it'll return nullptr.
  // Both a leaf spit which is not initialized and a non-leaf split contains
  // nullptr window.
  Window* GetWindow();
  const Window* GetWindow() const;

  Split* GetParent() const;
  Tab* GetTab() const;

  // Set the tab of this split and all of it's children. Note that since we can
  // create the splits before adding it to a tab, we need to set the tab recursively
  // for all the splits in the tree here.
  void SetTab(Tab* tab);

  // Returns an iterater which iterates on the leaf nodes.
  Iterator Iterate();

  Window* GetWindowAt(const Position& pos);
  void Draw(FrameBuffer& buff, Position pos, Area area);

private:
  Type type = Type::LEAF;

  // TODO: There is no size (w, h) for the split and will take equally at the
  // moment, fix it.

  Tab* tab = nullptr;      // The tab it belongs to.
  Split* parent = nullptr; // If parent is nullptr, its the root of the tab.
  std::vector<std::unique_ptr<Split>> children;

  // A leaf node will have 0 children and the window* will be available. However
  // it could be nullptr if the window is not initialized yet.
  std::unique_ptr<Window> window = nullptr;

  friend class Iterator;

private:
  // Insert a given split as a child for the current node and return a raw pointer
  // of the inserted split.
  Split* InsertChild(int index, std::unique_ptr<Split> child);

};


// -----------------------------------------------------------------------------
// Tab.
// -----------------------------------------------------------------------------


class Tab {
public:

  // Set the root split of the tab, and takes an optional active split (which
  // should be part of the given tree). If active is nullptr, it'll takes the
  // first leaf of the tree as active split.
  Tab(std::unique_ptr<Split> root);

  // Create a new tab from a window set to the root split.
  static std::unique_ptr<Tab> FromWindow(std::unique_ptr<Window> window);

  // Returns the display name of the tab this might not be unique among other tabs.
  // If the active window is a document window, it'll return the filename of that
  // document otherwise it'll returns an empty string and the caller decides what to
  // name this tab.
  std::string GetName() const;

  bool HandleEvent(const Event& event);
  void Update();
  void Draw(FrameBuffer& buff, Position pos, Area area);

  Split* GetRoot() const;
  Split* GetActiveSplit() const;
  void SetActiveSplit(Split* split);

  // "Actions" bind not bindable but called from Ui's actions, This method should
  // act like actions and return true if the event is handled / action is performed.
  bool NextWindow();
  bool Vsplit();
  bool Hsplit();

private:
  std::unique_ptr<Split> root;

  // Currently active leaf split in the split tree. Note that this is not the
  // overall active split in the Ui, since the ui can contain multiple tabs
  // (for file tree and buffers maybe).
  Split* active = nullptr;
};


// -----------------------------------------------------------------------------
// UI.
// -----------------------------------------------------------------------------


class Ui : public IUi, public ActionExecutor {
  DEFINE_GET_CLASS_NAME(Ui);

public:
  Ui();

  bool HandleEvent(const Event& event) override;
  void Update() override;
  void Draw(FrameBuffer& buff) override;

  void Info(const std::string& error);
  void Success(const std::string& error);
  void Warning(const std::string& error);
  void Error(const std::string& error);

  void AddTab(std::unique_ptr<Tab> tab);
  bool JumpToDocument(const Path& path, Coord coord); // Returns true on success.

  // FIXME: Try making these methods private.
  Window* GetActiveWindow() const;
  void SetWindowActive(Window* window);

  static KeyTree keytree;

private:
  KeyTreeCursor cursor;

  // active_tab_index will always points to a tab in the tabs list bellow if it
  // contains at least one element, otherwise -1.
  int active_tab_index = -1;
  std::vector<std::unique_ptr<Tab>> tabs;

  std::unique_ptr<Window> popup;
  std::shared_ptr<FileTree> tree;

  // FIXME(grep): This is temproary.
  std::string info_bar_text;

private:

  Tab* GetActiveTab() const;

  DocumentWindow* GetDocumentWindow(const Path& path) const;
  Window* GetWindowAt(Position pos) const;
  Split* GetWindowSplit(const Window* window) const;

  void RemoveTab(const Tab* tab);
  bool CloseWindow(Window* window);

  // FIXME(now): Remove this.
  // Returns if any filetree window available, search from left and right tabs.
  FileTreeWindow* GetFileTreeWindow() const;

  void DrawHomeScreen(FrameBuffer& buff, Position pos, Area area);
  void DrawPromptBar(FrameBuffer& buff); // Will draw at the bottom line.
  void DrawTabsBar(FrameBuffer& buff, Position pos, Area area);

public: // Actions.
  // WARNING:
  //
  // Note that since Window's first parent is not ActionExecutor we cannot safly
  // do the bellow case:
  //
  //   ActionExecutor* e = (ActionExecutor*)(ptr_of_window); // This will fail.
  //
  // And that's why we're using EventHander* in the Action methods bellow.
  // Unlike other child classes of event handers.
  static bool Action_PopupFilesFinder(ActionExecutor* self);
  static bool Action_PopupLiveGrep(ActionExecutor* self);
  static bool Action_NewDocument(ActionExecutor* self);
  static bool Action_TabNext(ActionExecutor* self);
  static bool Action_TabPrev(ActionExecutor* self);
  static bool Action_NextWindow(ActionExecutor* self);
  static bool Action_Vsplit(ActionExecutor* self);
  static bool Action_Hsplit(ActionExecutor* self);
  static bool Action_CloseWindow(ActionExecutor* self);
};


// -----------------------------------------------------------------------------
// Document Window.
// -----------------------------------------------------------------------------


// DocumentWindow is the window that handles events and display the undeling
// buffer it's more of a text editor with number line and scroll bar etc.
class DocumentWindow : public Window, public DocumentListener {
  DEFINE_GET_CLASS_NAME(DocumentWindow);

public:
  DocumentWindow();
  DocumentWindow(std::shared_ptr<Document> document);
  ~DocumentWindow();

  Type GetType() const override;
  std::shared_ptr<Document> GetDocument() const;

  // Events.
  void OnDocumentChanged() override;
  void OnFocusChanged(bool focus) override;
  std::unique_ptr<Window> Copy() const override;

  // Jump to the given coordinate, and place the cursor at that specific location.
  // If the coordinate is not valid, it'll make sure the current cursor coordinate
  // is ensured on the view.
  void JumpTo(const Coord& coord);

private:
  // The document we're editing on this window.
  std::shared_ptr<Document> document;

  // We take a backup of the document's cursors when we lost the focus and re-apply
  // when we gain again so if it changed by other windows it doesn't effect this.
  MultiCursor cursors_backup;

  // The coordinate where we start drawing the buffer from, this will change
  // after h-scroll and v-scroll.
  Position view_start = {0, 0};

  // We set it to the time it was blinked last time and
  // if (time_now - last_blink > blink_period) we blink again.
  // if cursor_blink_period <= 0, that means we won't blink.
  int cursor_blink_period = 0;  // Time in milliseconds.
  int cursor_last_blink   = 0;
  bool cursor_blink_show  = true;

private:
  void _Update() override;
  bool _HandleEvent(const Event& event) override;
  void _Draw(FrameBuffer& buff, Position pos, Area area) override;

  void ResetCursorBlink();
  void EnsureCursorOnView();

  void DrawBuffer(FrameBuffer& buff, Position pos, Area area);
  void DrawAutoCompletions(FrameBuffer& buff, Position pos, Area area);

  // Check the given index is within selection or inside the cursor and sets the pointers.
  // This is needed to set the background color of a cell and re-usable. Note that this
  // is only used in drawing the color of the cell.
  void CheckCellStatusForDrawing(int index, bool* in_cursor, bool* in_selection);

public: // Actions.
  static bool Action_NormalMode(DocumentWindow* self);
  static bool Action_InsertMode(DocumentWindow* self);
  static bool Action_CursorUp(DocumentWindow* self);
  static bool Action_CursorDown(DocumentWindow* self);
  static bool Action_CursorLeft(DocumentWindow* self);
  static bool Action_CursorRight(DocumentWindow* self);
  static bool Action_CursorEnd(DocumentWindow* self);
  static bool Action_CursorHome(DocumentWindow* self);
  static bool Action_SelectRight(DocumentWindow* self);
  static bool Action_SelectLeft(DocumentWindow* self);
  static bool Action_SelectUp(DocumentWindow* self);
  static bool Action_SelectDown(DocumentWindow* self);
  static bool Action_SelectHome(DocumentWindow* self);
  static bool Action_SelectEnd(DocumentWindow* self);
  static bool Action_AddCursor_down(DocumentWindow* self);
  static bool Action_AddCursor_up(DocumentWindow* self);
  static bool Action_InsertSpace(DocumentWindow* self);
  static bool Action_InsertNewline(DocumentWindow* self);
  static bool Action_InsertTab(DocumentWindow* self);
  static bool Action_Backspace(DocumentWindow* self);
  static bool Action_Undo(DocumentWindow* self);
  static bool Action_Redo(DocumentWindow* self);
  static bool Action_TriggerCompletion(DocumentWindow* self);
  static bool Action_CycleCompletionList(DocumentWindow* self);
  static bool Action_CycleCompletionListReversed(DocumentWindow* self);
  static bool Action_Clear(DocumentWindow* self);
};


// -----------------------------------------------------------------------------
// Find Window.
// -----------------------------------------------------------------------------


class FindWindow : public Window {
  DEFINE_GET_CLASS_NAME(FindWindow);

public:
  FindWindow(std::unique_ptr<Finder> finder);
  Type GetType() const override;

private:
  std::unique_ptr<Finder> finder;

  // The cursor inside the search bar.
  int cursor_index = 0;

  // The index of the selected item in the filtered list. By default it'll be
  // 0 if there is anything in the filter list.
  int selected_index   = -1;
  int view_start_index = 0; // The index at which the filters will be drawn.
  int selection_height = 0; // The height of the view area, needed to ensure the
                            // selection is on the view.

private:
  void _Update() override;
  bool _HandleEvent(const Event& event) override;
  void _Draw(FrameBuffer& buff, Position pos, Area area) override;

  void EnsureSelectionOnView();
  void OnItemsChanged();

  // Will return empty string if nothing is selected or the index is exceeded
  // because of IPC changed the list and index become invalid.
  std::string GetSelectedItem();

  // Draw a list of items in the given area (xywh).
  void DrawItems(FrameBuffer& buff,
                 int x, int y, int w, int h,
                 const std::vector<std::string>* items);

public: // Actions.
  static bool Action_CursorRight(FindWindow* self);
  static bool Action_CursorLeft(FindWindow* self);
  static bool Action_CursorHome(FindWindow* self);
  static bool Action_CursorEnd(FindWindow* self);
  static bool Action_Backspace(FindWindow* self);
  static bool Action_CycleSelection(FindWindow* self);
  static bool Action_CycleSelectionReversed(FindWindow* self);
  static bool Action_AcceptSelection(FindWindow* self);
  static bool Action_Close(FindWindow* self);
};


// -----------------------------------------------------------------------------
// Find Window.
// -----------------------------------------------------------------------------

class FileTree {
public:

  // The path should be a directory path where it'll be the root path.
  FileTree(const Path& path);

  struct Item {
    // The path of the item.
    Path path;

    // If it's a directory and expanded in the ui, we'll set this to true.
    bool expand = false;

    // The pointer reference to the parent. Nullptr if it's root.
    Item* parent = nullptr;

    // If it's a directory the bellow list will be the child items.
    std::vector<std::unique_ptr<Item>> items;

    // If it's incomplete we need to load the items from the system.
    bool is_incomplete = true;

    void LoadItems();
    Item* GetNextSibling() const; // nullptr if there isn't any.
    int GetIndexInParent() const; // returns -1 if root.
  };

  Item* GetRoot();

private: // FIXME: make it private.
  std::unique_ptr<Item> root;
};


class FileTreeWindow : public Window {
  DEFINE_GET_CLASS_NAME(FileTreeWindow);

public:
  FileTreeWindow(std::shared_ptr<FileTree> tree);

  Type GetType() const override;

private:
  std::shared_ptr<FileTree> tree;
  FileTree::Item* cursor = nullptr; // The current cursor position.

private:
  bool _HandleEvent(const Event& event) override;
  void _Update() override;
  void _Draw(FrameBuffer& buff, Position pos, Area area) override;

  // Draw the directory and returns the number of lines (height) it drew.
  int DrawDirItems(FileTree::Item* dir, FrameBuffer& buff, Position pos, Area area, int indent);

public:
  static bool Action_CursorUp(FileTreeWindow* self);
  static bool Action_CursorDown(FileTreeWindow* self);
  static bool Action_GotoParent(FileTreeWindow* self);
  static bool Action_SelectPath(FileTreeWindow* self);
};
