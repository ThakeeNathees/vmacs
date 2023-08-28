// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "cursors.hpp"

// Buffer has a unique pointer to history and history has a weak pointer
// (raw pointer) to buffer so we forward declare it here.
class Buffer;
class History;


class HistoryListener {
public:
  virtual void OnHistoryChanged(History* history) = 0;
};


class Action {

public:

  enum class Type {
    TEXT_INSERT,
    TEXT_REMOVE,
  };

  struct TextDelta {
    // If use select a text and past something, we remove the selected
    // text and insert the clipboard there, and it considered as a single
    // INSERT_TEXT action where added, removed strings are there.
    //
    // If we selection some lines and indent, it's an INSERT_TEXT action,
    // where the old de-indent text is removed and new string added.
    std::string removed;
    std::string added;

    // Index in the buffer where we performed the delta.
    int index = -1;

    // Only create this object as a unique pointer.
  public: static std::unique_ptr<TextDelta> New() { return std::unique_ptr<TextDelta>(new TextDelta()); }
  private: TextDelta() {}
  };

  // Only create this object as a unique pointer.
public: static std::unique_ptr<Action> New(Action::Type type = Type::TEXT_REMOVE) { return std::unique_ptr<Action>(new Action(type)); }
private: Action(Action::Type type) : type(type) {}

public:
  std::vector<std::unique_ptr<TextDelta>> text_deltas;

  // The cursors before and after we commit the action.
  Cursors before;
  Cursors after;

  // If true this can be merged with the next action. If we input multiple
  // characters we'll merge them into a single action and undo all at once.
  bool _mergable = false;

  // We merge action using white space like vscode, each word are groupd together.
  //
  //  "foooo baar bazqaztex"
  //   ^---^^---^^--------^
  //    (1)   (2)    (3)  merged groups.
  //
  // If it's a white space we start a new merge group.
  bool _whitespace = false;

  Type type;

};


// History is basically a list of actions, and it has a pointer to a buffer
// all the actions and commits will be performed on that buffer.
class History {

  // Only create this object as a unique pointer.
public: static std::unique_ptr<History> New() { return std::unique_ptr<History>(new History); }
private: History() {}

public:

  void SetBuffer(Buffer* buffer);

  void RegisterListener(HistoryListener* listener);

  // Commit an action and updates the history, returns the end cursors.
  Cursors CommitInsertText(Cursors cursors, const std::string& text);
  Cursors CommitRemoveText(Cursors cursors);

  bool HasUndo() const { return ptr >= 0; }
  bool HasRedo() const { return ptr < (int)(actions.size() -1); }

  const Cursors& Undo();
  const Cursors& Redo();

  void Clean();

private:
  void _UpdateMergability(Action& action);
  bool _CanMerge(Action& on, const Action& next);
  void _Merge(Action& on, const Action& next);

  void _CommitAction(std::unique_ptr<Action> action);
  void _OnHistoryChanged();

  // We're assuming that the buffer and listeners will be alive
  // till the lifetime of this history object.
  Buffer* buffer = nullptr;
  std::vector<HistoryListener*> listeners;

  std::vector<std::unique_ptr<Action>> actions;

  // Say we commited 4 actions like this:
  //
  //    a0 -> a1 -> a2 -> a3
  //                      ^-- ptr
  //
  // If we undo here:
  //   1. reverse the action a3
  //   2. ptr -> a2
  //
  // And the ptr changes lilke this:
  //
  //    a0 -> a1 -> a2 -> a3
  //                ^-- ptr
  //
  // If we redo here:
  //   1. ptr will commit a3
  //   2. ptr -> a3
  //
  int ptr = -1;

};
