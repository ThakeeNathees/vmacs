//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/

// This "module" doesn't depend on anything else other than core and os.
#include "core/core.hpp"
#include "os/os.hpp"


#include <nlohmann/json.hpp>
using Json = nlohmann::json;

// The "hardcoded" id for initialize request.
#define INITIALIZE_REQUSET_ID 0

// Unique Id for each request we make to the language server.
typedef uint32_t RequestId;


// TODO: Make the bellow structs json parsable (implement std::optional to be
// work with nlohmann json)and cleanup all the manual parsing.

// Defines the document change for the "textDocument/didChange" request.
struct DocumentChange {
  Coord start;
  Coord end;
  std::string text;
};


// Notification from the server after document open and change to push diagnostics.
struct Diagnostic {
  std::string code;    // unique code of the error/warning etc.
  std::string message; // A human readable message for the code.
  Coord start;         // Start position of the diagnostic.
  Coord end;           // Start position of the diagnostic.
  int severity;        // 1 = error, 2 = warning, ...
  std::string source;  // clangd, pyls, etc.
};


enum class CompletionItemKind {
  // Name            Nerd Font Icon
  Text          = 1,  //    0xea93
  Method        = 2,  //    0xea8c
  Function      = 3,  // 󰊕   0xf0295
  Constructor   = 4,  //    0xea8c
  Field         = 5,  //    0xeb5f
  Variable      = 6,  //    0xea88
  Class         = 7,  //    0xeb5b
  Interface     = 8,  //    0xeb61
  Module        = 9,  //    0xea8b
  Property      = 10, //    0xeb65
  Unit          = 11, //    0xea96
  Value         = 12, //    0xea95
  Enum          = 13, //    0xea95
  Keyword       = 14, //    0xeb62
  Snippet       = 15, //    0xeb66
  Color         = 16, //    0xeb5c
  File          = 17, //    0xea7b
  Reference     = 18, //    0xea94
  Folder        = 19, //    0xea83
  EnumMember    = 20, //    0xea95
  Constant      = 21, //    0xeb5d
  Struct        = 22, //    0xea91
  Event         = 23, //    0xea86
  Operator      = 24, //    0xeb64
  TypeParameter = 25, //    0xea92
};


// The range and text needs to be inserted for the completion. Select the region
// and insert the new text.
struct TextEdit {
  Coord start = {0, 0};
  Coord end   = {0, 0};
  std::string text;
};


// Response type of "textDocument/completion"
// https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#completionList
struct CompletionItem {

  // What should be displaied in the completion list and the default text to
  // insert if textEdit now availabe. TODO: labelDetails.
  std::string label;

  // The actual characters that needs to be inserted. If not present, use label.
  // Note that I'm choosing the text need to be inserted in the following order:
  // text_edit -> insert_text -> label.
  std::string insert_text;

  // This is optional, check if all the values are empty or not before using.
  TextEdit text_edit;

  // [Optional] Additional edits needs to be inserted along with the current
  // completion item. (Ex: an import statement at the top).
  std::vector<TextEdit> additional_text_edit;

  // CompletionItemKind enum value.
  CompletionItemKind kind = CompletionItemKind::Text;

  // TODO: this could be plain text or markup (read the docs).
  std::string documentation;

  // Weather it's depricated or not.
  bool depricated = false;

  // Optional string that should be used when sorting.
  std::string sort_text;
};


struct ParameterInformation {
  // Range is start_index, end_index (inclusive). This will be a substring of
  // the label of signature. If not present the values < 0.
  Slice label   = {-1, -1};
  std::string documentation;  // String or Markup, (we ignore markup for now).
};


// This contains a list of the parameter and the active parameter (to highlight).
struct SignatureInformation {
  std::string label;
  std::string documentation; // Stirng or Markup, (we ignore markup for now).
  std::vector<ParameterInformation> parameters;
};


struct SignatureItems {
  // Imange a function with operator overloaded, it'll have multiple signatures
  // for each overloaded function, here we have the entire list for all function.
  std::vector<SignatureInformation> signatures;
  int active_signature = -1; // If not in range ommit.
  int active_parameter = -1; // If not in range ommit.
};


/******************************************************************************
 * Language Server Client.
 *****************************************************************************/


typedef std::string Uri;
typedef std::function<void(const Uri&, uint32_t version, std::vector<Diagnostic>&&)> CallbackDiagnostic;
typedef std::function<void(const Uri&, bool is_incomplete, std::vector<CompletionItem>&&)> CallbackCompletion;
typedef std::function<void(const Uri&, SignatureItems&&)> CallbackSignatureHelp;


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

  bool IsTriggeringCharacterCompletion(char c) const;
  bool IsTriggeringCharacterSignature(char c) const;

  RequestId SendRequest(const std::string& method, const Json& params);
  void SendNotification(const std::string& method, const Json& params);

  // Abstracted request/notification methods.
  void DidOpen(const Uri& uri, const std::string& text, const std::string& langauge);
  void DidChange(const Uri& uri, uint32_t version, const std::vector<DocumentChange>& changes);
  void Completion(const Uri& uri, Coord position);
  void SignatureHelp(const Uri& uri, Coord position);
  // TODO: did close.

  // Callbacks for content recieved from the server. (FIXME: this is a public variable
  // Maybe this is okey, not everything needs to have getters and setters).
  CallbackDiagnostic cb_diagnostics = nullptr;
  CallbackCompletion cb_completion  = nullptr;
  CallbackSignatureHelp cb_signature_help  = nullptr;

private:
  LspConfig config;

  // The Interprocess communication proxy.
  std::unique_ptr<IPC> ipc;

  // The langauge server won't always send us all the bytes specified in the
  // content-length header. only a partial if it's too long and we buffer the
  // input in this string till we got all.
  //
  // Note that we don't need to lock this since it's now shared with any other
  // threads. Only the std IO thread is using this in a callback funciton.
  int pending_bytes = 0;
  std::string stdout_buffer;

  // Id of the next request, it'll increment after each request. Note that we
  // set id=0 as the initialize request id.
  std::atomic<uint32_t> next_request_id = INITIALIZE_REQUSET_ID;

  // All the request must sent to the server (except for the initialize request)
  // after we get a notification that server is initialized. we store that here.
  // The bellow condition_variable will wait for the atomic boolean to becomd
  // true before it sends any request to the server.
  std::atomic<bool> is_server_initialized = false;
  std::mutex mutex_server_initialized;
  std::condition_variable cond_server_initialized;

  // We set this to true when this object is destroied and that'll signal all
  // the child threads to stop immediately.
  std::atomic<bool> stop_all_threads = false;

  // All the requests will wait for the server in this "thread pool" and will be
  // joined once the client is destroied.
  std::vector<std::thread> threads;
  std::mutex mutex_threads_pool;

  std::vector<char> triggering_characters_completion; // Autocompletion.
  std::vector<char> triggering_characters_signature;  // For parameters.

  enum ResponseType {
    RESP_UNKNOWN    = 0,
    RESP_COMPLETION = 1,
    RESP_SIGNATURE_HELP = 2,
  };

  struct ResponseContext {
    ResponseType type;
    Uri uri; // Which uri that was requested this response.
  };

  // A table of all the pending requests that needs to be resolved when the
  // response is arrived.
  std::map<RequestId, ResponseContext> pending_requests;
  std::mutex mutex_pending_requests;
  
private:
  static void StdoutCallback(void* user_data, const char* buff, size_t length);
  static void StderrCallback(void* user_data, const char* buff, size_t length);
  static void ExitCallback(void* user_data, int exit_code);

  // This is called when the pending stdio buffer is filled and ready to be processed.
  static void ParseAndHandleResponse(LspClient* self, std::string_view json_string);

  // Parse and put the result on the item pointer return true on success.
  static bool JsonToCompletionItem(CompletionItem* item, const Json& json);

  void SendServerContent(const Json& content);
  void HandleServerContent(const Json& content);

  // Handle content from the server.
  void HandleNotify(const std::string& method, const Json& params);
  void HandleResponse(RequestId id, const Json& result);
  void HandleRequest(RequestId id, const std::string& method, const Json& params);
  void HandleError(RequestId id, const Json& error);

  // Register server capabilities and configs.
  void HandleServerInitilized(const Json& result);
};
