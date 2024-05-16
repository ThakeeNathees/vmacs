//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "lsp/client.hpp"
#include <tree_sitter/api.h>


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


class DocumentListener {
public:
  virtual void OnDocumentChanged()  = 0;
  virtual ~DocumentListener() = default;
};


// Since the document need theme when parsing the buffer to syntax highlight,
// we'll use this function pointer to fetch the theme, set it to document
// from the docwindow since document class doesn't know editor or theme config
// exists.
typedef const Theme* (*GetThemeFn)();


// text    = "hello\nworld"
// buffer  = {"h", "e", "l", "l", "o", "\n", "w", "o", "r", "l", "d", "\0"}
// indexes =   0    1    2    3    4     5    6    7    8    9    10    11
// Lines   = { (0, 5), (6, 10) }
//
// text    = "hello\nworld\n"  <-- Note that a new line at the end.
// buffer  = {"h", "e", "l", "l", "o", "\n", "w", "o", "r", "l", "d", "\n", "\0"}
// indexes =   0    1    2    3    4     5    6    7    8    9    10    11    12
// Lines   = { (0, 5), (6, 11), (12, 12) }
//
// Note That in the second example we have a slice after the end of the
// buffer since the buffer is ending with a new line we call this "null line".
//
// text    = ""
// buffer  = {"\0"}
// indexes =   0
// Lines   = { (0, 0) }
//
// Note that even if the buffer is empty we'll have one "null line", when you
// open a new file on a text editor the very first line exists but there isn't
// anything in the file buffer.
//
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
  std::string_view GetSubString(int index, int count) const;
  const std::string& GetData() const;

  int GetLineCount() const;
  Slice GetLine(int index) const;

  // Coord, column, index conversions. ColumnToIndex will take an optional col_delta
  // which will be set to the number of column it's behind, we the provided column
  // is at the middle of a tab.
  Coord IndexToCoord(int index) const;
  int CoordToIndex(Coord coord) const;
  int IndexToColumn(int index) const;
  int ColumnToIndex(int column, int line_num, int* col_delta=NULL) const;

  // If the index is not nullptr, it'll write the index if the coord is valid.
  bool IsValidIndex(int index) const;
  bool IsValidCoord(Coord coord, int* index) const;

  // Methods that modify the buffer.
  void InsertText(int index, const std::string& text);
  void RemoveText(int index, int count);

  // Buffer listener methods.
  void RegisterListener(BufferListener* listener);
  void UnRegisterListener(BufferListener* listener);

private:
  std::string data;
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
  int GetIntendedColumn() const;
  int GetRealColumn() const;
  bool HasSelection() const;
  int GetSelectionStart() const;
  bool Equals(const Cursor& other) const;

  // Note: (selection.end - 1) = last selected character in the buffer.
  //
  //          .--------. (selected text)
  // f  o  o  b  a  r  b  a  z
  // 0  1  2  3  4  5  6  7  8
  //
  // selection = (3, 7) but buff[7] is not selected.
  //
  Slice GetSelection() const;

  void SetIndex(int index);
  void SetIntendedColumn(int column);
  void SetSelectionStart(int index);
  void ClearSelection();

  // Adjust the column based on the current coordiante it's on now.
  void UpdateIntendedColumn();

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
  int intended_column = 0;
  int real_column     = 0; // The actual column it's currently in.

  // If the cursor has a selection this will be the start index. If selection
  // start is negative value, that means there is no selection. Note that it's
  // also possible to be the selection start and the cursor index to be the
  // in that case, the selection exists but the selected text is empty.
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
  Cursor& GetPrimaryCursor();
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


// For the undo/redo we have 3 primary types, Change, Action, History.
//
// Change: A single unit of instruction where a text is added or removed from a
//         specific index of the buffer.
//
// Action: Simply a collection of changes which will be executed when the user
//         is undo or redo. Imagine if a macro is executed all the changes of
//         the macro are all over the buffer, and we need to undo every change
//         at the same time, we call the collection of change an action and
//         group like this.
//
// History: Simple a linked list of Actions, we can do the undo/redo, and it
//          keep track of where are we at the history of the change.

struct Change {
  int index;        // The index where the change happened.
  bool added;       // If true a text was added otherwise removed.
  std::string text; // The added/removed text.
};


struct Action {
  // The state of the cursors before and after the changes.
  MultiCursor before;
  MultiCursor after;
  std::vector<Change> changes;

  // It'll insert the change in the to changes vector, if the last change and
  // this change can be combined as a single change, we'll just update the last
  // change. like every keystroke doesn't need a single change, which will take
  // a lot of memory.
  //
  // Note that this will take the ownership of the  change, so call like this.
  // PushChange(std::move(change));
  void PushChange(Change&& change);
};


class History {

public:
  History(std::shared_ptr<Buffer> buffer);

  // Returns the current version of the document (version according to the lsp).
  uint32_t GetVersion() const;

  // Between these calls all the changes are grouped into the same action.
  // This is usefull if we're running macro in to a single action.
  //
  // Example:
  //   StartAction();
  //   macro.Run();
  //   EndAction();
  //
  void StartAction();
  void EndAction();

  // If the cursor has selection it'll remove the selection and then add the
  // text for each cursor.
  MultiCursor CommitInsertText(const MultiCursor& cursors, const std::string& text);

  // Here if the direction is
  //   -1: perform backspace at the cursor.
  //   +1: perform delete at the cursor.
  MultiCursor CommitRemoveText(const MultiCursor& cursors, int direction);

  bool HasUndo() const;
  bool HasRedo() const;

  const MultiCursor& Undo();
  const MultiCursor& Redo();

  // History listener methods.
  void RegisterListener(HistoryListener* listener);
  void UnRegisterListener(HistoryListener* listener);

private:
  // Say we commited 4 actions like this:
  //
  //    a1 -> a2 -> a3 -> NULL
  //                      ^-- ptr
  // If we undo here:
  //   1. ptr--;
  //   2. reverse a3;
  //
  // And the ptr changes lilke this:
  //
  //    a1 -> a2 -> a3 -> NULL
  //                ^-- ptr
  //
  // If we redo here:
  //   1. commit a3;
  //   2. ptr++;
  //
  int ptr = 0;
  std::vector<Action> actions;

  std::shared_ptr<Buffer> buffer;

  // Current version of the buffer, It'll increase after each change of the
  // buffer, including undo/redo according to the lsp specification.
  uint32_t version = 0;

  // This is basically we start an action and all changes will go to that one
  // action till we stop the action and create another new action.
  // If this is true:
  //   assert ptr == actions.size
  //   assert actions.size > 0
  //   listening_action = actions[ptr-1] // It'll be used to append changes. */
  bool listening_action = false;

  // Returns the current listening action on which we'll append our changes.
  // If we're not started any action "session" it'll create and return a new
  // action.
  Action& _GetListeningAction(const MultiCursor& cursor);

  // Listeners for the history.
  std::vector<HistoryListener*> listeners;


private:
  // This should be called by every time the history is modified.
  void OnHistoryChanged(const std::vector<DocumentChange>& changes);
};


// A HighlightSlice is a range in the buffer with a tag called 'capture', we use
// that capture to know what sytle we should apply. The capture rule is defined
// in the highlight query of the language.
//
//   string = "while"
//   capture = "keyword.control.repeat"
//
//   string = "if"
//   capture = "keyword.control.conditional"
//
struct HighlightSlice {
  uint32_t start      = 0; // Start index in the buffer.
  uint32_t end        = 0; // End index in the buffer.
  const char* capture = NULL;
};


// Note that this struct is (once constructed) read only and it's safe to share
// accross threads.
// - Note that the life time of this `Language` type is the entire application
//   and it won't "die" at the middle of a highlighting process, So no need to
//   mutex lock. (Fix this behavior if I'm wrong).
// - The TSQuery objects it holds are the reslt of ts_query_new of treesitter
//   and we take the ownership here and will destroy at the destructor.
struct Language {
  LanguageId id;
  const TSLanguage* data    = nullptr;
  TSQuery* query_highlight  = nullptr;
  TSQuery* query_textobject = nullptr;

  Language() = default;
  Language(Language&& other);

  // No copies are allowed since it contains pointers that it needs to be the
  // owner and copy constructor will copy the ownershipt via pointer and result
  // in a double free situation.
  NO_COPY_CONSTRUCTOR(Language);

  // This will cleanup if any of the above are allocated.
  ~Language();
};


// Note that this class will hold the syntax tree information of a document and
// it's a stateful instance. It's not mento to be shared between threads.
class Syntax {

public:
  Syntax() = default;
  ~Syntax();
  NO_COPY_CONSTRUCTOR(Syntax);

  // This will parse and update it's syntax tree, highlight information textobjects.
  // If nullptr used for theme, it'll skip caching the highlight with sytle.
  void Parse(const Language* language, const Buffer* buffer, const Theme* theme);
  const std::vector<Style>& GetHighlights() const;

private:
  TSParser* parser = NULL;
  TSTree* tree     = NULL;

  // We'll generate the highlight slices when we do the parsing of the buffer,
  // which will be used to generte the highlights array, at which the i'th element
  // represent the style of the i'th character in the buffer.
  std::vector<HighlightSlice> slices;
  std::vector<Style> highlights;

private:
  void CacheHighlightSlices(const TSQuery* query, const Buffer* buffer);
  void CacheBufferStyle(const Theme* theme, const Buffer* buffer);
};


class Document : public HistoryListener, public BufferListener {

public:
  Document(); // Create with an empty buffer.
  Document(const Path& path, std::shared_ptr<Buffer> buffer);
  ~Document();

  // Listener registeration.
  void RegisterListener(DocumentListener* listener);
  void UnRegisterListener(DocumentListener* listener);

  LanguageId GetLanguageId() const;
  bool IsReadOnly() const;
  const Path& GetPath() const;

  void SetThemeGetter(GetThemeFn fn);

  const MultiCursor& GetCursors() const;
  void SetCursors(const MultiCursor& cursors);

  // Through parameter "returns" Diagnostics* at the given index if has any,
  // otherwise it won't change the given pointer.
  //
  // Note that it'll lock the internal mutex of the diagnostics vector and
  // returns the lock guard to the caller, So while the caller is processing the diagnostic
  std::unique_lock<std::mutex> GetDiagnosticAt(const Diagnostic** diagnostic, int index);

  // Lock the completion items and "returns" the pointer to the completion items.
  // The caller can modify the list if they wanted (remove entry if no fuzzy match).
  std::unique_lock<std::mutex> GetCompletionItems(std::vector<CompletionItem>** items);
  std::unique_lock<std::mutex> GetSignatureHelp(SignatureItems** items);

  // This will return the start index of the word (instead of a copy of the word) at
  // the given index which can be at the middle of the word. It's work like
  // Vim's 'b' motion.
  int GetWordBeforeIndex(int index) const;

  // Setters.
  void SetReadOnly(bool readonly);
  void SetLanguage(std::shared_ptr<const Language> language);

  // Set the language server client for this document.
  void SetLspClient(std::shared_ptr<LspClient> client);

  // Remove all the completion item, and signature help this is when we <esc>
  // on suggestion or, just moved away from the current position.
  void ClearCompletionItems();

  // Note that these methods are called from a different thread and we need to
  // re-draw the frame.
  void PushDiagnostics(uint32_t version, std::vector<Diagnostic>&& diagnostics);
  void PushCompletions(bool is_incomplete, std::vector<CompletionItem>&& items);
  void PushSignatureHelp(SignatureItems&& items);

  // Listener super class implementation.
  void OnHistoryChanged(const std::vector<DocumentChange>& changes) override;
  void OnBufferChanged() override;

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
  // EnterCharacter will insert a character by user typing this from the keyboard
  // and that'll trigger an auto completion to the lsp if possible. However the
  // InsertText will just insert a text without triggering lsp (ex: paste event).
  void EnterCharacter(char c);
  void InsertText(const std::string& text);
  void Backspace();
  void Undo();
  void Redo();

  // Lsp actions.
  void TriggerCompletion();
  void TriggerSignatureHelp();

  // Cycling the auto-complete suggestions. Note that Cycleing doesn't insert the
  // text by default like in most nvim completion plugins, but that can be achieved
  // by combinging it with SelectCompletionItem. Note that SelectCompletionItem
  // doesn't clear the completion popup (like if you press enter on vscode),
  // if needed call ClearCompletionItems after It's designed in this way so the
  // one using this class can create their desired behavior.
  //
  // Note that the bellow functions will return true if it cycled/selected items.
  // Needed to verify if the event is handled by the key binding.
  bool CycleCompletionList();
  bool CycleCompletionListReversed();
  bool SelectCompletionItem();

private:

  MultiCursor cursors;
  std::shared_ptr<Buffer> buffer;
  History history;
  std::vector<DocumentListener*> listeners;

  // Syntax tree of this document, can be used for syntax based navigation and
  // highlight.
  std::shared_ptr<const Language> language;
  Syntax syntax;
  GetThemeFn get_theme = nullptr;

  // If the file is a new buffer the path will be empty and the editor will ask
  // a path before save. If we started the editor like `vim test.c` the path is
  // `file://path/to/cwd/test.c` even thought the file isn't exists in the disk
  // we'll create a new one.
  Path path;

  std::shared_ptr<LspClient> lsp_client;

  // Since the diagnostics are updated from the LSP client's IO thread and we'll
  // be using in our main thread's draw call, it'll not safe without a lock.
  std::mutex mutex_diagnostics;
  std::vector<Diagnostic> diagnostics;

  // Updated from the LSP IO thread and used by the main loop.
  std::mutex mutex_completions;
  std::vector<CompletionItem> completion_items;
  bool is_incomplete = true; // Weather the list is incomplete or not.

  // The index of the selected item in the completion items list.
  int completion_selected = -1;

  // The index of the first character that triggerd the completion. Note that we
  // won't be using this to replace the selected word, consider typing std::vector
  // the start index will be the start of the 's' character but the suggestion
  // updated as 'vector' so we'll first try to get the index from textEdit range.
  // If not found there we'll use this. This variable is primarily used when
  // drawing the dropdown popup on where we started typing.
  int completion_start_index = -1;

  // Signature help is set by the LSP IO thread and read by the main loop.
  std::mutex mutex_signature_help;
  SignatureItems signatures_helps;

  // Document settings.
  bool readonly = false;
  // TODO:
  // Encoding: utf8, utf16, etc.
  // Line Ending.
  // Indent style (\t, or ' ').

  // DocumentWindow need cursor and buffer to draw the document, this might not be the
  // "oop" way I don't know.
  friend class DocumentWindow;

private:
  void OnDocumentChanged();
};
