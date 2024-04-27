//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT


#include "core/core.hpp"
#include "document.hpp"


Document::Document(const Uri& uri, std::shared_ptr<Buffer> buffer)
  : uri(uri), buffer(buffer), cursors(buffer.get()), history(buffer)
{
  history.RegisterListener(this);
  buffer->RegisterListener(this);
}


Document::~Document() {
  history.UnRegisterListener(this);
}


LanguageId Document::GetLanguageId() const {
  if (language) return language->id;
  return "";
}


bool Document::IsReadOnly() const {
  return readonly;
}


void Document::SetReadOnly(bool readonly) {
  this->readonly = readonly;
}


void Document::SetLanguage(std::shared_ptr<const Language> language) {
  this->language = language;
  syntax.Parse(language.get(), buffer.get());
}


int Document::GetWordBeforeIndex(int index) const {
  // First we skip all the white space characters (vim doesn't skip new line.
  // something to consider).
  while (index > 0 && IsCharWhitespace(buffer->At(index))) index--;

  if (IsCharName(buffer->At(index))) {
    while (index > 1 && IsCharName(buffer->At(index-1))) index--;
    return index;
  }

  while (index > 1 && !IsCharName(buffer->At(index-1))) index--;
  return index;
}


std::unique_lock<std::mutex> Document::GetDiagnosticAt(const Diagnostic** diagnos, int index) {

  // FIXME: If the file has too many errors an error diagnostic will be reported
  // where the start and end coord will be (0, 0) and (0, 0) Since it's not
  // actually belongs to any index, we should check if the first diagnostic in
  // the sorted (TODO) diagnostics list and index == 0 and return the diagnostic.

  {
    std::unique_lock<std::mutex> lock(mutex_diagnostics);
    Coord coord = buffer->IndexToCoord(index);

    // TODO: Sort the diagnostics when recieved and do a binary search.
    for (Diagnostic& diag : diagnostics) {

      // Check if the coordinate comes before this diagnostic.
      if ((coord.row < diag.start.row) ||
          (coord.row == diag.start.row && coord.col < diag.start.col)) {
        continue;
      }

      // Check if the coordinate comes after this diagnostic.
      if ((coord.row > diag.end.row) ||
          (coord.row == diag.end.row && coord.col >= diag.end.col)) {
        continue;
      }

      *diagnos = &diag;
      return std::move(lock);
    }
  }

  *diagnos = nullptr;
  return std::unique_lock<std::mutex>();
}


std::unique_lock<std::mutex> Document::GetCompletionItems(std::vector<CompletionItem>** items) {
  ASSERT(items != nullptr, OOPS);
  std::unique_lock<std::mutex> lock(mutex_completions);
  *items = &completion_items;
  return std::move(lock);
}


std::unique_lock<std::mutex> Document::GetSignatureHelp(SignatureItems** items) {
  ASSERT(items != nullptr, OOPS);
  std::unique_lock<std::mutex> lock(mutex_signature_help);
  *items = &signatures_helps;
  return std::move(lock);
}


// Fixme: this is not how we set client (mess). it'll be set when we call
// SetLanguage() above.
void Document::SetLspClient(std::shared_ptr<LspClient> client) {
  if (language == nullptr) return;
  lsp_client = client;
  lsp_client->DidOpen(uri, buffer->GetData(), "cpp");
}


void Document::ClearCompletionItems() {
  {
    std::lock_guard<std::mutex> loc(mutex_completions);
    completion_items.clear();
    completion_selected    = -1;
    completion_start_index = -1;
  }

  {
    std::lock_guard<std::mutex> loc(mutex_signature_help);
    signatures_helps.signatures.clear();
    signatures_helps.active_signature = -1;
    signatures_helps.active_parameter = -1;
  }
}


void Document::PushDiagnostics(uint32_t version, std::vector<Diagnostic>&& diagnostics) {
  {
    // This function is a callback called from from LSP's IO thread. So we need to
    // lock the mutex of the diagnostics vector. This will clear our current
    // diagnostics and update with the new values.
    std::lock_guard<std::mutex> lock(mutex_diagnostics);

    if (version == this->history.GetVersion()) {
      this->diagnostics = diagnostics;
    } else {
      this->diagnostics.clear();
    }
  }
}


void Document::PushCompletions(bool is_incomplete, std::vector<CompletionItem>&& items) {
  {
    // This will clear our current diagnostics and update with the new values.
    // Note that we don't reset 'completion_start_index' since we may already
    // doing the completion and this response just filters the list.
    std::lock_guard<std::mutex> lock(mutex_completions);
    completion_items = items;
    completion_selected = -1;
    this->is_incomplete = is_incomplete;
    std::sort(this->completion_items.begin(), this->completion_items.end(),
        [](const CompletionItem& l, const CompletionItem& r){
          return l.sort_text < r.sort_text;
        });

  }
}


void Document::PushSignatureHelp(SignatureItems&& items) {
  {
    std::lock_guard<std::mutex> lock(mutex_signature_help);
    this->signatures_helps = items;
  }
}


void Document::OnHistoryChanged(const std::vector<DocumentChange>& changes) {
  // TODO: Consider moving the diagnostics coordinates a little till the next
  // diagnostics comes from the server or maybe consider removing the current
  // diagnostics after a certain time in the update() method.

  if (lsp_client) {
    lsp_client->DidChange(uri, history.GetVersion(), changes);
  }
}


void Document::OnBufferChanged() {
  syntax.Parse(language.get(), buffer.get());
}


// -----------------------------------------------------------------------------
// Actions.
// -----------------------------------------------------------------------------

void Document::EnterCharacter(char c) {
  InsertText(std::string(1, c));

  if (!lsp_client) return;

  // Trigger signature help if it's a triggering character.
  if (lsp_client->IsTriggeringCharacterSignature(c)) {
    TriggerSignatureHelp();
  }
  // Note that if it's not a signature triggering character, we don't clear the
  // signature help, since the user still typeing the parameters and we want to
  // continue showing the signature help till it's cleared by some other means.

  {
    bool is_ch_trigger = lsp_client->IsTriggeringCharacterCompletion(c);
    std::lock_guard<std::mutex> lock(mutex_completions);
    if (!is_ch_trigger) {
      // Only clear the completion items (signature help might be valid).
      completion_items.clear();
      completion_selected = -1;
      return;
    }

    // We have inserted a character so all the past completion item text_edit
    // range will be outdated by 1 character, update it here.
    for (auto& item : completion_items) {
      item.text_edit.end.col += 1;
    }
  }

  // This is the proper way instead of requesting the server every time, but in that
  // case we have to do fuzzy match filter the existing list and we need to figure
  // out where to insert the text (range and new text alrady in the completion item).
  // 
  // { // Check if already in completion.
  //   std::lock_guard<std::mutex> lock(mutex_completions);
  //   if (!completion_items.empty() && !is_incomplete) {
  //     return;
  //   }
  // }
  TriggerCompletion();

  // Set the completion start index if we're not in completion already.
  // Note that we already inserted the character and the cursor has moved, so
  // just -1 to get the index.
  if (completion_start_index < 0) {
    completion_start_index = cursors.GetPrimaryCursor().GetIndex() - 1;
  }
}


void Document::InsertText(const std::string& text) {
  cursors = history.CommitInsertText(cursors, text);
}


void Document::Backspace() {
  cursors = history.CommitRemoveText(cursors, -1);
  ClearCompletionItems();
}


void Document::Undo() {
  if (!history.HasUndo()) return;
  cursors = history.Undo();
  ClearCompletionItems();
}


void Document::Redo() {
  if (!history.HasRedo()) return;
  cursors = history.Redo();
  ClearCompletionItems();
}


void Document::CursorRight() {
  for (Cursor& cursor : cursors.Get()) {

    if (cursor.HasSelection()) {
      const Slice selection = cursor.GetSelection();
      cursor.SetIndex(selection.end);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() < buffer->GetSize()) {
      cursor.SetIndex(cursor.GetIndex() + 1);
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::CursorLeft() {
  for (Cursor& cursor : cursors.Get()) {

    if (cursor.HasSelection()) {
      const Slice selection = cursor.GetSelection();
      cursor.SetIndex(selection.start);
      cursor.ClearSelection();
    } else if (cursor.GetIndex() > 0) {
      cursor.SetIndex(cursor.GetIndex() - 1);
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::CursorUp() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    const Coord coord = cursor.GetCoord();

    if (coord.row == 0) {
      cursor.SetIndex(0);
      cursor.SetColumn(0);
    } else {
      const int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row-1);
      cursor.SetIndex(index);
    }

  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::CursorDown() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    const Coord coord = cursor.GetCoord();
    const int line_count = buffer->GetLineCount();

    if (coord.row == buffer->GetLineCount() - 1) {
      cursor.SetIndex(buffer->GetLine(line_count-1).end);
      cursor.UpdateColumn();
    } else {
      int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row+1);
      cursor.SetIndex(index);
    }

  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::CursorHome() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    const Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);

    // "ptr" will be the index of first non whitespace character.
    int ptr = line.start, buffsize = buffer->GetSize();
    for (; ptr <= line.end && ptr < buffsize; ptr++) {
      if (buffer->At(ptr) != ' ' && buffer->At(ptr) != '\t') break;
    }

    if (coord.col == 0 || coord.col > ptr - line.start) {
      cursor.SetIndex(ptr);
    } else {
      cursor.SetIndex(line.start);
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::CursorEnd() {
  for (Cursor& cursor : cursors.Get()) {
    cursor.ClearSelection();
    Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);
    cursor.SetIndex(line.end);
    cursor.UpdateColumn();
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::SelectRight() {
  for (Cursor& cursor : cursors.Get()) {
    const int index = cursor.GetIndex();

    if (index < buffer->GetSize()) {
      if (!cursor.HasSelection()) cursor.SetSelectionStart(index);
      cursor.SetIndex(index + 1);
      if (cursor.GetSelectionStart() == cursor.GetIndex()) {
        cursor.ClearSelection();
      }
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::SelectLeft() {
  for (Cursor& cursor : cursors.Get()) {
    int index = cursor.GetIndex();

    if (index > 0) {
      if (!cursor.HasSelection()) cursor.SetSelectionStart(index);
      cursor.SetIndex(index - 1);
      if (cursor.GetSelectionStart() == cursor.GetIndex()) {
        cursor.ClearSelection();
      }
    }

    cursor.UpdateColumn();
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::SelectUp() {
  for (Cursor& cursor : cursors.Get()) {
    Coord coord = cursor.GetCoord();
    const int cursor_index = cursor.GetIndex();

    // Edge case, don't select anything just return.
    if (cursor_index == 0) continue;

    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(cursor_index);
    }

    if (coord.row == 0) {
      cursor.SetIndex(0);
      cursor.UpdateColumn();
      continue;
    }

    int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row-1);
    cursor.SetIndex(index);

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }

  cursors.Changed();
  ClearCompletionItems();
}


void Document::SelectDown() {
  for (Cursor& cursor : cursors.Get()) {
    Coord coord = cursor.GetCoord();
    int line_count = buffer->GetLineCount();
    int cursor_index = cursor.GetIndex();
    int buffsz = buffer->GetSize();

    // Edge case, don't select anything just return.
    if (coord.row == line_count - 1 && cursor_index == buffsz) continue;

    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(cursor_index);
    }

    if (coord.row == line_count - 1) {
      Slice last_line = buffer->GetLine(line_count-1);
      cursor.SetIndex(last_line.end);
      cursor.UpdateColumn();
      continue;
    }

    int index = buffer->ColumnToIndex(cursor.GetColumn(), coord.row+1);
    cursor.SetIndex(index);

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }

  cursors.Changed();
  ClearCompletionItems();
}


void Document::SelectHome() {
  for (Cursor& cursor : cursors.Get()) {
    const Coord coord = cursor.GetCoord();
    const Slice& line = buffer->GetLine(coord.row);
    const int index = cursor.GetIndex();

    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(index);
    }

    // "ptr" will be the index of first non whitespace character.
    int ptr = line.start, buffsize = buffer->GetSize();
    for (; ptr <= line.end && ptr < buffsize; ptr++) {
      if (buffer->At(ptr) != ' ' && buffer->At(ptr) != '\t') break;
    }

    if (coord.col == 0 || coord.col > ptr - line.start) {
      cursor.SetIndex(ptr);
    } else {
      cursor.SetIndex(line.start);
    }

    cursor.UpdateColumn();

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::SelectEnd() {
  for (Cursor& cursor : cursors.Get()) {
    Coord coord = cursor.GetCoord();
    Slice line = buffer->GetLine(coord.row);

    if (coord.col == (line.end-line.start)) continue;

    int cursor_index = cursor.GetIndex();
    if (!cursor.HasSelection()) {
      cursor.SetSelectionStart(cursor_index);
    }

    cursor.SetIndex(line.end);
    cursor.UpdateColumn();

    if (cursor.GetSelectionStart() == cursor.GetIndex()) {
      cursor.ClearSelection();
    }
  }
  cursors.Changed();
  ClearCompletionItems();
}


void Document::AddCursorDown() {
  cursors.AddCursorDown();
  ClearCompletionItems();
}


void Document::AddCursorUp() {
  cursors.AddCursorUp();
  ClearCompletionItems();
}


void Document::TriggerCompletion() {
  if (lsp_client) {
    lsp_client->Completion(uri, cursors.GetPrimaryCursor().GetCoord());
  }
}


void Document::TriggerSignatureHelp() {
  if (lsp_client) {
    lsp_client->SignatureHelp(uri, cursors.GetPrimaryCursor().GetCoord());
  }
}


void Document::CycleCompletionList() {
  {
    std::lock_guard<std::mutex> lock(mutex_completions);
    if (completion_items.size() == 0) completion_selected = -1;
    else {
      completion_selected = (completion_selected + 1) % completion_items.size();
    }
  }
}


void Document::CycleCompletionListReversed() {
  {
    std::lock_guard<std::mutex> lock(mutex_completions);
    if (completion_items.size() == 0) completion_selected = -1;
    else {
      completion_selected--;
      if (completion_selected < 0) {
        completion_selected = completion_items.size() - 1;
      }
    }
  }
}


void Document::SelectCompletionItem() {
  {
    std::lock_guard<std::mutex> lock(mutex_completions);
    if (completion_items.size() == 0 || !BETWEEN(0, completion_selected, completion_items.size()-1)) return;
    const CompletionItem& item = completion_items[completion_selected];

    Cursor& cursor = cursors.GetPrimaryCursor();
    int start_index = (BETWEEN(0, completion_start_index, buffer->GetSize()-1))
      ? completion_start_index
      : GetWordBeforeIndex(cursor.GetIndex());

    cursors.ClearMultiCursors();
    cursor.SetSelectionStart(start_index);

    // Fill the text based on the order: insert_text or text_edit or label.
    if (!item.insert_text.empty()) InsertText(item.insert_text);
    else if (!item.text_edit.text.empty()) InsertText(item.text_edit.text);
    else InsertText(item.label);
  }
}

