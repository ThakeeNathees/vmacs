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

#include "lsp/client.hpp"
#include "ui/ui.hpp"

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
  client.StartServer();

  Path path = Path("/Users/thakeenathees/Desktop/thakee/repos/vmacs/build/main.c");
  std::string x;

  std::string text;
  ASSERT(Platform::ReadFile(&text, path), "My ugly code");
  client.DidOpen(path, text, "c");

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
        { "uri",  path.Uri() },
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
//
//
// Pending:
//   check what happens with empty Path with lsp server and handle.
//   open empty, search for a file with file picker.
//   Add another language server client and test. show errors and ask inputs.
//   Global config (tabsize), dropdown icons, dropdown list max count. lsp config.
//   Better draw diagnostics.
//   remove global thread stop and handle locally
//     document depends on redraw and: use listeners.
//     synax depends on get theme: use callback to fetch theme.
//
//   structure:
//     config move.
//     theme: getting values and proper, dynamic changeing (listener);
//     cleanup what ever we have and try to complete: before adding more things.
//     Platform interface and Utils class.
//
//  Note:
//    Document shouldn't draw autocompletion if it's in a pane that's not focused.
//      and if the same document opened in two panes, only one show the list.
//    Drawing autocompletion is done at the document draw level, so split will
//      override the list if we draw another on top of.
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
//   esc doesn't clear the selection.
//   [bug in termbox] color 0x000000 (black) cannot be used as it will be replaced
//     with default color (use emacs theme compare with helix)
//     ref: https://github.com/nsf/termbox/issues/114
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

  // FIXME: This shoul be called after the editor is initialized to send errors.

  Window::keytree.RegisterAction("close_popup",  Window::Action_ClosePopup);
  Window::keytree.RegisterAction("popup_files_finder",  Window::Action_PopupFilesFinder);

  Window::keytree.RegisterBinding("*", "<esc>", "close_popup");
  Window::keytree.RegisterBinding("*", "<C-o>", "popup_files_finder");


  DocPane::keytree.RegisterAction("cursor_up", (FuncAction) DocPane::Action_CursorUp);
  DocPane::keytree.RegisterAction("cursor_down", (FuncAction) DocPane::Action_CursorDown);
  DocPane::keytree.RegisterAction("cursor_left", (FuncAction) DocPane::Action_CursorLeft);
  DocPane::keytree.RegisterAction("cursor_right", (FuncAction) DocPane::Action_CursorRight);
  DocPane::keytree.RegisterAction("cursor_end", (FuncAction) DocPane::Action_CursorEnd);
  DocPane::keytree.RegisterAction("cursor_home", (FuncAction) DocPane::Action_CursorHome);
  DocPane::keytree.RegisterAction("select_right", (FuncAction) DocPane::Action_SelectRight);
  DocPane::keytree.RegisterAction("select_left", (FuncAction) DocPane::Action_SelectLeft);
  DocPane::keytree.RegisterAction("select_up", (FuncAction) DocPane::Action_SelectUp);
  DocPane::keytree.RegisterAction("select_down", (FuncAction) DocPane::Action_SelectDown);
  DocPane::keytree.RegisterAction("select_home", (FuncAction) DocPane::Action_SelectHome);
  DocPane::keytree.RegisterAction("select_end", (FuncAction) DocPane::Action_SelectEnd);
  DocPane::keytree.RegisterAction("add_cursor_down", (FuncAction) DocPane::Action_AddCursor_down);
  DocPane::keytree.RegisterAction("add_cursor_up", (FuncAction) DocPane::Action_AddCursor_up);
  DocPane::keytree.RegisterAction("insert_space", (FuncAction) DocPane::Action_InsertSpace);
  DocPane::keytree.RegisterAction("insert_newline", (FuncAction) DocPane::Action_InsertNewline);
  DocPane::keytree.RegisterAction("insert_tab", (FuncAction) DocPane::Action_InsertTab);
  DocPane::keytree.RegisterAction("backspace", (FuncAction) DocPane::Action_Backspace);
  DocPane::keytree.RegisterAction("undo", (FuncAction) DocPane::Action_Undo);
  DocPane::keytree.RegisterAction("redo", (FuncAction) DocPane::Action_Redo);
  DocPane::keytree.RegisterAction("trigger_completion", (FuncAction) DocPane::Action_TriggerCompletion);
  DocPane::keytree.RegisterAction("cycle_completion_list", (FuncAction) DocPane::Action_CycleCompletionList);
  DocPane::keytree.RegisterAction("cycle_completion_list_reversed", (FuncAction) DocPane::Action_CycleCompletionListReversed);
  DocPane::keytree.RegisterAction("clear", (FuncAction) DocPane::Action_Clear);

  DocPane::keytree.RegisterBinding("*", "<up>",        "cursor_up");
  DocPane::keytree.RegisterBinding("*", "<down>",      "cursor_down");
  DocPane::keytree.RegisterBinding("*", "<left>",      "cursor_left");
  DocPane::keytree.RegisterBinding("*", "<right>",     "cursor_right");
  DocPane::keytree.RegisterBinding("*", "<home>",      "cursor_home");
  DocPane::keytree.RegisterBinding("*", "<end>",       "cursor_end");
  DocPane::keytree.RegisterBinding("*", "<S-right>",   "select_right");
  DocPane::keytree.RegisterBinding("*", "<S-left>",    "select_left");
  DocPane::keytree.RegisterBinding("*", "<S-up>",      "select_up");
  DocPane::keytree.RegisterBinding("*", "<S-down>",    "select_down");
  DocPane::keytree.RegisterBinding("*", "<S-home>",    "select_home");
  DocPane::keytree.RegisterBinding("*", "<S-end>",     "select_end");
  DocPane::keytree.RegisterBinding("*", "<M-down>",    "add_cursor_down");
  DocPane::keytree.RegisterBinding("*", "<M-up>",      "add_cursor_up");
  DocPane::keytree.RegisterBinding("*", "<space>",     "insert_space");
  DocPane::keytree.RegisterBinding("*", "<enter>",     "insert_newline");
  DocPane::keytree.RegisterBinding("*", "<tab>",       "insert_tab");
  DocPane::keytree.RegisterBinding("*", "<backspace>", "backspace");
  DocPane::keytree.RegisterBinding("*", "<C-z>",       "undo");
  DocPane::keytree.RegisterBinding("*", "<C-y>",       "redo");
  DocPane::keytree.RegisterBinding("*", "<C-x><C-k>",  "trigger_completion");
  DocPane::keytree.RegisterBinding("*", "<C-n>",       "cycle_completion_list");
  DocPane::keytree.RegisterBinding("*", "<C-p>",       "cycle_completion_list_reversed");
  DocPane::keytree.RegisterBinding("*", "<esc>",       "clear");



  FindPane::keytree.RegisterAction("cursor_right", (FuncAction) FindPane::Action_CursorRight);
  FindPane::keytree.RegisterAction("cursor_left", (FuncAction) FindPane::Action_CursorLeft);
  FindPane::keytree.RegisterAction("cursor_home", (FuncAction) FindPane::Action_CursorHome);
  FindPane::keytree.RegisterAction("cursor_end", (FuncAction) FindPane::Action_CursorEnd);
  FindPane::keytree.RegisterAction("backspace", (FuncAction) FindPane::Action_Backspace);
  FindPane::keytree.RegisterAction("cycle_selection", (FuncAction) FindPane::Action_CycleSelection);
  FindPane::keytree.RegisterAction("cycle_selection_reversed", (FuncAction) FindPane::Action_CycleSelectionReversed);
  FindPane::keytree.RegisterAction("accept_selection", (FuncAction) FindPane::Action_AcceptSelection);

  FindPane::keytree.RegisterBinding("*", "<right>", "cursor_right");
  FindPane::keytree.RegisterBinding("*", "<left>",  "cursor_left");
  FindPane::keytree.RegisterBinding("*", "<home>",  "cursor_home");
  FindPane::keytree.RegisterBinding("*", "<end>",  "cursor_home");
  FindPane::keytree.RegisterBinding("*", "<backspace>",  "backspace");
  FindPane::keytree.RegisterBinding("*", "<C-n>",  "cycle_selection");
  FindPane::keytree.RegisterBinding("*", "<C-p>",  "cycle_selection_reversed");
  FindPane::keytree.RegisterBinding("*", "<down>",  "cycle_selection");
  FindPane::keytree.RegisterBinding("*", "<up>",  "cycle_selection_reversed");
  FindPane::keytree.RegisterBinding("*", "<tab>",  "cycle_selection");
  FindPane::keytree.RegisterBinding("*", "<S-tab>", "cycle_selection_reversed");
  FindPane::keytree.RegisterBinding("*", "<enter>", "accept_selection");



  std::unique_ptr<FrontEnd> fe;

  if (argc == 2) fe = std::make_unique<Termbox2>();
  else           fe = std::make_unique<Raylib>();

  std::shared_ptr<IEditor> editor = IEditor::Singleton();
  editor->SetFrontEnd(std::move(fe));

  Editor* e = (Editor*) editor.get();


  // FIXME:
  Path path("/Users/thakeenathees/Desktop/thakee/repos/vmacs/build/main.cpp");
  std::shared_ptr<Document> doc = e->OpenDocument(path);
  ASSERT(doc != nullptr, OOPS);
  doc->SetThemeGetter([](){
      return Editor::GetCurrentTheme();
  });
  std::shared_ptr<const Language> lang = e->GetLanguage("cpp");
  ASSERT(lang != nullptr, OOPS);
  std::shared_ptr<LspClient> client = e->GetLspClient("clangd");
  ASSERT(client != nullptr, OOPS);
  doc->SetLanguage(lang);
  doc->SetLspClient(client);

  std::unique_ptr<DocPane> docpane1 = std::make_unique<DocPane>(doc);
  std::unique_ptr<DocPane> docpane2 = std::make_unique<DocPane>(doc);
  std::unique_ptr<DocPane> docpane3 = std::make_unique<DocPane>(doc);

  std::unique_ptr<Split> root = std::make_unique<Split>();

  root->Vsplit(true);
  root->GetChild(0)->SetPane(std::move(docpane1));

  root->GetChild(1)->Hsplit(true);
  root->GetChild(1)->GetChild(0)->SetPane(std::move(docpane2));
  root->GetChild(1)->GetChild(1)->SetPane(std::move(docpane3));

  // Split* active = root->GetChild(1);
  std::unique_ptr<Tab> tab = std::make_unique<Tab>(
      std::move(root));

  std::unique_ptr<Window> window = std::make_unique<Window>();
  window->AddTab(std::move(tab));

  e->SetWindow(std::move(window));

  e->MainLoop();
  return 0;
}

