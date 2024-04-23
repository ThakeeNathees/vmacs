//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "lsp/lsp.hpp"


class BufferListener {
public:
  virtual void OnBufferChanged() = 0;
  virtual ~BufferListener() = default;
};


class HistoryListener {
public:
  virtual void OnHistoryChanged(const std::vector<DocumentChange>& changes) = 0;
  virtual ~HistoryListener() = default;
};


/*
 * text    = "hello\nworld"
 * buffer  = {"h", "e", "l", "l", "o", "\n", "w", "o", "r", "l", "d", "\0"}
 * indexes =   0    1    2    3    4     5    6    7    8    9    10    11
 * Lines   = { (0, 5), (6, 10) }
 * 
 * text    = "hello\nworld\n"  <-- Note that a new line at the end.
 * buffer  = {"h", "e", "l", "l", "o", "\n", "w", "o", "r", "l", "d", "\n", "\0"}
 * indexes =   0    1    2    3    4     5    6    7    8    9    10    11    12
 * Lines   = { (0, 5), (6, 11), (12, 12) }
 *
 * Note That in the second example we have a slice after the end of the
 * buffer since the buffer is ending with a new line we call this "null line".
 * 
 * text    = ""
 * buffer  = {"\0"}
 * indexes =   0
 * Lines   = { (0, 0) }
 *
 * Note that even if the buffer is empty we'll have one "null line", when you
 * open a new file on a text editor the very first line exists but there isn't
 * anything in the file buffer.
 */
class Lines {

public:
  void ComputeLines(const char* text);
  const std::vector<Slice>& Get() const;

private:
  std::vector<Slice> slices = { {0, 0} };
};


class Buffer {

public:
  Buffer() = default;
  Buffer(const std::string& data);

  int GetSize() const;
  int At(int index) const; // Returns the codepoint at the index.
  StringView GetSubString(int index, int count) const;
  const String& GetData() const;

  int GetLineCount() const;
  Slice GetLine(int index) const;

  Coord IndexToCoord(int index) const;
  int CoordToIndex(Coord coord) const;
  int ColumnToIndex(int column, int line_num);

  // Methods that modify the buffer.
  void InsertText(int index, const String& text);
  void RemoveText(int index, int count);

  // Buffer listener methods.
  void RegisterListener(BufferListener* listener);
  void UnRegisterListener(BufferListener* listener);

private:
  String data;
  Lines lines;
  std::vector<BufferListener*> listeners;

private:
  // This should be called by every time the buffer is modified.
  void OnBufferChanged();
};


class Cursor {

public:
  Cursor(Buffer* buffer = nullptr);

  int GetIndex() const;
  Coord GetCoord() const;
  int GetColumn() const;
  bool HasSelection() const;
  int GetSelectionStart() const;
  bool Equals(const Cursor& other) const;

  /* Note: (selection.end - 1) = last selected character in the buffer.
   * 
   *          .--------. (selected text)
   * f  o  o  b  a  r  b  a  z
   * 0  1  2  3  4  5  6  7  8 
   *
   * selection = (3, 7) but buff[7] is not selected.
   */
  Slice GetSelection() const;

  void SetIndex(int index);
  void SetColumn(int column);
  void SetSelectionStart(int index);
  void ClearSelection();

  // Adjust the column based on the current coordiante it's on now.
  void UpdateColumn();

private:
  Buffer* buffer = nullptr;  // A weak reference to the buffer it's in.
  int index      = 0;        // Index of the cursor in the buffer.
  Coord coord    = { 0, 0 }; // Coordinate of the cursor in the buffer.

  // The column it wants to go if exists. Note that the column here and the
  // coord.col are not the same.
  //
  // coord.col: the character index from the line start index.
  // column:    visible column index in the grid since tag is ~4 space long we
  //            include that in the column number.
  int column  = 0;        

  /*
   * If the cursor has a selection this will be the start index. If selection
   * start is negative value, that means there is no selection. Note that it's
   * also possible to be the selection start and the cursor index to be the
   * in that case, the selection exists but the selected text is empty.
   */
  int selection_start = -1;
};


// Simply a collection of Cursor instances.
class MultiCursor {

public:
  MultiCursor(Buffer* buffer = nullptr);

  // This will returns the sorted cursors.
  std::vector<Cursor>& Get();

  // Primary cursor is the only cursor we'll be focused on the view and new
  // cursors will be added from which.
  const Cursor& GetPrimaryCursor() const;

  void AddCursor(const Cursor& cursor);
  void AddCursorDown();
  void AddCursorUp();
  void ClearMultiCursors();
  void ClearSelections();

  // This should be called every time the underlying cursors were changed
  // outside of the Multicursor class to make sure all the cursors are sorted,
  // remove duplicates, and merge selections.
  void Changed();

private:
  // A weak reference to the current buffer we're in.
  Buffer* buffer = nullptr;

  // Invariant: We'll always have at least one cursor. We'll add the primary
  // cursor at the constructor.
  std::vector<Cursor> cursors;

  // If reversed, the primary cursor will be the first cursor, otherwise it'll
  // be the last cursor in the cursors list.
  bool reversed = false;

private:
  void SortCursors();
};


/*
 * For the undo/redo we have 3 primary types, Change, Action, History.
 *
 * Change: A single unit of instruction where a text is added or removed from a
 *         specific index of the buffer.
 *
 * Action: Simply a collection of changes which will be executed when the user
 *         is undo or redo. Imagine if a macro is executed all the changes of
 *         the macro are all over the buffer, and we need to undo every change
 *         at the same time, we call the collection of change an action and
 *         group like this.
 *
 * History: Simple a linked list of Actions, we can do the undo/redo, and it
 *          keep track of where are we at the history of the change.
 */

struct Change {
  int index;   // The index where the change happened.
  bool added;  // If true a text was added otherwise removed.
  String text; // The added/removed text.
};


struct Action {
  // The state of the cursors before and after the changes.
  MultiCursor before;
  MultiCursor after;
  std::vector<Change> changes;

  /*
   * It'll insert the change in the to changes vector, if the last change and
   * this change can be combined as a single change, we'll just update the last
   * change. like every keystroke doesn't need a single change, which will take
   * a lot of memory.
   *
   * Note that this will take the ownership of the  change, so call like this.
   * PushChange(std::move(change));
   */
  void PushChange(Change&& change);
};


class History {

public:
  History(std::shared_ptr<Buffer> buffer);

  // Returns the current version of the document (version according to the lsp).
  uint32_t GetVersion() const;

  /* Between these calls all the changes are grouped into the same action.
   * This is usefull if we're running macro in to a single action.
   *
   * Example:
   *   StartAction();
   *   macro.Run();
   *   EndAction();
   */
  void StartAction();
  void EndAction();

  /* If the cursor has selection it'll remove the selection and then add the
   * text for each cursor. */
  MultiCursor CommitInsertText(const MultiCursor& cursors, const std::string& text);

  /* Here if the direction is
   *   -1: perform backspace at the cursor.
   *   +1: perform delete at the cursor. */
  MultiCursor CommitRemoveText(const MultiCursor& cursors, int direction);

  bool HasUndo() const;
  bool HasRedo() const;

  const MultiCursor& Undo();
  const MultiCursor& Redo();

  // History listener methods.
  void RegisterListener(HistoryListener* listener);
  void UnRegisterListener(HistoryListener* listener);

private:
  /* Say we commited 4 actions like this:
   *
   *    a1 -> a2 -> a3 -> NULL
   *                      ^-- ptr
   * If we undo here:
   *   1. ptr--;
   *   2. reverse a3;
   *
   * And the ptr changes lilke this:
   *
   *    a1 -> a2 -> a3 -> NULL
   *                ^-- ptr
   *
   * If we redo here:
   *   1. commit a3;
   *   2. ptr++;
   */
  int ptr = 0;
  std::vector<Action> actions;

  std::shared_ptr<Buffer> buffer;

  /* Current version of the buffer, It'll increase after each change of the
   * buffer, including undo/redo according to the lsp specification. */
  uint32_t version = 0;

  /* This is basically we start an action and all changes will go to that one
   * action till we stop the action and create another new action.
   * If this is true:
   *   assert ptr == actions.size
   *   assert actions.size > 0
   *   listening_action = actions[ptr-1] // It'll be used to append changes. */
  bool listening_action = false;

  /* Returns the current listening action on which we'll append our changes.
   * If we're not started any action "session" it'll create and return a new
   * action. */
  Action& _GetListeningAction(const MultiCursor& cursor);

  // Listeners for the history.
  std::vector<HistoryListener*> listeners;


private:
  // This should be called by every time the history is modified.
  void OnHistoryChanged(const std::vector<DocumentChange>& changes);
};


class Document : public HistoryListener {

public:
  Document(const Uri& uri, std::shared_ptr<Buffer> buffer);
  ~Document();

  // Getters.
  const std::string& GetLanguage() const;
  bool IsReadOnly() const;

  // Setters.
  void SetLspClient(std::shared_ptr<LspClient> client);
  void SetReadOnly(bool readonly);
  void SetLanguage(const std::string& language);

  // If any language server send notification the editor will recieve it first
  // and send to the corresponded document.
  void PushDiagnostics(std::vector<Diagnostic>&& diagnostics);

  void OnHistoryChanged(const std::vector<DocumentChange>& changes) override;

  // Cursor actions.
  void CursorRight();
  void CursorLeft();
  void CursorUp();
  void CursorDown();
  void CursorHome();
  void CursorEnd();

  void SelectRight();
  void SelectLeft();
  void SelectUp();
  void SelectDown();
  void SelectHome();
  void SelectEnd();

  // Multi cursor action.
  void AddCursorDown();
  void AddCursorUp();

  // Buffer actions.
  void InsertText(const std::string& text);
  void Backspace();
  void Undo();
  void Redo();

private:
  MultiCursor cursors;
  std::shared_ptr<Buffer> buffer;
  History history;

  // Uri is simply "file://" + "the/file/path/in/the/disk".
  //
  // If the file is a new buffer the uri will be empty and the editor will ask
  // a path before save. If we started the editor like `vim test.c` the uri is
  // `file://path/to/cwd/test.c` even thought the file isn't exists in the disk
  // we'll create a new one.
  Uri uri;

  std::shared_ptr<LspClient> lsp_client;
  std::vector<Diagnostic> diagnostics;

  // Document settings.
  std::string language;
  bool readonly = false;
  // TODO:
  // Encoding: utf8, utf16, etc.
  // Line Ending.
  // Indent style (\t, or ' ').
  // readonly;

  // DocPane need cursor and buffer to draw the document, this might not be the
  // "oop" way I don't know.
  friend class DocPane;
};
