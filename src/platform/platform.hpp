//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include <tree_sitter/api.h>

#include <filesystem>
namespace fs = std::filesystem;

// The language parser loading function.
typedef const TSLanguage* (*TreeSitterLoader)(void);

// The will be the result after loading a language resource.
struct LanguageLoadResult {
  const char* language_id;
  TreeSitterLoader tree_sitter_loader;
  const char* query_highlight;
};

// IPC callback functions.
typedef std::function<void(void* user_data, const char* buff, size_t length)> FuncStdoutCallback;
typedef std::function<void(void* user_data, int exit_code)> FuncExitCallback;


class Path {

public:
  Path() = default;
  Path(std::string path);

  std::string String() const;
  std::string Uri() const;

  std::string FileName() const;

  bool Empty() const;
  bool Exists() const;

  Path operator /(const std::string& inner) const;
  bool operator <(const Path& other) const; // Needed to be used in map keys.

  static Path FromUri(std::string uri);
  static fs::path Normalize(const fs::path& path);

private:
  fs::path path;
};


// A platform is an an "abstract" interface to comminicate to the host system.
// It could be a direct os level of if it's running inside another application,
// Tha application will be the platform, Loading files, resources logging etc.
// all are provided by the platform.
class Platform {

public:
  // --------------------------------------------------------------------------
  // Os dependent functions each os should implement this functionalities.
  // --------------------------------------------------------------------------
  static uint64_t GetPid();

  // --------------------------------------------------------------------------
  // Os independent but still we need this from the host system.
  // --------------------------------------------------------------------------

  // Load all the themes in memory or from some config directory and return them
  // as json object, that will be used to construct the Theme instance.
  static std::map<std::string, Json> LoadThemes();

  // Load all the languages in memory or from some config directory.
  static std::vector<LanguageLoadResult> LoadLanguages();

  // TODO: Implement error type for reading since it could be permission error
  // or invalid path or already in used etc.
  // Reads an entire file and return the content. if the path is invalid or an error,
  // this will return false.
  static bool ReadFile(std::string* ret, const Path& path);
  static bool ListDirectory(std::vector<std::string>* items, const Path& path);
};


// A platform independent abstract interface for two way inter process
// communication mainly used for the language server communication.
class IPC {

public:
  struct IpcOptions {
    void* user_data  = nullptr;    // Used in the callbacks.
    std::string file; // The executable (command/file) we'll be calling execvp.
    std::vector<std::string> argv; // Optional additional arguments.
    int timeout_sec  = -1;         // -1 means no timeout.

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
  virtual void Run() = 0;

  // This will signale the child process to stop listening for stdin and complete
  // the process if nothing else to display.
  virtual void StopListening() = 0;

  virtual void WriteToStdin(const std::string& data) = 0;

  virtual ~IPC() = default;

protected:
  IpcOptions opt;
};

