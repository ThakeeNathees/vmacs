// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "client.hpp"


LspClient::LspClient(std::string_view server_cmd, std::string_view cwd) {

  cp = Process::New();

  cp->Config(server_cmd, cwd,
    [=](const std::string& data) {
      LspClient::_OnStdoutReady(this, data);
    },
    [](const std::string& data) {
      printf("[stderr] %s\n", data.c_str());
    });

  bool ok = cp->Run();
  assert(ok && "child process creation failed!");
}


void LspClient::_OnStdoutReady(LspClient* client, std::string_view data) {

  do {
    size_t len_start = data.find("Content-Length:");
    if (len_start == std::string::npos) return;
    len_start += strlen("Content-Length:");

    size_t len_end = data.find("\r\n", len_start);
    if (len_end == std::string::npos) return;

    std::string str_len = std::string(data.data() + len_start, len_end - len_start);
    int content_len = std::stoi(str_len);

    size_t content_start = data.find("\r\n\r\n");
    if (content_start == std::string::npos) return;
    content_start += strlen("\r\n\r\n");

    // Something went wrong from server.
    if (data.length() - content_start < content_len) return;

    std::string_view content_str = data.substr(content_start, content_len);

    json content = json::parse(content_str);
    data = data.substr(content_start + content_len);

    printf("\n%s\n", content.dump(2).c_str());
    client->_HandleServerContent(content);

  } while (data.length() > 0);
}


void LspClient::_HandleServerContent(const json& content) {
  if (!content.contains("id") && content.contains("params")) {
    OnNotify(content["method"], content["params"]);
    return;
  }

  RequestId id = content["id"];

  if (content.contains("method")) {
    std::string_view method = content["method"].template get<std::string_view>();
    OnRequest(id, method, content["params"]);
    return;
  }

  if (content.contains("result")) {
    OnResponse(id, content["result"]);
    return;
  }

  if (content.contains("error")) {
    OnError(id, content["error"]);
    return;
  }
}


void LspClient::_WriteToServer(std::string_view content) {
  assert(cp->IsRunning() && "Child process is not running!");

  std::string data;
  data += "Content-Length: " + std::to_string(content.length()) + "\r\n";
  data += "\r\n";
  data += content;

  cp->WriteStdin(data);
}


RequestId LspClient::SendRequest(std::string_view method, const json& params) {
  // Except for "initialize" method all other request should be send after the
  // server is initialized.
  RequestMessage message;
  message.jsonrpc = "2.0";
  message.id      = next_id++;
  message.method  = method;
  message.params  = params;
  _WriteToServer(json(message).dump());
  return message.id;
}


void LspClient::SendNotification(std::string_view method, const json& params) {
  NotificationMessage message;
  message.jsonrpc = "2.0";
  message.method = method;
  message.params = params;
  _WriteToServer(json(message).dump());
}


RequestId LspClient::Initialize(std::optional<DocumentUri> root_uri) {
  InitializeParams params;
  params.rootUri = root_uri;
  return SendRequest("initialize", params);
}


RequestId LspClient::Shutdown() {
  return SendRequest("shutdown", json());
}


RequestId LspClient::Completion(DocumentUri uri, Position position, std::optional<CompletionContext> context) {
  CompletionParams params;
  params.textDocument.uri = uri;
  params.position = position;
  params.context = context;
  return SendRequest("textDocument/completion", params);
}


RequestId LspClient::SignatureHelp(DocumentUri uri, Position position) {
  TextDocumentPositionParams params;
  params.textDocument.uri = uri;
  params.position = position;
  return SendRequest("textDocument/signatureHelp", params);
}


RequestId LspClient::GotoDefinition(DocumentUri uri, Position position) {
  TextDocumentPositionParams params;
  params.textDocument.uri = uri;
  params.position = position;
  return SendRequest("textDocument/definition", params);
}


RequestId LspClient::GotoDeclaration(DocumentUri uri, Position position) {
  TextDocumentPositionParams params;
  params.textDocument.uri = uri;
  params.position = position;
  return SendRequest("textDocument/declaration", params);
}


RequestId LspClient::Hover(DocumentUri uri, Position position) {
  TextDocumentPositionParams params;
  params.textDocument.uri = uri;
  params.position = position;
  return SendRequest("textDocument/hover", params);
}


void LspClient::Initialized() {
  SendNotification("initialized", json::object());
}


void LspClient::Exit() {
  SendNotification("exit", json());
}


void LspClient::DidOpen(DocumentUri uri, std::string_view text, std::string_view lang_id) {
  DidOpenTextDocumentParams params;
  params.textDocument.uri = uri;
  params.textDocument.text = text;
  params.textDocument.languageId = lang_id;
  SendNotification("textDocument/didOpen", params);
}


void LspClient::DidClose(DocumentUri uri) {
  DidCloseTextDocumentParams params;
  params.textDocument.uri = uri;
  SendNotification("textDocument/didClose", params);
}


void LspClient::DidChange(DocumentUri uri, std::vector<TextDocumentContentChangeEvent>&& changes) {
  DidChangeTextDocumentParams params;
  params.textDocument.uri = uri;
  params.contentChanges = std::move(changes);
  SendNotification("textDocument/didChange", params);
}


void LspClient::OnNotify(const std::string& method, const json& params) {

}


void LspClient::OnResponse(RequestId id, const json& result) {
  if (id == 0) {
    is_server_initialized = true;
    Initialized();
  }
}


void LspClient::OnRequest(RequestId id, std::string_view method, const json& params) {

}


void LspClient::OnError(RequestId id, const json& error) {

}
