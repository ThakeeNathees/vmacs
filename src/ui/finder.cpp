//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "finder.hpp"
#include "ui.hpp"


void Finder::RegisterItemsChangeListener(CallbackFinderItemsChanged cb_filter) {
  cb_items_changed = cb_filter;
}


void Finder::StdoutCallbackLoadResults(
    void* data, const char* buff, size_t length,
    std::string& pending,
    std::mutex& mutex, std::vector<std::string>& lines,
    CallbackFinderItemsChanged cb) {
  {

    std::lock_guard<std::mutex> lock(mutex);

    const char* end = buff;

    while (length--) {
      if (*end == '\n') {
        size_t len = (end-buff); // This will skip the '\n' character, +1 if needed.
        if (len != 0) {
          if (!pending.empty()) {
            lines.push_back(pending + std::string(buff, len));
            pending.clear();
          } else {
            lines.push_back(std::string(buff, len));
          }
        }
        buff = end+1;
      }
      end++;
    }

    if (buff != end) {
      ASSERT(*end == '\0', OOPS);
      pending += std::string(buff, (end-buff));
    }
  }

  if (cb) cb();
}


std::unique_lock<std::mutex> Finder::GetItems(const std::vector<std::string>** ret) {
  // If search text is empty we send the un-filtered items.
  if (search_text.empty()) {
    std::unique_lock<std::mutex> lock(mutex_total);
    *ret = &total;
    return std::move(lock);
  }

  std::unique_lock<std::mutex> lock(mutex_filters);
  *ret = &filters;
  return std::move(lock);
}


int Finder::GetItemsCount() {
  if (search_text.empty()) return GetItemsCountTotal();

  std::lock_guard<std::mutex> lock(mutex_filters);
  return filters.size();
}


int Finder::GetItemsCountTotal() {
  std::lock_guard<std::mutex> lock(mutex_total);
  return total.size();
}


std::string& Finder::GetSearchText() {
  return search_text;
}


// ----------------------------------------------------------------------------
// Files Finder
// ----------------------------------------------------------------------------

void FilesFinder::Initialize() {
  IPC::IpcOptions opt; // FIXME(grep): we need to do this properly.
  opt.user_data      = this;
  opt.file           = "rg";
  opt.argv           = { "--files" };
  opt.timeout_sec    = 4;
  opt.sending_inputs = false;
  opt.stdout_cb      = [&](void* data, const char* buff, size_t length) {
    Finder::StdoutCallbackLoadResults(
        data, buff, length,
        pending_results,
        mutex_total, total,
        cb_items_changed);
  };
  ipc_total        = IPC::New(opt);
  ipc_total->Run();
}


void FilesFinder::InputChanged() {
  if (search_text.empty()) {
    {
      std::lock_guard<std::mutex> lock_r(mutex_total);
      std::lock_guard<std::mutex> lock_f(mutex_filters);
      filters = total; // Copy all the values from results.
    }
  } else {
    TriggerFuzzyFilter();
  }
}


// FIXME: This method is not final.
void FilesFinder::TriggerFuzzyFilter() {
  { // This scope is absolutely necessary here otherwise the bellow destrucor
    // of IPC will cause a deadlock.
    std::lock_guard<std::mutex> lock_filter(mutex_filters);
    filters.clear();
  }

  IPC::IpcOptions opt; // FIXME: This needs to implemented properly.
  opt.user_data      = this;
  opt.file           = "fzf";
  opt.argv           = { "--filter", search_text };
  opt.timeout_sec    = 2;
  opt.sending_inputs = true;
  opt.stdout_cb      = [&](void* data, const char* buff, size_t length) {
    Finder::StdoutCallbackLoadResults(
        data, buff, length,
        pending_filter,
        mutex_filters, filters,
        cb_items_changed);
  };

  // This will destroy the old ipc and starts a new one.
  ipc_filter         = IPC::New(opt);

  {
    std::lock_guard<std::mutex> lock_total(mutex_total);
    for (auto& s : total) {
      ipc_filter->WriteToStdin(s + "\n");
    }
    ipc_filter->StopListening();
    ipc_filter->Run();
  }

}


bool FilesFinder::SelectItem(const std::string& item) {
  Path path(item);

  // TODO: Error to editor.
  if (!path.Exists()) return false;

  // TODO: If document is nullptr report error to editor.
  Editor* e = Editor::Singleton().get();
  std::shared_ptr<Document> doc = e->OpenDocument(path);
  if (doc == nullptr) return false;

  // FIXME(grep): Implement tab from window and call it here.
  std::unique_ptr<DocumentWindow> docwin = std::make_unique<DocumentWindow>(doc);
  std::unique_ptr<Split> root = std::make_unique<Split>();
  root->SetWindow(std::move(docwin));

  // TODO: If the file already opened in another tab, just set it active.
  std::unique_ptr<Tab> tab = std::make_unique<Tab>(std::move(root));
  ((Ui*)e->GetUi())->AddTab(std::move(tab));
  return true;
}


// -----------------------------------------------------------------------------
// Live Grep.
// -----------------------------------------------------------------------------


void LiveGrep::Initialize() {
}


void LiveGrep::InputChanged() {
  TriggerGrep();
}


void LiveGrep::TriggerGrep() {
  { // This scope is absolutely necessary here otherwise the bellow destrucor
    // of IPC will cause a deadlock.
    std::lock_guard<std::mutex> lock_filter(mutex_filters);
    filters.clear();
  }

  IPC::IpcOptions opt; // FIXME: This needs to implemented properly.
  opt.user_data      = this;
  opt.file           = "rg";
  opt.argv           = {
    "--color=never",
    "--no-heading",
    "--with-filename",
    "--line-number",
    "--column",
    "--smart-case",
    search_text,
  };
  opt.timeout_sec    = 2;
  opt.sending_inputs = false;
  opt.stdout_cb      = [&](void* data, const char* buff, size_t length) {
    Finder::StdoutCallbackLoadResults(
        data, buff, length,
        pending_filter,
        mutex_filters, filters,
        cb_items_changed);
  };

  // This will destroy the old ipc and starts a new one.
  ipc_filter         = IPC::New(opt);
  ipc_filter->Run();
}


bool LiveGrep::SelectItem(const std::string& item) {

  // TODO: This contains line number and column, open the document and set the cursor
  // to that specific location, (this behaviour can be re-used with lsp goto definition and jumplist).
  std::vector<std::string> splitted = StringSplit(item, ':');
  if (splitted.empty()) return false;

  // TODO: the bellow logic should be moved since it's re-usable.

  Path path(splitted[0]);

  // TODO: Error to editor.
  if (!path.Exists()) return false;

  // TODO: If document is nullptr report error to editor.
  Editor* e = Editor::Singleton().get();
  std::shared_ptr<Document> doc = e->OpenDocument(path);
  if (doc == nullptr) return false;

  // FIXME(grep): Implement tab from window and call it here.
  std::unique_ptr<DocumentWindow> docwin = std::make_unique<DocumentWindow>(doc);
  std::unique_ptr<Split> root = std::make_unique<Split>();
  root->SetWindow(std::move(docwin));

  // TODO: If the file already opened in another tab, just set it active.
  std::unique_ptr<Tab> tab = std::make_unique<Tab>(std::move(root));
  ((Ui*)e->GetUi())->AddTab(std::move(tab));
  return true;
}
