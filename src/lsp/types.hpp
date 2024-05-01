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
#include "platform/platform.hpp"


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



