//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"

#include <string>
#include <functional>


typedef std::function<void(void* user_data, const char* buff, size_t length)> FuncStdoutCallback;
typedef std::function<void(void* user_data, int exit_code)> FuncExitCallback;

// Os dependent functions.
uint64_t GetPid();


// A platform independent abstract interface for two way inter process
// communication mainly used for the language server communication.
class IPC {

public:
  struct IpcOptions {
    void* user_data              = nullptr;
    std::string cmd              = "";
    int timeout_sec              = -1;    // -1 means no timeout.
    
    // Set to true to the child process to listen for inputs which we'll be
    // send using WriteToStdin() method. If this is false, WriteToStdin
    // shouldn't write anything to the stdin of the child process.
    bool sending_inputs          = false;

    FuncStdoutCallback stdout_cb = nullptr;
    FuncStdoutCallback stderr_cb = nullptr;
    FuncExitCallback exit_cb     = nullptr;
  };

  // Platform dependent implementation should be defined by the os module for
  // each different platforms.
  static std::unique_ptr<IPC> New(IpcOptions opt);

  IPC(IpcOptions opt) : opt(opt) {}

  // Note that this method isn't supposed to be called multiple times. Create a
  // new IPC instance if you want to listen to another process.
  virtual void StartListening() = 0;
  virtual void StopListening() = 0;

  virtual void WriteToStdin(const std::string& data) = 0;

  virtual ~IPC() = default;

protected:
  IpcOptions opt;
};

