// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "core/common.h"
#include "core/buffer.h"
#include "cursors.h"


class History;

class HistoryListener {
public:
  virtual void HistoryChanged(History* history) = 0;
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

  // If we a selection up or down, we don't need text delta just store the
  // start and end lines of the selection and it'll be easier to do the delta.
  struct MoveDelta {
    int line_start;
    int line_end;
  };

  // Only create this object as a unique pointer.
public: static std::unique_ptr<Action> New(Action::Type type = Type::TEXT_REMOVE) { return std::unique_ptr<Action>(new Action(type)); }
private: Action(Action::Type type) : type(type) {}

public:
  // One action can contain multiple delta for each cursors.
  // TODO: Maybe I should consider inherit an abstract Action class.
  std::vector<std::unique_ptr<TextDelta>> text_deltas;
  std::vector<MoveDelta> move_deltas;

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
