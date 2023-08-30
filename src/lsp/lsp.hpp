// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

/*
 * https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/
 */

#include <stdint.h>
#include <string>
#include <optional>

#include <nlohmann/json.hpp>
using json = nlohmann::json;


// Once this (https://github.com/nlohmann/json/pull/4033) merged, I can get rid of these macros
// and use the "standard" macros of theirs.
#define JSON_DEFINE(Type, ...) NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Type, __VA_ARGS__)

#define JSON_DEFINE_SUB(Type, Parent, ...)                                                  \
  friend void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) {       \
    to_json(nlohmann_json_j, static_cast<const Parent&>(nlohmann_json_t));                  \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__))                \
  }                                                                                         \
  friend void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) {     \
    from_json(nlohmann_json_j, static_cast<Parent&>(nlohmann_json_t));                      \
    const Type nlohmann_json_default_obj{};                                                 \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__)) \
  }

#define JSON_ENUM(Type)                                  \
  enum class Type;                                       \
  static inline void to_json(json& j, const Type& t) {   \
    j = (int) t;                                         \
  }                                                      \
  static inline void from_json(const json& j, Type& t) { \
    t = (Type) j.get<int>();                             \
  }                                                      \
  enum class Type


typedef std::string_view DocumentUri;


// std::optional<T> serialization and de-serialization.
namespace nlohmann {
  template <typename T>
  struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& opt) {
      if (opt.has_value()) {
        j = *opt; // this will call adl_serializer<T>::to_json which will
      } else {
        j = nullptr; // find the free function to_json in T's namespace!
      }
    }

    static void from_json(const json& j, std::optional<T>& opt) {
      if (!j.is_null()) {
        opt = j.template get<T>(); // same as above, but with
      }
    }
  };
}


JSON_ENUM(CompletionItemKind) {
  Text = 1,
  Method = 2,
  Function = 3,
  Constructor = 4,
  Field = 5,
  Variable = 6,
  Class = 7,
  Interface = 8,
  Module = 9,
  Property = 10,
  Unit = 11,
  Value = 12,
  Enum = 13,
  Keyword = 14,
  Snippet = 15,
  Color = 16,
  File = 17,
  Reference = 18,
  Folder = 19,
  EnumMember = 20,
  Constant = 21,
  Struct = 22,
  Event = 23,
  Operator = 24,
  TypeParameter = 25,
};


JSON_ENUM(SymbolKind) {
  File = 1,
  Module = 2,
  Namespace = 3,
  Package = 4,
  Class = 5,
  Method = 6,
  Property = 7,
  Field = 8,
  Constructor = 9,
  Enum = 10,
  Interface = 11,
  Function = 12,
  Variable = 13,
  Constant = 14,
  String = 15,
  Number = 16,
  Boolean = 17,
  Array = 18,
  Object = 19,
  Key = 20,
  Null = 21,
  EnumMember = 22,
  Struct = 23,
  Event = 24,
  Operator = 25,
  TypeParameter = 26,
};


JSON_ENUM(CompletionTriggerKind) {
  Invoked = 1,
  TriggerCharacter = 2,
  TriggerForIncompleteCompletions = 3,
};


struct RequestId {

  int _int = 0;
  std::string _str; // If it's empty string we consider int part.

  RequestId() {}
  RequestId(int val) : _int(val) {}
  RequestId(const std::string& val) : _str(val) {}
  RequestId(const char* val) : _str(val) {}

  bool operator==(const RequestId& other);
  bool operator==(int val);
  bool operator==(const std::string& val);
  bool operator==(const char* val);

  friend void to_json(json& j, const RequestId& t);
  friend void from_json(const json& j, RequestId& t);
};


struct Position {
  uint32_t line;
  uint32_t character;
  JSON_DEFINE(Position, line, character)
};


struct Range {
  Position start;
  Position end;
  JSON_DEFINE(Range, start, end)
};


struct Location {
  DocumentUri uri;
  Range range;
  JSON_DEFINE(Location, uri, range)
};


struct RequestMessage {
  std::string_view jsonrpc;
  RequestId id;
  std::string_view method;
  json params;
  JSON_DEFINE(RequestMessage, jsonrpc, id, method, params)
};


struct ResponseError {
  int code;
  std::string_view message;
  std::optional<json> data;
  JSON_DEFINE(ResponseError, code, message, data)
};


struct ResponseMessage {
  std::string_view jsonrpc;
  RequestId id;
  std::optional<json> result;
  std::optional<ResponseError> error;
  JSON_DEFINE(ResponseMessage, jsonrpc, id, result, error)
};


struct NotificationMessage {
  std::string_view jsonrpc;
  std::string_view method;
  json params;
  JSON_DEFINE(NotificationMessage, jsonrpc, method, params)
};


struct InitializeParams {
  std::optional<uint32_t> processId;
  json capabilities = json::object(); // TODO: Make a struct.
  std::optional<DocumentUri> rootUri;
  JSON_DEFINE(InitializeParams, processId, capabilities, rootUri)
};


struct InitializeResult {
  json capabilities = json::object(); // TODO: Make a struct.
  json serverInfo = json::object();
};


struct TextDocumentItem {
  DocumentUri uri;
  std::string_view languageId;
  int version = 0;
  std::string_view text;
  JSON_DEFINE(TextDocumentItem, uri, languageId, version, text)
};


struct TextDocumentIdentifier {
  DocumentUri uri;
  JSON_DEFINE(TextDocumentIdentifier, uri)
};


struct TextDocumentPositionParams {
  TextDocumentIdentifier textDocument;
  Position position;
  JSON_DEFINE(TextDocumentPositionParams, textDocument, position)
};


struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
  /**
   * The version number of this document.
   *
   * The version number of a document will increase after each change,
   * including undo/redo. The number doesn't need to be consecutive.
   */
  int version;
  JSON_DEFINE_SUB(VersionedTextDocumentIdentifier, TextDocumentIdentifier, version)
};


struct TextDocumentContentChangeEvent {
  Range range;
  std::string_view text; // New text for the provided range.
  JSON_DEFINE(TextDocumentContentChangeEvent, range, text)
};


struct DidOpenTextDocumentParams {
  TextDocumentItem textDocument;
  JSON_DEFINE(DidOpenTextDocumentParams, textDocument)
};


struct DidCloseTextDocumentParams {
  TextDocumentIdentifier textDocument;
  JSON_DEFINE(DidCloseTextDocumentParams, textDocument)
};


struct DidChangeTextDocumentParams {
  /**
   * The document that did change. The version number points
   * to the version after all provided content changes have
   * been applied.
   */
  VersionedTextDocumentIdentifier textDocument;

  /**
   * The actual content changes. The content changes describe single state
   * changes to the document. So if there are two content changes c1 (at
   * array index 0) and c2 (at array index 1) for a document in state S then
   * c1 moves the document from S to S' and c2 from S' to S''. So c1 is
   * computed on the state S and c2 is computed on the state S'.
   *
   * To mirror the content of a document using change events use the following
   * approach:
   * - start with the same initial content
   * - apply the 'textDocument/didChange' notifications in the order you
   *   receive them.
   * - apply the `TextDocumentContentChangeEvent`s in a single notification
   *   in the order you receive them.
   */
  std::vector<TextDocumentContentChangeEvent> contentChanges;

  JSON_DEFINE(DidChangeTextDocumentParams, textDocument, contentChanges)
};


struct CompletionContext {
  // How the completion was triggered.
  CompletionTriggerKind triggerKind;
  std::optional<std::string_view> triggerCharacter;
  JSON_DEFINE(CompletionContext, triggerKind, triggerCharacter)
};


struct CompletionParams : public TextDocumentPositionParams {
  std::optional<CompletionContext> context;
  JSON_DEFINE_SUB(CompletionParams, TextDocumentPositionParams, context)
};
