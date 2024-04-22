//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "lsp.hpp"

#include <future>


bool RequestId::operator==(const RequestId& other) {
  return (_int == other._int && _str == other._str);
}


bool RequestId::operator==(int val) {
  return (_str.length() == 0 && _int == val);
}


bool RequestId::operator==(const std::string& val) {
  return (_str.length() > 0 && _str == val);
}


LspClient::LspClient(LspConfig config) : config(config) {

  IPC::IpcOptions opt;
  opt.user_data      = this;
  opt.cmd            = config.server;
  opt.timeout_sec    = -1;
  opt.sending_inputs = true;
  opt.stdout_cb      = LspClient::StdoutCallback;
  opt.stderr_cb      = LspClient::StderrCallback;
  opt.exit_cb        = LspClient::ExitCallback;
  ipc                = IPC::New(opt);
}


LspClient::~LspClient() {
  SendNotification("exit", json::object());
}


void LspClient::StartServer(std::optional<Uri> root_uri) {
  ipc->StartListening();
  // TODO: configure params.
  SendRequest("initialize", json::object());
}


RequestId LspClient::SendRequest(const std::string& method, const json& params) {
  RequestId id = next_id++;
  SendServerContent({
    { "jsonrpc", "2.0" },
    { "id",      (int) id },
    { "method",  method },
    { "params",  params },
  });
  return id;
}


void LspClient::SendNotification(const std::string& method, const json& params) {
  SendServerContent({
    { "jsonrpc",  "2.0" },
    { "method",   method },
    { "params",   params },
  });
}


void LspClient::SendServerContent(const json& content) {

  std::string dump = content.dump();
  std::string data = "";
  std::string content_len = std::to_string(dump.length());

  data = "Content-Length: " + content_len + "\r\n";
  data += "\r\n";
  data += dump;

  bool is_initialize_request = (
    content.contains("id") &&
    content["id"].is_number_integer() &&
    (content["id"].template get<int>() == INITIALIZE_REQUSET_ID));

  if (is_initialize_request) {
    ipc->WriteToStdin(data);

  } else { // Wait till the server initialized.
    (void) std::async(std::launch::async, [&](){
      std::unique_lock<std::mutex> lock(mutex_server_initialized);
      while (!is_server_initialized) {
        cond_server_initialized.wait(lock);
      }
      ipc->WriteToStdin(data);
    });
  }

}


void LspClient::HandleServerContent(const json& content) {

  // TODO: check contains method and params, before getting.
  if (!content.contains("id") && content.contains("method") && content.contains("params")) {
    HandleNotify(content["method"], content["params"]);
    return;
  }

  if (!content.contains("id")) return;

  RequestId id;
  json content_id = content["id"];
  if (content_id.is_number_integer()) {
    id = content_id.template get<int>();
  } else if (content_id.is_string()) {
    id = content_id.template get<std::string>();
  } else {
    return;
  }

  if (content.contains("method") && content.contains("params")) {
    HandleRequest(id, content["method"], content["params"]);
    return;
  }

  if (content.contains("result")) {
    HandleResponse(id, content["result"]);
    return;
  }

  if (content.contains("error")) {
    HandleError(id, content["error"]);
    return;
  }
}


// FIXME: cleanup this mess.
std::vector<Diagnostic> LspClient::diags = {};
void LspClient::HandleNotify(const std::string& method, const json& params) {

  if (method == "textDocument/publishDiagnostics") {
    // TODO: Do a try catch here since the bellow might also throw if the server
    // failed for some reason.
    for (const json& diag : params["diagnostics"]) {
      Diagnostic diagnostic;
      diagnostic.code      = diag["code"].template get<std::string>();
      diagnostic.message   = diag["message"].template get<std::string>();
      diagnostic.severity  = diag["severity"].template get<int>();
      diagnostic.source    = diag["source"].template get<std::string>();
      diagnostic.start.row = diag["range"]["start"]["line"].template get<int>();
      diagnostic.start.col = diag["range"]["start"]["character"].template get<int>();
      diagnostic.end.row   = diag["range"]["end"]["line"].template get<int>();
      diagnostic.end.col   = diag["range"]["end"]["character"].template get<int>();
      diags.push_back(diagnostic);
    }

    Uri uri = params["uri"];
    int version = params["version"];
  }

}


void LspClient::HandleResponse(RequestId id, const json& result) {

  // If the server is initialized, update the atomic bool and tell the server
  // we're initialized as well.
  if (id == INITIALIZE_REQUSET_ID) {
    is_server_initialized = true;
    cond_server_initialized.notify_all();
    SendNotification("initialized", json::object());
  }

}


void LspClient::HandleRequest(RequestId id, const std::string& method, const json& params) {

}


void LspClient::HandleError(RequestId id, const json& error) {

}


void LspClient::StdoutCallback(void* user_data, const char* buff, size_t length) {
  LspClient* self = (LspClient*) user_data;
  std::string_view data(buff, length);

  do {

    size_t len_start = data.find("Content-Length:");
    if (len_start == std::string::npos) return;
    len_start += strlen("Content-Length:");

    size_t len_end = data.find("\r\n", len_start);
    if (len_end == std::string::npos) return;

    std::string len_str = std::string(data.data() + len_start, len_end - len_start);
    int content_len = std::stoi(len_str); // TODO: This may throw (we shouldn't anything).

    size_t content_start = data.find("\r\n\r\n");
    if (content_start == std::string::npos) return;
    content_start += strlen("\r\n\r\n");

    // Something went wrong from the server.
    if (data.length() - content_start < content_len) return;

    std::string_view content_str = data.substr(content_start, content_len);

    // TODO: This may throw, Handle (we shouldn't trust anything).
    json content = json::parse(content_str);

    // FIXME: this is temproary.
    // printf("[lsp-client] %s\n", content.dump(2).c_str());

    self->HandleServerContent(content);

    // Update data for the next iteration.
    data = data.substr(content_start + content_len);
  } while (data.length() > 0);

}


void LspClient::StderrCallback(void* user_data, const char* buff, size_t length) {
  // printf("[lsp-stderr] %*s\n", (int) length, buff);
}


void LspClient::ExitCallback(void* user_data, int exit_code) {
  // FIXME: this is temproary.
  printf("[lsp-client] exit_code=%i\n", exit_code);
}
