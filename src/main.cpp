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
#include "platform/platform.hpp"

#include "lsp/lsp.hpp"

#include "frontend/raylib/fe_raylib.hpp"
#include "frontend/termbox2/fe_termbox2.hpp"

#include "editor/editor.hpp"
#include <tree_sitter/api.h>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;

#include "platform/posix.hpp"

void fzf_things_test() {
  IPC::IpcOptions opt;
  opt.timeout_sec = 3;

  // opt.file = "sh";
  // // opt.argv = {  "-c", "fd | fzf --filter edit" };
  // opt.argv = {  "-c", "find . -type f | fzf --filter buffer" };

  // opt.file = "sh";
  // opt.argv = {  "-c", "rg editor" };

  // opt.argv = {  "-c", "find . -type f | fzf --filter buffer" };
  // opt.file = "grep";
  // opt.argv = {  "-rn", "editor" };

  opt.file = "fzf";
  opt.argv = {  "--read0", "--filter", "hello" };
  opt.sending_inputs = true;

  opt.stdout_cb = [] (void*, const char* buff, size_t len) {
    printf("out: %*s\n", (int)len, buff);
  };
  opt.stderr_cb = [](void*, const char* buff, size_t len) {
    printf("err: %*s\n", (int)len, buff);
  };

  bool done = false;
  opt.exit_cb = [&done](void*, int) {
    done = true;
    printf("exitted.\n");
  };

  auto ipc = IPC::New(opt);
  ipc->WriteToStdin("hello1\n");
  ipc->WriteToStdin("hello2\n");
  ipc->WriteToStdin(std::string("hello3\n\0", 8));
  ipc->StopListening();
  ipc->Run();

  // printf(">> ");
  // std::string s;
  // std::cin >> s;
  while (!done);
}


void lsp_test() {

  LspConfig config;
  config.id = "clangd";

  LspClient client(config);
  client.StartServer(std::nullopt);

  std::string path = "/Users/thakeenathees/Desktop/thakee/repos/vmacs/build/main.c";
  Uri uri = std::string("file://") + path;
  std::string x;

  std::string text;
  ASSERT(Platform::ReadFile(&text, path), "My ugly code");
  client.DidOpen(uri, text, "c");

  // goto definition.
  // std::cin >> x;
  // client.SendRequest(
  //   "textDocument/definition", {
  //   {
  //     "textDocument", {
  //       { "uri",  std::string("file://") + path },
  //     },
  //   },
  //   {
  //     "position", {
  //       { "line", 5  },
  //       { "character", 2 },
  //     },
  //   }
  // });

  { std::string s; std::cin >> s; }
  client.SendRequest(
    "textDocument/completion", {
    {
      "textDocument", {
        { "uri",  std::string("file://") + path },
      },
    },
    {
      "position", {
        { "line", 2 },
        { "character", 9 },
      },
    }
  });


  // Prevent the client from destroy before we got response from server.
  { std::string s; std::cin >> s; }

}


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
// The goal should be all pack together in as a single binary but also supports
// loading resources from file (mainly theme and treesitter language).
//
// Now:
//   open empty, search for a file with file picker (BUG: file picker buffer unfinished input), open and edit.
//   Add another language server client and test. show errors and ask inputs.
//
//
// Pending:
//   Global config (tabsize), dropdown icons, dropdown list max count. lsp config.
//   Better draw diagnostics.
//   remove global thread stop and handle locally.
//
//
//   structure:
//     config move.
//     theme: getting values and proper, dynamic changeing (listener);
//   cleanup what ever we have and try to complete: before adding more things.
//   Platform interface and Utils class.
//   thememes and querty integrate.
//
// Big things:
//   gap buffer.
//   file tree.
//   mouse support.
//   load configs.
//   status line
//
//   prompt line + autocompletion (if them auto update).
//   number line + diagnos gutter.
//   scrollbar
//   macro.
//   open/close files.
//   splits and tabs.
//   copy paste clipboard.
//   nerdfont support (maybe not)
//   terminal (maybe)
//
// LSP:
//   completion: additional text (like import statement).
//   goto definition.
//   rename.
//   get all references.
//   hover (maybe)
//   the signature help contains multiple signatures only send the active signature to the caller not an array.
//
// BUG:
//   draw auto completion popup only in the current focused pane.
//   drawing popup needs to be reviewed since if it goes out of the window, we just trim it but it needs to be pushed inside. (better draw primitives required)
//   signature help will hide pressing space after comma.
//
//  Cleanup things:
//    Registry of language server, and language => lsp mapping in the Editor:
//      { "clangd" : LspClient(), "pyright" : LspClient(), ...  }
//      { "c" : "clangd", "c++": "clangd",  "python" : "pyright", etc. }
//    Theme manager...
//    Global Configs.
//
//
// Unfinished, working things:
//   autocompletion + (show documnt, symbol helper for parameter, icon, etc.)
//   autocompletion selection of items. icon config.
//   Theme loading from file and swith theme, theme listening for change etc.
//   proper keybinding and loading that from config.
//   auto completion -> scroll if we cycle and add a scroll bar depends on better drawing code. (not current temp).
//   IPC instead of "sh" as the file, use correct file with va_args (variadic parameters);
//   finder (picker):
//     (file, rgrep, diagnos, buffer, symbols(treesitter))
//     preview.
//     actoin selection.
//
// Pending:
//   <S-tab> is not bindable (but should be. "\x1b[Z")
//
//   keybinding with logical like micro: (&, |, ',') (comman run sequence).
//     <tab> : "cycle_completion_list | insert_tab"
//     <S-tab> : "cycle_completion_list_reversed | insert_tab"
//
//   Need to check if fzf/rg exists in the system and report (implement fallback tools).
//
//   Main loop should be handled by the front end (so raylib can draw forever and termbox2 don't have to). and FE will set the FPS.
//   editor send events and ask draw in a structured manner.
//   M-c rename to A-c (M doesn't make sence, even in Eamcs)
//   proper draw system. (request client for buffer and fill only if it needs to re-draw).
//   line numbers.
//   global configs (tabwidth, ).
//   debug print that accumilate logs and dumps at the end.
//   pair and auto indent
//   editor modes implement properly.
//   default bindnigs and tmeme settings.
//   treesitter and syntax highlighting.
//   status bar; prompt bar
//   multi cursor select next match what under cursor (like micro)
//
//   Can't bind to 12j, d10|, ... with numbers.
//
// Code quality:
//   Position and Coord names are alike (change Position -> Coord, Coord-> BuffPos)
//   Write a clang-formater and apply through all the source.
//   change all DrawBuffer parameter as reference than value.
//   remove all the trailing white spaces all around the source.
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
//







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

  // fzf_things_test();
  // return 0;

  // theme_test();
  // return 0;

  // tree_sitter_test();
  // return 0;

  // lsp_test();
  // return 0;

  std::unique_ptr<FrontEnd> fe;

  if (argc == 2) fe = std::make_unique<Termbox2>();
  else           fe = std::make_unique<Raylib>();

  std::shared_ptr<IEditor> editor = IEditor::Singleton();
  editor->SetFrontEnd(std::move(fe));

  editor->MainLoop();
  return 0;
}

