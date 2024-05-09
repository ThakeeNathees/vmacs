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

  virtual std::unique_lock<std::mutex> GetFilteredItems(const std::vector<std::string>** ret) = 0;
  virtual std::unique_lock<std::mutex> GetTotalItems(const std::vector<std::string>** ret) = 0;

  virtual int GetFilteredItemsCount() = 0;
  virtual int GetTotalItemsCount() = 0;

  virtual void InputChanged(const std::string& input_text) = 0;
  virtual void SelectedItem(const std::string& item) = 0;

  void RegisterItemsChangeListener(
      CallbackFinderItemsChanged cb_filter,
      CallbackFinderItemsChanged cb_total);

protected:
  CallbackFinderItemsChanged cb_item_changed_filter = nullptr;
  CallbackFinderItemsChanged cb_item_changed_total = nullptr;
};



class FilesFinder : public Finder {
public:
  void Initialize() override;

  std::unique_lock<std::mutex> GetFilteredItems(const std::vector<std::string>** ret) override;
  std::unique_lock<std::mutex> GetTotalItems(const std::vector<std::string>** ret) override;
  int GetFilteredItemsCount() override;
  int GetTotalItemsCount() override;

  void InputChanged(const std::string& input_text) override;
  void SelectedItem(const std::string& item) override;

private:
  std::unique_ptr<IPC> ipc_results;
  std::mutex mutex_results;
  std::vector<std::string> results;

  std::unique_ptr<IPC> ipc_filter;
  std::mutex mutex_filters;
  std::vector<std::string> filters;

  // IF any input is not ending with a new line, we'll set to the bellow buffer
  // and join with the start of the next stdout input.
  std::string buff_results;
  std::string buff_filter;

private:

  static void StdoutCallbackResults(void* data, const char* buff, size_t length);
  static void StdoutCallbackFilter(void* data, const char* buff, size_t length);
  void TriggerFuzzyFilter(const std::string& input_text);

};

