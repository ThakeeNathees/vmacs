//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/

#include "core/core.hpp"
#include "os/os.hpp"

#include <optional>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;


// RequestId = int | string
struct RequestId {
  int _int = 0;
  std::string _str;

  RequestId() {}
  RequestId(int val) : _int(val) {}
  RequestId(const std::string& val) : _str(val) {}

  operator int() const { return _int; }
  operator const std::string&() const { return _str; }

  bool operator==(const RequestId& other);
  bool operator==(int val);
  bool operator==(const std::string& val);
};


struct Diagnostic {
  uint32_t version;    // Document version where this diagnostic applied.
  std::string code;    // unique code of the error/warning etc.
  std::string message; // A human readable message for the code.
  Coord start;         // Start position of the diagnostic.
  Coord end;           // Start position of the diagnostic.
  int severity;        // 1 = error, 2 = warning, ...
  std::string source;  // clangd, pyls, etc.
};


/******************************************************************************
 * Language Server Client.
 *****************************************************************************/

// The "hardcoded" id for initialize request.
#define INITIALIZE_REQUSET_ID 0

typedef std::string Uri;
typedef std::function<void(const Uri&, std::vector<Diagnostic>&&)> CallbackDiagnostic;


struct LspConfig {
  std::string server; // Currently only the IPC method is supported.
};


class LspClient {

public:

  LspClient(LspConfig config);
  ~LspClient();

  // This will start the server and send initialize request, async wait for
  // server to respond with initialized. And send client initialized.
  void StartServer(std::optional<Uri> root_uri);

  RequestId SendRequest(const std::string& method, const Json& params);
  void SendNotification(const std::string& method, const Json& params);

  // Abstracted request/notification methods.
  void DidOpen(const Uri& uri, const std::string& text, const std::string& langauge);

  // Callbacks for content recieved from the server. These are global static
  // and should be set by the one that's listening for any server inputs.
  static CallbackDiagnostic cb_diagnostics;

private:
  LspConfig config;

  // The Interprocess communication proxy.
  std::unique_ptr<IPC> ipc;

  // Id of the next request, it'll increment after each request. Note that we
  // set id=0 as the initialize request id.
  std::atomic<int> next_id = INITIALIZE_REQUSET_ID;

  // All the request must sent to the server (except for the initialize request)
  // after we get a notification that server is initialized. we store that here.
  // The bellow condition_variable will wait for the atomic boolean to becomd
  // true before it sends any request to the server.
  std::atomic<bool> is_server_initialized = false;
  mutable std::mutex mutex_server_initialized;
  std::condition_variable cond_server_initialized;

private:
  static void StdoutCallback(void* user_data, const char* buff, size_t length);
  static void StderrCallback(void* user_data, const char* buff, size_t length);
  static void ExitCallback(void* user_data, int exit_code);

  void SendServerContent(const Json& content);
  void HandleServerContent(const Json& content);

  // Handle content from the server.
  void HandleNotify(const std::string& method, const Json& params);
  void HandleResponse(RequestId id, const Json& result);
  void HandleRequest(RequestId id, const std::string& method, const Json& params);
  void HandleError(RequestId id, const Json& error);
};
