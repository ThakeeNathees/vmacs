// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <assert.h>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "process.hpp"
#include "lsp.hpp"


class LspClient {
public:
  LspClient(std::string_view server_cmd, std::string_view cwd);

  // Initialize server with Initialize() method.
  bool IsServerReady() const { return is_server_initialized; }

  RequestId SendRequest(std::string_view method, const json& params);
  void SendNotification(std::string_view method, const json& params);

  // Requests.
  RequestId Initialize(std::optional<DocumentUri> root_uri);
  RequestId Shutdown();
  RequestId Completion(DocumentUri uri, Position position, std::optional<CompletionContext> context);
  RequestId SignatureHelp(DocumentUri uri, Position position);
  RequestId GotoDefinition(DocumentUri uri, Position position);
  RequestId GotoDeclaration(DocumentUri uri, Position position);
  RequestId Hover(DocumentUri uri, Position position);

  // Notification to send to server.
  void Initialized();
  void Exit();
  void DidOpen(DocumentUri uri, std::string_view text, std::string_view lang_id);
  void DidClose(DocumentUri uri);
  void DidChange(DocumentUri uri, std::vector<TextDocumentContentChangeEvent>&& changes);

private:
  void _WriteToServer(std::string_view content);

  // Handle response from server.
  static void _OnStdoutReady(LspClient* client, std::string_view data);
  void _HandleServerContent(const json& content);

  void OnNotify(const std::string& method, const json& params);
  void OnResponse(RequestId id, const json& result);
  void OnRequest(RequestId id, const std::string_view method, const json& params);
  void OnError(RequestId id, const json& error);

private:

  // Till server not initiallized, we shouldn't send any request.
  // ref: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialize
  bool is_server_initialized = false;

  // Id of the next request it'll increment for each request.
  int next_id = 0;

  std::unique_ptr<Process> cp;
};
