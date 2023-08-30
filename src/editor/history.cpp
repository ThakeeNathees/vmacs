// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "history.hpp"
#include "buffer.hpp"


Cursors History::CommitInsertText(Cursors cursors, const std::string& text) {
  ASSERT(buffer != nullptr, OOPS);

  std::unique_ptr<Action> action = Action::New(Action::Type::TEXT_INSERT);
  action->before = cursors;

  std::vector<Cursor>& curslist = cursors.Get();
  for (int i = 0; i < (int)curslist.size(); i++) {
    Cursor& cursor = curslist[i];

    std::unique_ptr<Action::TextDelta> delta = Action::TextDelta::New();
    delta->added = text;

    // +5 means 5 characters were added and
    // -5 means 5 characters were removed.
    int char_count_delta = 0;

    if (cursor.HasSelection()) {
      Slice selection = cursor.GetSelection();
      int count = selection.end - selection.start;

      delta->index = selection.start;
      delta->start = buffer->IndexToCoord(selection.start);
      delta->end = buffer->IndexToCoord(selection.end);

      delta->removed = buffer->GetSubstring(delta->index, count);

      buffer->RemoveText(selection.start, count);
      char_count_delta = -count;

      cursor.SetIndex(buffer, selection.start);
      cursor.ClearSelection();

    } else {
      delta->index = cursor.GetIndex();
      delta->start = buffer->IndexToCoord(delta->index);
      delta->end = delta->start;
    }

    buffer->InsertText(cursor.GetIndex(), text);
    char_count_delta += (int)text.size();

    cursor.SetIndex(buffer, cursor.GetIndex() + (int)text.size());
    cursor.UpdateColumn();

    // Move all the cursors after this one.
    for (int j = i + 1; j < (int)curslist.size(); j++) {
      Cursor& curs = curslist[j];
      curs.SetIndex(buffer, curs.GetIndex() + char_count_delta);
      if (curs.HasSelection()) {
        curs.SetSelectionStart(buffer, curs.GetSelectionStart() + char_count_delta);
      }
    }

    action->text_deltas.push_back(std::move(delta));
  }
  cursors.OnMoved(buffer);

  ASSERT(action->text_deltas.size() > 0, OOPS);
  action->after = cursors;
  _CommitAction(std::move(action));

  return cursors;
}


Cursors History::CommitRemoveText(Cursors cursors) {
  ASSERT(buffer != nullptr, OOPS);

  std::unique_ptr<Action> action = Action::New(Action::Type::TEXT_REMOVE);
  action->before = cursors;

  std::vector<Cursor>& curslist = cursors.Get();
  for (int i = 0; i < (int)curslist.size(); i++) {
    Cursor& cursor = curslist[i];

    std::unique_ptr<Action::TextDelta> delta = Action::TextDelta::New();

    int removed_char_count = 0;

    if (cursor.HasSelection()) {

      Slice selection = cursor.GetSelection();
      int count = selection.end - selection.start;

      delta->index = selection.start;
      delta->start = buffer->IndexToCoord(selection.start);
      delta->end = buffer->IndexToCoord(selection.end);

      delta->removed = buffer->GetSubstring(delta->index, count);

      buffer->RemoveText(selection.start, count);
      removed_char_count = count;

      cursor.SetIndex(buffer, selection.start);
      cursor.ClearSelection();

    } else {
      int index = cursor.GetIndex();
      if (index == 0) continue;

      // FIXME: This logic is written for backspace and not valid for delete.
      delta->index = index - 1;
      delta->start = buffer->IndexToCoord(delta->index);
      delta->end = buffer->IndexToCoord(delta->index + 1);

      delta->removed = buffer->GetSubstring(index - 1, 1);

      buffer->RemoveText(index - 1, 1);
      removed_char_count = 1;

      cursor.SetIndex(buffer, index - 1);
    }
    cursor.UpdateColumn();

    // Move all the cursors after this one.
    for (int j = i + 1; j < (int)curslist.size(); j++) {
      Cursor& curs = curslist[j];
      curs.SetIndex(buffer, curs.GetIndex() - removed_char_count);
      if (curs.HasSelection()) {
        curs.SetSelectionStart(buffer, curs.GetSelectionStart() - removed_char_count);
      }
    }

    action->text_deltas.push_back(std::move(delta));
  }
  cursors.OnMoved(buffer);

  // If we're at index 0 and try to backspace, nothing would be deleted so
  // the delta is zero and we don't have to commit it as an action.
  if (action->text_deltas.size() == 0) return action->before;

  action->after = cursors;
  _CommitAction(std::move(action));

  return cursors;
}


void History::_CommitAction(std::unique_ptr<Action> action) {
  ASSERT(ptr >= -1, OOPS);

  _UpdateMergability(*action);
  const Action* action_ptr = action.get();

  if (ptr >= 0 && _CanMerge(*actions[ptr], *action)) {
    _Merge(*actions[ptr], *action);
  } else if (ptr == actions.size() -1) {
    actions.push_back(std::move(action));
    ptr++;
  } else {
    actions[++ptr] = std::move(action);
  }

  // Remove all the future actions.
  actions.erase(actions.begin() + (ptr + 1), actions.end());

  _OnHistoryChanged(false, action_ptr);
}


void History::_UpdateMergability(Action& action) {

  switch (action.type) {

    case Action::Type::TEXT_INSERT: {
      for (const auto& delta : action.text_deltas) {
        if (delta->added.size() != 1) return;
      }
      action._mergable = true;
      action._whitespace = true;
      for (const auto& delta : action.text_deltas) {
        char c = delta->added[0];
        if (!(c == ' ' || c == '\t' || c == '\n')) {
          action._whitespace = false;
          return;
        }
      }
    } break;

    case Action::Type::TEXT_REMOVE: {
      action._mergable = true;
      action._whitespace = true;
      for (const auto& delta : action.text_deltas) {
        char c = delta->removed[0];
        if (!(c == ' ' || c == '\t' || c == '\n')) {
          action._whitespace = false;
          return;
        }
      }
    } break;
  }

  return;
}

bool History::_CanMerge(Action& on, const Action& next) {
#define return_cant_merge \
  do {										\
    on._mergable = false;	\
    return false;				  \
  } while (false)

  if (!on._mergable || !next._mergable) return_cant_merge;
  if (on.type != next.type) return false;
  if (!on.after.Equals(next.before)) return_cant_merge;

  switch (on.type) {
    case Action::Type::TEXT_INSERT: {
      for (const auto& delta : next.text_deltas) {
        if (delta->removed != "") return false;
      }
      if (on.text_deltas.size() != next.text_deltas.size()) return_cant_merge;
      if (on._whitespace) return true;
      if (next._whitespace) return_cant_merge;
      return true;
    } break;

    case Action::Type::TEXT_REMOVE: {
      if (on.text_deltas.size() != next.text_deltas.size()) return_cant_merge;
      if (on._whitespace) return true;
      if (next._whitespace) return_cant_merge;
      return true;
    } break;

    default:
      UNREACHABLE();

  }

#undef return_cant_merge
  return true;
}


void History::_Merge(Action& on, const Action& next) {
  ASSERT(on.after.Equals(next.before), OOPS);
  ASSERT(on.type == next.type, OOPS);

  switch (on.type) {
    case Action::Type::TEXT_INSERT: {
      ASSERT(on.text_deltas.size() == next.text_deltas.size(), OOPS);
      on._whitespace = next._whitespace;
      on.after = next.after;

      for (int i = 0; i < on.text_deltas.size(); i++) {
        on.text_deltas[i]->added += next.text_deltas[i]->added;
        for (int j = i + 1; j < on.text_deltas.size(); j++) {
          on.text_deltas[j]->index++;
        }
      }
    } break;

    case Action::Type::TEXT_REMOVE: {
      ASSERT(on.text_deltas.size() == next.text_deltas.size(), OOPS);
      on._whitespace = next._whitespace;
      on.after = next.after;
      for (int i = 0; i < on.text_deltas.size(); i++) {
        on.text_deltas[i]->removed.insert(0, next.text_deltas[i]->removed);
        // Since it'll remove one character fix the indexes of the next deltas.
        for (int j = i + 1; j < on.text_deltas.size(); j++) {
          on.text_deltas[j]->index--;
        }
      }

      // Unlike addition, removal changes the start index of the delta by -1.
      for (int i = 0; i < on.text_deltas.size(); i++) {
        on.text_deltas[i]->index--;
      }
    } break;

    default:
    UNREACHABLE();

  }
}


// Since we're appling the delta from the first cursor to the last cursor, we
// have to do the undo from the last cursor to the fist cursor, but the redo
// the other way around.
const Cursors& History::Undo() {
  ASSERT(HasUndo() && buffer != nullptr, OOPS);
  const Action& action = *actions[ptr--];

  switch (action.type) {

    case Action::Type::TEXT_INSERT: {
      for (int i = (int)action.text_deltas.size() - 1; i >= 0; i--) {
        const auto& delta = action.text_deltas[i];
        ASSERT(delta->added == buffer->GetSubstring(delta->index, (int)delta->added.size()), OOPS);
        buffer->RemoveText(delta->index, (int)delta->added.size());
        if (delta->removed.size() > 0) buffer->InsertText(delta->index, delta->removed);
      }
    } break;

    case Action::Type::TEXT_REMOVE: {
      for (int i = (int)action.text_deltas.size() - 1; i >= 0; i--) {
        const auto& delta = action.text_deltas[i];
        ASSERT(delta->added.size() == 0, OOPS);
        buffer->InsertText(delta->index, delta->removed);
      }
    } break;
  }

  _OnHistoryChanged(true, &action);
  return action.before;
}


const Cursors& History::Redo() {
  ASSERT(HasRedo() && buffer != nullptr, OOPS);
  const Action& action = *actions[++ptr];

  switch (action.type) {

    case Action::Type::TEXT_INSERT: {
      for (const auto& delta : action.text_deltas) {
        ASSERT(delta->removed == buffer->GetSubstring(delta->index, (int)delta->removed.size()), OOPS);
        buffer->RemoveText(delta->index, (int)delta->removed.size());
        buffer->InsertText(delta->index, delta->added);
      }
    } break;

    case Action::Type::TEXT_REMOVE: {
      for (const auto& delta : action.text_deltas) {
        ASSERT(delta->added.size() == 0, OOPS);
        ASSERT(delta->removed == buffer->GetSubstring(delta->index, (int)delta->removed.size()), OOPS);
        buffer->RemoveText(delta->index, (int)delta->removed.size());
      }
    } break;
  }

  _OnHistoryChanged(false, &action);
  return action.after;
}


void History::Clean() {
  actions.erase(actions.begin(), actions.end());
  ptr = -1;
}


void History::SetBuffer(Buffer* buffer) {
  ASSERT(this->buffer == nullptr, "Trying to set buffer to history multiple times.");
  this->buffer = buffer;
}


void History::RegisterListener(HistoryListener* listener) {
  listeners.push_back(listener);
}


void History::_OnHistoryChanged(bool undo, const Action* action) {
  for (HistoryListener* listener : listeners) {
    listener->OnHistoryChanged(this, undo, action);
  }
}
