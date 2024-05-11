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
  // opt.file           = "fd";
  // opt.argv           = StringSplit("--type f", ' ');
  // opt.file           = "find";
  // opt.argv           = StringSplit(". -type f", ' ');
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

    // Nothing to filter, just delete the un-wanted thread.
    ipc_filter = nullptr;

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
  // opt.file           = "grep"; // Use grep if fzf not available.
  // opt.argv           = { search_text };
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

  Coord coord(-1, -1); // Invalid coord will jump to the current cursor of that document.
  Editor* e = Editor::Singleton().get();
  return ((Ui*)e->GetUi())->JumpToDocument(path, coord);
}


// -----------------------------------------------------------------------------
// Live Grep.
// -----------------------------------------------------------------------------


void LiveGrep::Initialize() {
}


void LiveGrep::InputChanged() {
  if (search_text.empty()) {
    {
      std::lock_guard<std::mutex> lock(mutex_filters);
      filters.clear();
    }
    ipc_filter = nullptr; // Delete the search thread.
  } else {
    TriggerGrep();
  }

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

  // TODO:
  // - This contains line number and column, open the document and set the cursor
  //   to that specific location, (this behaviour can be re-used with lsp goto
  //   definition and jumplist).
  // - Implement a "parser" to parse "<file/path>:<line>:<character>:<rest-of-the-text>"
  //
  std::vector<std::string> splitted = StringSplit(item, ':');
  if (splitted.size() < 3) return false;

  Path path(splitted[0]);
  Coord coord(0, 0);

  // If for some reason the coordinate failed to parse, we'll still proceed with
  // the path and any valid coordinate will be set bellow.
  try {
    coord.line = std::stoi(splitted[1]) - 1;
    // Note that we're not setting the character of the coord so it'll always
    // jumps to the line we needed.
    // coord.character = std::stoi(splitted[2]) - 1;
  } catch (std::exception ex) { }


  Editor* e = Editor::Singleton().get();
  return ((Ui*)e->GetUi())->JumpToDocument(path, coord);
}
