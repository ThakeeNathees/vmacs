//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "lsp.hpp"

#include <future>

// FIXME: Remove this (mess), for debugging.
// #define DUMP_TO_TERM


#define GET_STRING_OR(json, key, or)                 \
  (((json).contains(key) && (json)[key].is_string()) \
    ? (json)[key].template get<std::string>()        \
    : or)

#define GET_BOOL_OR(json, key, or)                    \
  (((json).contains(key) && (json)[key].is_boolean()) \
    ? (json)[key].template get<bool>()                \
    : or)

#define GET_INT_OR(json, key, or)                            \
  (((json).contains(key) && (json)[key].is_number_integer()) \
    ? (json)[key].template get<int>()                        \
    : or)


// Recursively search if a json value exists. Consider the folloging example:
//
//   json["capabilities"]["completionProvider"]["triggerCharacters"]
//
// The call should be made like this:
//
//   const char* path[] = {"capabilities", "completionProvider", "triggerCharacters", NULL };
//   const Json* trig;
//   bool contains = GetJson(&trig, json, path);
//
static bool GetJson(const Json** ret, const Json& json, const char** args) {
  if (*args == NULL) { *ret = &json; return true; }
  if (!json.contains(*args)) { return false;}
  return GetJson(ret, json[*args], args+1);
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

  // TODO: Make a shoutdown lsp method call and call this before unregister and
  // remove lsp clients from the lsp_client registry.
  // SendNotification("exit", Json::object());

  // It's possible the server is not initialized and all the requests are
  // waiting still.
  stop_all_threads = true;
  cond_server_initialized.notify_all();

  // This is necessary, consider without destroing ipc here and the LspClient is
  // "dying" bellow and waiting to join threads but suddenly we got response
  // from the language server for our initialize request and now we have to send
  // "initialized" notification. So the notification will wait on the mutex
  // `mutex_threads_pool` and it'll only unlocks when we're done with this
  // destructor. So by the time "initilized" method aquire the mutex it's already
  // destroied.
  //
  // So we make sure the IPC is destroied properly and we have nothing to do
  // with it or its callback before we cleanup.
  ipc = nullptr;

  {
    std::lock_guard<std::mutex> lock_pool(mutex_threads_pool);
    for (std::thread& thread : threads) {
      if (thread.joinable()) thread.join();
    }
  }
}


void LspClient::StartServer(std::optional<Uri> root_uri) {
  ipc->StartListening();
  // TODO: configure params.
  SendRequest("initialize", Json::object());
}


bool LspClient::IsTriggeringCharacter(char c) const {
  if (BETWEEN('a', c, 'z') || BETWEEN('A', c, 'Z') || c == '_') return true;
  for (char tc : triggering_characters) {
    if (tc == c) return true;
  }
  return false;
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


void LspClient::DidChange(const Uri& uri, uint32_t version, const std::vector<DocumentChange>& changes) {
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
  SendNotification("textDocument/didChange", {
      { "textDocument", {{ "uri", uri }, { "version", version }} },
      { "contentChanges", content_changes },
  });
}


void LspClient::Completion(const Uri& uri, Coord position) {
  RequestId id = SendRequest("textDocument/completion", {
      { "textDocument", {{ "uri", uri }} },
      { "position", {{ "line", position.row }, { "character", position.col }} },
  });
  ResponseContext ctx;
  ctx.type = ResponseType::RESP_COMPLETION;
  ctx.uri = uri;
  {
    std::lock_guard<std::mutex> lock(mutex_pending_requests);
    pending_requests[id] = ctx;
  }
}


void LspClient::HandleServerInitilized(const Json& result) {

  { // Store the triggering characters.
    const Json* trig;
    static const char* trig_chars_path[] = {"capabilities", "completionProvider", "triggerCharacters", NULL};
    if(GetJson( &trig, result, trig_chars_path)) {
      if (trig->is_array()) {
        for (auto& c : *trig) {
          if (!c.is_string()) continue;
          std::string ch = c.template get<std::string>();
          if (ch.empty()) continue;
          triggering_characters.push_back(ch[0]);
        }
      }
    }
  }

}


void LspClient::SendServerContent(const Json& content) {

  // It's possible for the ipc to be nullptr if we're waiting for the threads
  // to join in the destructor and this method is called by some other threads.
  if (ipc == nullptr) return;

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
    if (ipc) ipc->WriteToStdin(data);

  } else {

    // This is not necessary all handled properly (read in the destructor)
    // but just in case.
    if (stop_all_threads) return;

    {
    std::lock_guard<std::mutex> look_pool(mutex_threads_pool);

    // Wait in a thread till the server is initialized.
    threads.emplace_back([this, data](){
      std::unique_lock<std::mutex> lock(mutex_server_initialized);
      while (!is_server_initialized && !stop_all_threads && !global_thread_stop) {
        cond_server_initialized.wait(lock);
      }
      if (stop_all_threads || global_thread_stop) return;
      if (ipc) ipc->WriteToStdin(data);
    });
    }

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

    for (const Json& diag : params["diagnostics"]) {
      Diagnostic diagnostic;

      // Not optional.
      diagnostic.message   = GET_STRING_OR(diag, "message", "");

      // TODO: Assuming "range" will always exists, fix this. Figure out a better
      // way to convert back and forth json to c++ types.
      diagnostic.start.row = diag["range"]["start"]["line"].template get<int>();
      diagnostic.start.col = diag["range"]["start"]["character"].template get<int>();
      diagnostic.end.row   = diag["range"]["end"]["line"].template get<int>();
      diagnostic.end.col   = diag["range"]["end"]["character"].template get<int>();
      
      diagnostic.severity  = GET_INT_OR(diag, "severity", 3);
      diagnostic.code      = GET_STRING_OR(diag, "code", "");
      diagnostic.source    = GET_STRING_OR(diag, "source", "");


      diagnostics.push_back(std::move(diagnostic));
    }

    Uri uri = GET_STRING_OR(params, "uri", "");
    uint32_t version = GET_INT_OR(params, "version", 0);
    cb_diagnostics(uri, version, std::move(diagnostics));
    return;
  }

}


void LspClient::HandleResponse(RequestId id, const Json& result) {

  // If the server is initialized, update the atomic bool and tell the server
  // we're initialized as well.
  if (id == INITIALIZE_REQUSET_ID) {
    is_server_initialized = true;
    cond_server_initialized.notify_all();
    HandleServerInitilized(result);
    SendNotification("initialized", Json::object());
  }

  ResponseContext ctx;
  {
    std::lock_guard<std::mutex> lock(mutex_pending_requests);
    auto it = pending_requests.find(id);
    if (it == pending_requests.end()) return; // TODO: Log ignored response.

    // Copy the values and and remove the pending request from the table.
    ctx = std::move(it->second);
    pending_requests.erase(it);
  }

  switch (ctx.type) {
    case RESP_UNKNOWN: return;
    case RESP_COMPLETION:
     {
       if (cb_completion == nullptr) return;
       if (result.is_null()) return;

       bool is_incomplete = false;
       std::vector<CompletionItem> completion_list;

       // Result could be either CompletionItem[] or CompletionList.
       if (result.is_array()) {
         for (const Json& item : result) {
           CompletionItem ci;
           if (!JsonToCompletionItem(&ci, item)) continue;
           completion_list.push_back(std::move(ci));
         }
       } else if (result.is_object()) {
         is_incomplete = GET_BOOL_OR(result, "isIncomplete", false);

         if (!result.contains("items")) return; // Error ??
         if (!result["items"].is_array()) return;
         for (const Json& item : result["items"]) {
           CompletionItem ci;
           if (!JsonToCompletionItem(&ci, item)) continue;
           completion_list.push_back(std::move(ci));
         }

       } else return; // Unknown response ??

       cb_completion(ctx.uri, is_incomplete, std::move(completion_list));
       return; // we're done here.

     } break; // case RESP_COMPLETION

  }

}


void LspClient::HandleRequest(RequestId id, const std::string& method, const Json& params) {

}


void LspClient::HandleError(RequestId id, const Json& error) {

}


void LspClient::ParseAndHandleResponse(LspClient* self, std::string_view json_string) {
  try {
    Json content = Json::parse(json_string);
#ifdef DUMP_TO_TERM
    printf("%s\n", content.dump(2).c_str());
#else
    // FIXME: cleanup this mess.
    // printf(" [lsp-client] %s\n", content.dump(2).c_str());
    FILE* f = fopen("./build/lsp.log", "a");
    fprintf(f, "%s\n", content.dump(2).c_str());
    fclose(f);
#endif

    self->HandleServerContent(content);

  } catch (std::exception) {}
}


bool LspClient::JsonToCompletionItem(CompletionItem* item, const Json& json) {
  ASSERT(item != nullptr, OOPS);

  if (!json.contains("label")) return false;
  item->label = json["label"].template get<std::string>();
  item->insert_text = GET_STRING_OR(json, "insertText", "");

  item->kind = (CompletionItemKind) GET_INT_OR(json, "kind", 1);
  item->documentation = GET_STRING_OR(json, "documentation", "");
  item->depricated = GET_BOOL_OR(json, "depricated", false);
  item->documentation = GET_STRING_OR(json, "sortText", "");

  auto _ParseTextEdit = [](const Json& json) -> TextEdit {
    TextEdit text_edit;

    Coord start, end;
    if (!json.contains("range")) return text_edit;
    const Json& range = json["range"];

    // TODO: Handle all the possible errors it might throw (create a common function as it's reusable).
    start.row = range["range"]["start"]["line"].template get<int>();
    start.col = range["range"]["start"]["character"].template get<int>();
    end.row   = range["range"]["end"]["line"].template get<int>();
    end.col   = range["range"]["end"]["character"].template get<int>();

    text_edit.start = start;
    text_edit.end   = end;
    text_edit.text = GET_STRING_OR(json, "newText", "");

    return text_edit;
  };

  return true;
}


void LspClient::StdoutCallback(void* user_data, const char* buff, size_t length) {
  LspClient* self = (LspClient*) user_data;
  std::string_view data(buff, length);

  do {
    if (self->pending_bytes > 0) {
      if (data.length() < self->pending_bytes) { // Still not enough.
        self->stdout_buffer += data;
        self->pending_bytes -= data.length();
        return; // Nothing else to do.
      }

      self->stdout_buffer += data.substr(0, self->pending_bytes);
      ParseAndHandleResponse(self, self->stdout_buffer);

      self->pending_bytes = 0;
      self->stdout_buffer.clear();
      data = data.substr(self->pending_bytes);
    }

    size_t len_start = data.find("Content-Length:");
    if (len_start == std::string::npos) return;
    len_start += strlen("Content-Length:");

    size_t len_end = data.find("\r\n", len_start);
    if (len_end == std::string::npos) return;

    int content_len = 0;
    std::string len_str = std::string(data.data() + len_start, len_end - len_start);
    try {
      content_len = std::stoi(len_str);
    } catch (std::exception) { return; }

    size_t content_start = data.find("\r\n\r\n");
    if (content_start == std::string::npos) return; // Invalid response ??
    content_start += strlen("\r\n\r\n");

    // The output is not enough to parse, push it to our stdout_buffer.
    size_t available_content = data.length() - content_start;
    if (available_content < content_len) {
      self->stdout_buffer = data.substr(content_start);
      self->pending_bytes = content_len - available_content;
      return;
    }

    std::string_view content_str = data.substr(content_start, content_len);

    ParseAndHandleResponse(self, content_str);

    // Update data for the next iteration.
    data = data.substr(content_start + content_len);
  } while (data.length() > 0);

}


// FIXME: Clean up this mess.
void LspClient::StderrCallback(void* user_data, const char* buff, size_t length) {
  // printf(" [lsp-stderr] %*s\n", (int) length, buff);
#ifdef DUMP_TO_TERM
  printf("%*s", (int) length, buff);
#else
  // FIXME: cleanup this mess.
  FILE* f = fopen("./build/lsp.log", "a");
  fprintf(f, "%*s", (int) length, buff);
  fclose(f);
#endif
}


void LspClient::ExitCallback(void* user_data, int exit_code) {
  // FIXME: this is temproary.
  printf(" [lsp-client] exit_code=%i\n", exit_code);
}
