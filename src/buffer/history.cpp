//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "buffer.hpp"


void Action::PushChange(Change&& change) {

  if (changes.size() == 0) {
    changes.push_back(change);
    return;
  }

  Change& last = changes[changes.size()-1];

  // Merge two additions.
  if (last.added && change.added) {
    if (last.index + last.text.size() == change.index) {
      last.text += change.text;
      return;
    }
  }

  // Merge two removals.
  if (!last.added && !change.added) {
    if (change.index + change.text.size() == last.index) {
      last.index = change.index;
      last.text = change.text + last.text;
      return;
    }
  }

  // Insert and backspace.
  if (last.added && !change.added) {
    if (
        last.text.size() >= change.text.size() &&
        last.index + last.text.size() == change.index + change.text.size()) {
      ASSERT(EndsWith(last.text, change.text), OOPS);
      int count = last.text.size() - change.text.size();
      last.text = last.text.substr(0, count);
      return;
    }
  }

  changes.push_back(change);
  return;
}


History::History(std::shared_ptr<Buffer> buffer)
  : buffer(buffer) {
}


void History::StartAction() {
  listening_action = true;
}


void History::EndAction() {
  listening_action = false;
}


// TODO: when inserting or removing text, instead of creating new change object
// try to merge it with the last change of the action if possible to prevent a
// bunch of objects eat RAM.
Cursor History::CommitInsertText(const Cursor& cursor_, const std::string& text) {

  // Make a copy of the given cursor modify and return.
  Cursor cursor = cursor_;

  Action& action = _GetListeningAction(cursor);

  if (cursor.HasSelection()) {
    Slice selection = cursor.GetSelection();
    int count = selection.end - selection.start;

    // It's possible for the selection to have the same start and end, and be
    // an empty selection.
    if (count != 0) {
      Change change;
      change.added = false;
      change.index = selection.start;
      change.text = buffer->GetSubString(selection.start, count);
      action.PushChange(std::move(change));

      buffer->RemoveText(selection.start, count);
      cursor.SetIndex(selection.start);
    }
    cursor.ClearSelection();
  }

  int cursor_index = cursor.GetIndex();

  Change change;
  change.added = true;
  change.index = cursor_index;
  change.text = text;
  action.PushChange(std::move(change));

  buffer->InsertText(cursor_index, text);

  int next_index = cursor_index + text.size();
  cursor.SetIndex(next_index);
  cursor.UpdateColumn();

  action.after = cursor;
  return cursor;
}


Cursor History::CommitRemoveText(const Cursor& cursor_, int direction) {
  ASSERT(-1 == direction || direction == +1, "Invalid value for direction.");

  // Make a copy of the given cursor modify and return.
  Cursor cursor = cursor_;

  Action& action = _GetListeningAction(cursor);

  Slice selection = cursor.GetSelection();
  int count = selection.end - selection.start;

  // If it's an empty selection, just ignore it and do regular backspace/delete
  // operation in the below else block.
  if (cursor.HasSelection() && count == 0) {
    cursor.ClearSelection();
  }

  if (cursor.HasSelection()) {
    Change change;
    change.added = false;
    change.index = selection.start;
    change.text = buffer->GetSubString(selection.start, count);
    action.PushChange(std::move(change));

    buffer->RemoveText(selection.start, count);

    cursor.SetIndex(selection.start);
    cursor.ClearSelection();
    cursor.UpdateColumn();

  } else {

    int index = cursor.GetIndex();
    bool can_remove = (
        (direction == -1 && index >= 1) ||
        (direction == +1 && index <= buffer->GetSize()-1));

    if (can_remove) {

      if (direction == -1) index--;

      Change change;
      change.added = false;
      change.index = index;
      change.text = buffer->GetSubString(index, 1);
      action.PushChange(std::move(change));

      buffer->RemoveText(index, 1);

      cursor.SetIndex(index);
      cursor.UpdateColumn();
    }
  }

  action.after = cursor;
  return cursor;
}


bool History::HasUndo() const {
  return ptr >= 1;
}


bool History::HasRedo() const {
  return ptr <= actions.size() - 1;
}


const Cursor& History::Undo() {
  ASSERT(HasUndo(), OOPS);
  const Action& action = actions[--ptr];

  for (int i = action.changes.size() -1; i >= 0; i--) {
    const Change& change = action.changes[i];
    if (change.added) {
      ASSERT(change.text == buffer->GetSubString(change.index, change.text.size()), OOPS);
      buffer->RemoveText(change.index, change.text.size());
    } else {
      buffer->InsertText(change.index, change.text);
    }
  }

  EndAction();
  return action.before;
}


const Cursor& History::Redo() {
  ASSERT(HasRedo(), OOPS);
  const Action& action = actions[ptr++];

  for (const Change& change : action.changes) {
    if (change.added) {
      buffer->InsertText(change.index, change.text);
    } else {
      ASSERT(change.text == buffer->GetSubString(change.index, change.text.size()), OOPS);
      buffer->RemoveText(change.index, change.text.size());
    }
  }

  EndAction();
  return action.after;
}


Action& History::_GetListeningAction(const Cursor& cursor) {

  if (!listening_action) {
    listening_action = true;

    if (ptr == actions.size()) {
      actions.push_back(Action());
      ptr++;
    } else {
      actions[ptr++] = Action();
      actions.erase(actions.begin() + ptr, actions.end());
    }

    Action& listening = actions[ptr-1];
    listening.before = cursor;
    return listening;
  }

  ASSERT(ptr == actions.size(), OOPS);
  ASSERT(ptr > 0, OOPS);
  Action& listening = actions[ptr-1];
  return listening;
}

