//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "finder.hpp"


void Finder::RegisterItemsChangeListener(CallbackFinderItemsChanged cb) {
  cb_item_changed = cb;
}


void Finder::ItemsChanged() {
  if (cb_item_changed) cb_item_changed(); 
}


void FilesFinder::Initialize() {
  IPC::IpcOptions opt; // FIXME(grep): we need to do this properly.
  opt.user_data      = this;
  opt.file           = "rg";
  opt.argv           = { "--files" };
  opt.timeout_sec    = 4;
  opt.sending_inputs = false;
  opt.stdout_cb      = FilesFinder::StdoutCallbackResults;
  ipc_results        = IPC::New(opt);
  ipc_results->Run();
}


std::unique_lock<std::mutex> FilesFinder::GetFilteredItems(const std::vector<std::string>** ret) {
  std::unique_lock<std::mutex> lock(mutex_filters);
  *ret = &filters;
  return std::move(lock);
}


std::unique_lock<std::mutex> FilesFinder::GetTotalItems(const std::vector<std::string>** ret) {
  std::unique_lock<std::mutex> lock(mutex_results);
  *ret = &results;
  return std::move(lock);
}


int FilesFinder::GetFilteredItemsCount() {
  std::lock_guard<std::mutex> lock(mutex_filters);
  return filters.size();
}


int FilesFinder::GetTotalItemsCount() {
  std::lock_guard<std::mutex> lock(mutex_results);
  return results.size();
}


void FilesFinder::InputChanged(std::string input_text) {
  if (input_text.empty()) {
    {
      std::lock_guard<std::mutex> lock_r(mutex_results);
      std::lock_guard<std::mutex> lock_f(mutex_filters);
      filters = results; // Copy all the values from results.
    }
  } else {
      TriggerFuzzyFilter(input_text);
    }
  }


// FIXME: These methods should buffer the pending input if it's now ends with
// a new line otherwise we'll get a broken path (see lsp/client.cpp) for reference.
//
// FIXME: Make sure the buff only contains printable ascii/utf8 values otherwise
// it'll messup the ui drawing of those text.
void FilesFinder::StdoutCallbackResults(void* data, const char* buff, size_t length) {
  FilesFinder* self = (FilesFinder*) data;
  {
    std::lock_guard<std::mutex> lock(self->mutex_results);
    const char* end = buff;
    while (length--) {
      if (*end == '\n') {
        size_t len = (end-buff);
        if (len != 0) {
          self->results.push_back(std::string(buff, len));
        }
        buff = end+1;
      }
      end++;
    }
  }
}


void FilesFinder::StdoutCallbackFilter(void* data, const char* buff, size_t length) {

  FilesFinder* self = (FilesFinder*) data;
  {
    std::lock_guard<std::mutex> lock(self->mutex_filters);
    const char* end = buff;
    while (length--) {
      if (*end == '\n') {
        size_t len = (end-buff);
        if (len != 0) {
          self->filters.push_back(std::string(buff, len));
        }
        buff = end+1;
      }
      end++;
    }
  }
  self->ItemsChanged();
}




// FIXME: This method is not final.
void FilesFinder::TriggerFuzzyFilter(const std::string& input_text) {
  { // This scope is absolutely necessary here otherwise the bellow destrucor
    // of IPC will cause a deadlock.
    std::lock_guard<std::mutex> lock_filter(mutex_filters);
    filters.clear();
  }

  IPC::IpcOptions opt; // FIXME: This needs to implemented properly.
  opt.user_data      = this;
  opt.file           = "fzf";
  opt.argv           = { "--filter", input_text };
  opt.timeout_sec    = 2;
  opt.sending_inputs = true;
  opt.stdout_cb      = FilesFinder::StdoutCallbackFilter;

  // This will destroy the old ipc and starts a new one.
  ipc_filter         = IPC::New(opt);

  {
    std::lock_guard<std::mutex> lock_results(mutex_results);
    std::lock_guard<std::mutex> lock_filters(mutex_filters);
    for (auto& s : results) {
      ipc_filter->WriteToStdin(s + "\n");
    }
    ipc_filter->StopListening();
    ipc_filter->Run();
  }

}

