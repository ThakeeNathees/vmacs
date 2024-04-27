//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "document.hpp"


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


uint32_t History::GetVersion() const {
  return version;
}


void History::StartAction() {
  listening_action = true;
}


void History::EndAction() {
  listening_action = false;
}


MultiCursor History::CommitInsertText(const MultiCursor& cursors_, const std::string& text) {

  // Make a copy of the given cursors modify and return.
  MultiCursor cursors = cursors_;

  Action& action = _GetListeningAction(cursors);

  std::vector<DocumentChange> lsp_changes; // Required to send to lsp server.

  std::vector<Cursor>& curslist = cursors.Get();
  for (int i = 0; i  < curslist.size(); i++) {
    Cursor& cursor = curslist[i];

    // We need to keep track of the number of characters added/removed
    // eventually to move the cursors comes after this one.
    // +5 means 5 characters were added and
    // -5 means 5 characters were removed.
    int delta_next_cursor = 0;

    DocumentChange lsp_change;

    if (cursor.HasSelection()) {
      Slice selection = cursor.GetSelection();
      int count = selection.end - selection.start;

      // Update the lsp change.
      lsp_change.start = buffer->IndexToCoord(selection.start);
      lsp_change.end   = buffer->IndexToCoord(selection.end);
      lsp_change.text  = text;

      // It's possible for the selection to have the same start and end, and be
      // an empty selection.
      if (count != 0) {

        // Update the action.
        Change change;
        change.added = false;
        change.index = selection.start;
        change.text = buffer->GetSubString(selection.start, count);
        action.PushChange(std::move(change));

        // Update the buffer.
        buffer->RemoveText(selection.start, count);
        delta_next_cursor -= count;
        cursor.SetIndex(selection.start);
      }

     cursor.ClearSelection();

    } else {
      // Update the lsp change.
      lsp_change.start = cursor.GetCoord();
      lsp_change.end   = cursor.GetCoord();
      lsp_change.text  = text;
    }

    // Current index after the selection is removed (if any).
    const int cursor_index = cursor.GetIndex();

    // Update the action.
    Change change;
    change.added = true;
    change.index = cursor_index;
    change.text = text;
    action.PushChange(std::move(change));

    // Push the lsp change.
    lsp_changes.push_back(lsp_change);

    // Update the buffer.
    buffer->InsertText(cursor_index, text);
    delta_next_cursor += text.size();

    int next_index = cursor_index + text.size();
    cursor.SetIndex(next_index);
    cursor.UpdateColumn();

    // Move all the cursors after this one.
    for (int j = i + 1; j < (int)curslist.size(); j++) {
      Cursor& curs = curslist[j];
      curs.SetIndex(curs.GetIndex() + delta_next_cursor);
      if (curs.HasSelection()) {
        curs.SetSelectionStart(curs.GetSelectionStart() + delta_next_cursor);
      }
    }
  }

  // Sort and merge cursors if required.
  cursors.Changed();

  action.after = cursors;
  version++;

  // Notify the history changes.
  OnHistoryChanged(lsp_changes);
  return cursors;
}


MultiCursor History::CommitRemoveText(const MultiCursor& cursors_, int direction) {
  ASSERT(-1 == direction || direction == +1, "Invalid value for direction.");

  // Make a copy of the given cursors modify and return.
  MultiCursor cursors = cursors_;

  Action& action = _GetListeningAction(cursors);

  // It's possible that the history doesn't changed by the remove commit since
  // there isn't anything to remove. We'll notify OnHistoryChanged if only there
  // is an actual change.
  bool history_changed = false;

  std::vector<DocumentChange> lsp_changes; // Required to send to lsp server.

  std::vector<Cursor>& curslist = cursors.Get();
  for (int i = 0; i < curslist.size(); i++) {
    Cursor& cursor = curslist[i];

    // We need to keep track of the number of characters added/removed
    // eventually to move the cursors comes after this one.
    // +5 means 5 characters were added and
    // -5 means 5 characters were removed.
    int delta_next_cursor = 0;

    DocumentChange lsp_change;

    // If it's an empty selection, just ignore it and do regular backspace/delete
    // operation in the below else block.
    Slice selection = cursor.GetSelection();
    int count = selection.end - selection.start;
    if (cursor.HasSelection() && count == 0) {
      cursor.ClearSelection();
    }

    if (cursor.HasSelection()) {
      // Update the action.
      Change change;
      change.added = false;
      change.index = selection.start;
      change.text = buffer->GetSubString(selection.start, count);
      action.PushChange(std::move(change));

      // Update the lsp changes.
      lsp_change.start = buffer->IndexToCoord(selection.start);
      lsp_change.end   = buffer->IndexToCoord(selection.end);
      lsp_change.text  = "";
      lsp_changes.push_back(lsp_change);

      // Update the buffer.
      buffer->RemoveText(selection.start, count); 
      history_changed = true;
      delta_next_cursor -= count;

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

        // Update the action.
        Change change;
        change.added = false;
        change.index = index;
        change.text  = buffer->GetSubString(index, 1);
        action.PushChange(std::move(change));

        // Update the lsp changes.
        lsp_change.start = buffer->IndexToCoord(index);
        lsp_change.end   = buffer->IndexToCoord(index + 1);
        lsp_change.text  = "";
        lsp_changes.push_back(lsp_change);

        // Update the buffer.
        buffer->RemoveText(index, 1);
        history_changed  = true;
        delta_next_cursor -= 1;

        cursor.SetIndex(index);
        cursor.UpdateColumn();

      }
    } // !HasSelection().

    // Move all the cursors after this one.
    for (int j = i + 1; j < curslist.size(); j++) {
      Cursor& curs = curslist[j];
      curs.SetIndex(curs.GetIndex() + delta_next_cursor);
      if (curs.HasSelection()) {
        curs.SetSelectionStart(curs.GetSelectionStart() + delta_next_cursor);
      }
    }
  } // Each cursor loop.

  // We backspace/delet into nothing and there is no changes. Just return.
  if (!history_changed) return cursors;

  // Sort and merge cursors if required.
  cursors.Changed();

  action.after = cursors;
  version++;

  // Notify the history changes.
  OnHistoryChanged(lsp_changes);
  return cursors;
}


bool History::HasUndo() const {
  return ptr >= 1;
}


bool History::HasRedo() const {
  return ptr <= actions.size() - 1;
}


const MultiCursor& History::Undo() {
  ASSERT(HasUndo(), OOPS);
  const Action& action = actions[--ptr];
  std::vector<DocumentChange> lsp_changes;

  for (int i = action.changes.size() -1; i >= 0; i--) {
    const Change& change = action.changes[i];
    DocumentChange lsp_change;

    if (change.added) {
      ASSERT(change.text == buffer->GetSubString(change.index, change.text.size()), OOPS);
      lsp_change.start = buffer->IndexToCoord(change.index);
      lsp_change.end   = buffer->IndexToCoord(change.index + change.text.size());
      lsp_change.text  = "";
      lsp_changes.push_back(lsp_change);
      buffer->RemoveText(change.index, change.text.size());
    } else {
      lsp_change.start = buffer->IndexToCoord(change.index);
      lsp_change.end   = lsp_change.start;
      lsp_change.text  = change.text;
      lsp_changes.push_back(lsp_change);
      buffer->InsertText(change.index, change.text);
    }
  }
  version++;

  EndAction();

  OnHistoryChanged(lsp_changes);
  return action.before;
}


const MultiCursor& History::Redo() {
  ASSERT(HasRedo(), OOPS);
  const Action& action = actions[ptr++];
  std::vector<DocumentChange> lsp_changes;

  for (const Change& change : action.changes) {
    DocumentChange lsp_change;
    if (change.added) {
      lsp_change.start = buffer->IndexToCoord(change.index);
      lsp_change.end   = lsp_change.start;
      lsp_change.text  = change.text;
      lsp_changes.push_back(lsp_change);
      buffer->InsertText(change.index, change.text);
    } else {
      ASSERT(change.text == buffer->GetSubString(change.index, change.text.size()), OOPS);
      lsp_change.start = buffer->IndexToCoord(change.index);
      lsp_change.end   = buffer->IndexToCoord(change.index + change.text.size());
      lsp_change.text  = "";
      lsp_changes.push_back(lsp_change);
      buffer->RemoveText(change.index, change.text.size());
    }
  }
  version++;

  EndAction();

  OnHistoryChanged(lsp_changes);
  return action.after;
}


Action& History::_GetListeningAction(const MultiCursor& cursors) {

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
    listening.before = cursors;
    return listening;
  }

  ASSERT(ptr == actions.size(), OOPS);
  ASSERT(ptr > 0, OOPS);
  Action& listening = actions[ptr-1];
  return listening;
}

void History::RegisterListener(HistoryListener* listener) {
  listeners.push_back(listener);
}


void History::UnRegisterListener(HistoryListener* listener) {
  for (int i = 0; i < listeners.size(); i++) {
    if (listeners[i] == listener) {
      listeners.erase(listeners.begin() + i);
      return;
    }
  }
}


void History::OnHistoryChanged(const std::vector<DocumentChange>& changes) {
  for (HistoryListener* listener : listeners) {
    listener->OnHistoryChanged(changes);
  }
}
