// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"

#include <string>
#include <memory>
#include <functional>

// Callback function to read stdout/stderr.
typedef std::function<void(const std::string&)> CallbackOutFn;


class Process {

public:
  // Construct and return a platform specific process type.
  static std::unique_ptr<Process> New();

  // Configure the process before calling Run();
  virtual void Config(
    std::string_view cmd,
    std::string_view cwd,
    const CallbackOutFn& cb_stdout = nullptr,
    const CallbackOutFn& cb_stderr = nullptr,
    int thread_sleep_timer = 0
  ) = 0;

  virtual bool Run() = 0;
  virtual bool IsRunning() const = 0;

  virtual bool ReadStdout(std::string& ret) = 0;
  virtual bool ReadStderr(std::string& ret) = 0;

  virtual bool WriteStdin(const std::string& message) = 0;
};
