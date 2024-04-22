//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "lsp.hpp"

#include <future>


CallbackDiagnostic LspClient::cb_diagnostics = nullptr;


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
  // TODO: send exit notification.
  // SendNotification("exit", Json::object());

  // It's possible the server is not initialized and all the requests are
  // waiting still.
  stop_all_threads = true;
  cond_server_initialized.notify_all();

  for (std::thread& thread : threads) {
    if (thread.joinable()) thread.join();
  }
}


void LspClient::StartServer(std::optional<Uri> root_uri) {
  ipc->StartListening();
  // TODO: configure params.
  SendRequest("initialize", Json::object());
}


RequestId LspClient::SendRequest(const std::string& method, const Json& params) {
  RequestId id = next_request_id++;
  SendServerContent({
    { "jsonrpc", "2.0" },
    { "id",      id },
    { "method",  method },
    { "params",  params },
  });
  return id;
}


void LspClient::SendNotification(const std::string& method, const Json& params) {
  SendServerContent({
    { "jsonrpc",  "2.0" },
    { "method",   method },
    { "params",   params },
  });
}


void LspClient::DidOpen(const Uri& uri, const std::string& text, const std::string& language) {
  SendNotification(
    "textDocument/didOpen", {
      {
        "textDocument", {
          { "uri", uri },
          { "text", text },
          { "languageId", language },
        }
      }
    });
}


void LspClient::DidChange(const Uri& uri, uint32_t version, std::vector<DocumentChange> changes) {
  std::vector<Json> content_changes;
  for (const DocumentChange& change : changes) {
    content_changes.push_back({
      {
        "range", {
          { "start", {{ "line", change.start.row }, { "character", change.start.col }}  },
          { "end", {{ "line", change.end.row }, { "character", change.end.col }} },
        },
      },
      { "text", change.text }
    });
  }
  SendNotification("textDocument/didChange", content_changes);
}


void LspClient::SendServerContent(const Json& content) {

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

  } else {
    // Wait in a thread till the server is initialized.
    threads.emplace_back([this, data](){
      std::unique_lock<std::mutex> lock(mutex_server_initialized);
      while (!is_server_initialized && !stop_all_threads && !global_thread_stop) {
        cond_server_initialized.wait(lock);
      }
      if (stop_all_threads || global_thread_stop) return;
      ipc->WriteToStdin(data);
    });

  }
}


void LspClient::HandleServerContent(const Json& content) {

  // TODO: check contains method and params, before getting.
  if (!content.contains("id") && content.contains("method") && content.contains("params")) {
    HandleNotify(content["method"], content["params"]);
    return;
  }

  if (!content.contains("id")) return;
  if (!content["id"].is_number_integer()) return;

  RequestId id = content["id"].template get<int>();

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


void LspClient::HandleNotify(const std::string& method, const Json& params) {

  if (method == "textDocument/publishDiagnostics") {

    if (cb_diagnostics == nullptr) return;
    std::vector<Diagnostic> diagnostics;

    // TODO: Do a try catch here since the bellow might also throw if the server
    // failed for some reason.
    for (const Json& diag : params["diagnostics"]) {
      Diagnostic diagnostic;
      diagnostic.version   = params["version"];
      diagnostic.code      = diag["code"].template get<std::string>();
      diagnostic.message   = diag["message"].template get<std::string>();
      diagnostic.severity  = diag["severity"].template get<int>();
      diagnostic.source    = diag["source"].template get<std::string>();
      diagnostic.start.row = diag["range"]["start"]["line"].template get<int>();
      diagnostic.start.col = diag["range"]["start"]["character"].template get<int>();
      diagnostic.end.row   = diag["range"]["end"]["line"].template get<int>();
      diagnostic.end.col   = diag["range"]["end"]["character"].template get<int>();

      diagnostics.push_back(std::move(diagnostic));
    }

    Uri uri = params["uri"];
    cb_diagnostics(uri, std::move(diagnostics));
    return;
  }

}


void LspClient::HandleResponse(RequestId id, const Json& result) {

  // If the server is initialized, update the atomic bool and tell the server
  // we're initialized as well.
  if (id == INITIALIZE_REQUSET_ID) {
    is_server_initialized = true;
    cond_server_initialized.notify_all();
    SendNotification("initialized", Json::object());
  }

}


void LspClient::HandleRequest(RequestId id, const std::string& method, const Json& params) {

}


void LspClient::HandleError(RequestId id, const Json& error) {

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
    Json content = Json::parse(content_str);

    // FIXME: this is temproary.
    // printf("[lsp-client] %s\n", content.dump(2).c_str());

    self->HandleServerContent(content);

    // Update data for the next iteration.
    data = data.substr(content_start + content_len);
  } while (data.length() > 0);

}


// FIXME: Clean up this mess.
void LspClient::StderrCallback(void* user_data, const char* buff, size_t length) {
  // printf("[lsp-stderr] %*s\n", (int) length, buff);
  static bool first = true;
  FILE* f = fopen("./build/lsp.log", (first) ?  "w" : "a");
  first = false;
  fprintf(f, "%*s\n", (int) length, buff);
  fclose(f);
}


void LspClient::ExitCallback(void* user_data, int exit_code) {
  // FIXME: this is temproary.
  printf("[lsp-client] exit_code=%i\n", exit_code);
}
