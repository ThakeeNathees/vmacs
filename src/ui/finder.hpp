//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "platform/platform.hpp"

typedef std::function<void()> CallbackFinderItemsChanged;


// An abstract type which will be used as the "backend" for the FindWindow to provide
// the filter items, preview and a selection action.
class Finder {
public:
  virtual ~Finder() = default;

  virtual void Initialize() = 0;

  // Returns the locked mutex of the list and set the pointer, the caller of this
  // method is responsible to release the lock.
  std::unique_lock<std::mutex> GetItems(const std::vector<std::string>** ret);

  int GetItemsCount();
  int GetItemsCountTotal(); // Returns the total "cached" results to display the filtered ratio.

  // The search text is stored in the finder itself. If the user modified the
  // text they should call InputChanged() method declared bellow to trigger the
  // search functions.
  std::string& GetSearchText();

  virtual void InputChanged() = 0;
  virtual bool SelectItem(const std::string& item) = 0;

  void RegisterItemsChangeListener(CallbackFinderItemsChanged cb_filter);

  // This will load the stdout of the child process into a vector of lines, if the
  // output is terminated at the middle of the line, this function will set the
  // string pending and which needs to be passed next time to be used again.
  static void StdoutCallbackLoadResults(
    void* data, const char* buff, size_t length,
    std::string& pending,
    std::mutex& mutex, std::vector<std::string>& lines,
    CallbackFinderItemsChanged cb);

protected:
  CallbackFinderItemsChanged cb_items_changed = nullptr;

  // Since this value is only read and write by the main thread and for child
  // process we'll pass the string as value before creating the process/thread
  // so we don't need a mutex for this string.
  std::string search_text;

  std::unique_ptr<IPC> ipc_total;
  std::mutex mutex_total;
  std::vector<std::string> total;

  std::unique_ptr<IPC> ipc_filter;
  std::mutex mutex_filters;
  std::vector<std::string> filters;

  // IF any input is not ending with a new line, we'll set to the bellow buffer
  // and join with the start of the next stdout input.
  std::string pending_results;
  std::string pending_filter;

};


// -----------------------------------------------------------------------------
// Files finder
// -----------------------------------------------------------------------------


class FilesFinder : public Finder {
public:
  void Initialize() override;
  void InputChanged() override;
  bool SelectItem(const std::string& item) override;

private:
  void TriggerFuzzyFilter();

};


// -----------------------------------------------------------------------------
// Live grep.
// -----------------------------------------------------------------------------


class LiveGrep : public Finder {
public:
  void Initialize() override;
  void InputChanged() override;
  bool SelectItem(const std::string& item) override;

private:
  void TriggerGrep();
};
