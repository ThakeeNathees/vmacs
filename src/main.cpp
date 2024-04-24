//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT


#include <vmacs.hpp>


// FIXME: remove this.
#include <iostream>
#include <fstream>
#include <sstream>
#include <assert.h>
#include <thread>

// FIXME: For util function of vmacs class, remove it.
#include "core/core.hpp"
#include "os/os.hpp"

#include "lsp/lsp.hpp"

#include "frontend/raylib/fe_raylib.hpp"
#include "frontend/termbox2/fe_termbox2.hpp"

#include "editor/editor.hpp"
#include <tree_sitter/api.h>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;


extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_javascript(void);

void tree_sitter_test() {

  const TSLanguage* lang = tree_sitter_c();
  const char* query_source = R"!(
  [
    "case"
    "}"
    "]"
    ")"
  ] @outdent
  )!";
  uint32_t error_offset;
  TSQueryError err;

  const TSQuery* query = ts_query_new(lang,
    query_source,
    strlen(query_source),
    &error_offset,
    &err
  );

  TSParser* parser = ts_parser_new();
  ts_parser_set_language(parser, lang);

  const char* source_code = R"!(
  {}
  )!";
  TSTree* tree = ts_parser_parse_string(
      parser,
      NULL,
      source_code,
      strlen(source_code)
      );
  // Print.
  TSNode root_node = ts_tree_root_node(tree);
  char* dump = ts_node_string(root_node);
  printf("Syntax tree: %s\n", dump);


  TSQueryCursor* cursor = ts_query_cursor_new();
  ts_query_cursor_exec(cursor, query, root_node);

  TSQueryMatch match;
  if (false) {
    uint32_t capture_index;
    while (ts_query_cursor_next_capture(cursor, &match, &capture_index)) {
      printf("capture_count=%i\n", match.capture_count);
      printf("pattern_index=%i\n", match.pattern_index);
      for (int i = 0; i < match.capture_count; i++) {
        uint32_t len;
        const char* cap_name = ts_query_capture_name_for_id(query, match.captures[i].index, &len);
        printf("capture_name=%*s\n", len, cap_name);
      }
      printf("\n\n");
    }
  } else {

    while (ts_query_cursor_next_match(cursor, &match)) {
      printf("capture_count=%i\n", match.capture_count);
      printf("pattern_index=%i\n", match.pattern_index);
      for (int i = 0; i < match.capture_count; i++) {
        uint32_t len;
        const char* cap_name = ts_query_capture_name_for_id(query, match.captures[i].index, &len);
        printf("capture_name=%*s\n", len, cap_name);
        char* s = ts_node_string(root_node);
        printf("Syntax tree: %s\n\n", s);
        printf("start=%i, end=%i\n\n",
          ts_node_start_byte(match.captures[i].node),
          ts_node_end_byte(match.captures[i].node)
        );
        free(s);
      }
      printf("\n");

    }

  }

  // TSNode expr_stmnt_node = ts_node_named_child(root_node, 0);
  // TSNode array_node = ts_node_named_child(expr_stmnt_node, 0);
  // printf("%s\n", ts_node_type(array_node));

  // for (int i = 0; i < ts_node_child_count(array_node); i++) {
  //   TSNode n = ts_node_child(array_node, i);
  //   printf("%s\n", ts_node_type(n));
  // }

  // printf("cc = %i\n", );
  // printf("nc = %i\n", ts_node_named_child_count(array_node));

  free(dump);
  ts_tree_delete(tree);
  ts_parser_delete(parser);
  return;
}


// TODO:
//
// Now:
//   tree-sitter.
//
//
// Top priority:
//
//
// Bugs:
//  [lsp] if the server send partial result, we ignore -> wait for more...
//  startup time is a bit high, maybe because of raylib ??? try disabling.
//
//  Cleanup things:
//    make docpane takes document as a parameter when constucted.
//    Registry of language server, and language => lsp mapping in the Editor:
//      { "clangd" : LspClient(), "pyright" : LspClient(), ...  }
//      { "c" : "clangd", "c++": "clangd",  "python" : "pyright", etc. }
//    Theme manager...
//    Global Configs.
//
// Pending:
//
// Alt+shift+arrow not detected by termbox: Add this entry:
// {"\x1b[1;10B",   TB_KEY_ARROW_DOWN,  TB_MOD_ALT | TB_MOD_SHIFT },
//
//   helix them system + tree sitter.
//
//   editor send events and ask draw in a structured manner.
//   proper draw system. (request client for buffer and fill only if it needs to re-draw).
//   line numbers.
//   global configs (tabwidth, ).
//   debug print that accumilate logs and dumps at the end.
//   pair and auto indent
//   editor modes implement properly.
//   default bindnigs and tmeme settings.
//   treesitter and syntax highlighting.
//   status bar; prompt bar
//   global atomic bool to say all the while (true) loop thread to end.
//
//   Can't bind to 12j, d10|, ... with numbers.
//
// Code quality:
//   fprintf in the lsp client.
//   write tests (if I have time)
//   ipc timeout value hardcoded fix.
//   merge core + buffer (maybe)
//   Refactor fronend (better file names).
//   Maybe rename.
//   change all comments to // insead of /**/
//   change color values to 256, (terminal support first).
//   restructure the source files.
//   use String (typedef) for all std::string.
//
// Future:
//   picker (fzf, rg, etc...)
//   lsp
//   utf8 support and nerd font icons.













std::string ReadAll2(const std::string& path) {
  std::ifstream inputFile(path.data());
  assert(inputFile.is_open() && "Failed to open file.");
  std::stringstream buffer;
  buffer << inputFile.rdbuf();
  inputFile.close();
  return buffer.str();
}

void lsp_test() {

  LspConfig config;
  config.server = "clangd";

  LspClient client(config);
  client.StartServer(std::nullopt);

  std::string path = "/Users/thakeenathees/Desktop/thakee/temp/lsp/main.c";
  Uri uri = std::string("file://") + path;
  std::string x;

  client.DidOpen(uri, ReadAll2(path), "c");

  // goto definition.
  // std::cin >> x;
  client.SendRequest(
    "textDocument/definition", {
    {
      "textDocument", {
        { "uri",  std::string("file://") + path },
      },
    },
    {
      "position", {
        { "line", 5  },
        { "character", 2 },
      },
    }
  });

  std::vector<DocumentChange> chs;
  DocumentChange ch;
  ch.start = {1, 0};
  ch.end = {1, 0};
  ch.text = "void";
  chs.push_back(ch);
  client.DidChange(uri, 1, chs);

  // Prevent the client from destroy before we got response from server.
  std::string s; std::cin >> s;

  // Otherwise it'll wait forever.
  global_thread_stop = true;
}



int main(int argc, char** argv) {


  // FIXME: move this to os dependent code.
  //
  // writing to stdin after the lsp server is finished will cause sigpipe and
  // terminate the parent process, Not sure this is the proper way but for now
  // this will do.
  // 
  // Test it with send the language server "exit" notification twise.
  //
  signal(SIGPIPE, SIG_IGN);


  // tree_sitter_test();
  // return 0;

  // lsp_test();
  // return 0;

  std::unique_ptr<FrontEnd> fe;

  // fe = std::make_unique<Termbox2>();
  if (argc == 2) fe = std::make_unique<Termbox2>();
  else           fe = std::make_unique<Raylib>();

  std::unique_ptr<IEditor> editor = IEditor::New();
  editor->SetFrontEnd(std::move(fe));


  editor->MainLoop();
  return 0;
}

