//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT



// TODO:
// The goal should be all pack together in as a single binary but also supports
// loading resources from file (mainly theme and treesitter language).
//
// Now:
//  Theme (ui) is a huge todo.
//  Editor::Error(), etc.
//  Window pos/area.
//
// Gap Buffer:
//   tree-sitter read callback
//   abstract the interface
//   user integers as characters
//   implement gap buffer.
//
//
// Pending:
//  - Loading config from file + (lsp config from config file).
//  - Remove tomlcpp library. We don't need it (maybe?)
//  - calculate area manually instead of draw so they can setup the view at start.
//  - Check if the LSP client exists and show an error message otherwise.
//  - fzf, rg check if exists.
//  - Opening new file should open in the split if the current tab has multi splits.
//    it's a single window tab, we start in a new tab.
//  - Clicking on tabname should change the tab.
//  - Jump to document should make the view center.
//  - open document in editor where language and lsp are solved from the path.
//    glue split positions (viw shouldn't move and the selection wont' change after modify in another split)
//    check what happens with empty Path with lsp server and handle.
//    open empty, search for a file with file picker.
//    Add another language server client and test. show errors and ask inputs.
//    Global config (tabsize), dropdown icons, dropdown list max count. lsp config.
//    Better draw diagnostics.
//  - Support true and 256 color from config.
//  - autocompletion:
//    - An abstract class for completion list.
//    - auto completion scrolling.
//  - Draw diagnostics the same line.
//  - Scroll if more tabs.

//
//   structure:
//     theme: getting values and proper, dynamic changeing (listener);
//     Utils class (maybe)
//
//  Note:
//    Drawing autocompletion is done at the document draw level, so split will
//      override the list if we draw another on top of.
//
// Big things:
//   gap buffer.
//   file tree.
//   mouse support.
//   load configs.
//   status line
//   prompt line + autocompletion (if them auto update).
//   number line + diagnos gutter.
//   scrollbar
//   macro.
//   open/close files.
//   copy paste clipboard.
//   terminal (maybe)
//
// FileTree:
//   cannot goto file tree from docwindow.
//   scroll if cursor is bellow
//   implement an iterator.
//   icons
//   newfile/rename/move/copy/delete.
//
// LSP:
//   pylsp completion insert text position has issue, it should replace only the text after '.'.
//     json.loa|
//     loads|
//
//   completion: additional text (like import statement).
//   goto definition.
//   rename.
//   get all references.
//   hover (maybe)
//   the signature help contains multiple signatures only send the active signature to the caller not an array.
//
// BUG:
//   drawing popup needs to be reviewed since if it goes out of the window, we just trim it but it needs to be pushed inside. (better draw primitives required)
//   signature help will hide pressing space after comma.
//   [bug in termbox] color 0x000000 (black) cannot be used as it will be replaced
//     with default color (use emacs theme compare with helix)
//     ref: https://github.com/nsf/termbox/issues/114
//
//  Cleanup things:
//    Registry of language server, and language => lsp mapping in the Editor:
//      { "clangd" : LspClient(), "pyright" : LspClient(), ...  }
//      { "c" : "clangd", "c++": "clangd",  "python" : "pyright", etc. }
//
//
// Unfinished, working things:
//   Filetree: everything is unfinished.
//   Icons api, provide nerd/unicode/ascii icons based on config.
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
//   Tab/split:
//     Tab name.
//     Tab scrolling if we have too many tabs.
//     Closing tabs (closing duplicats etc).
//     split size are fixed.
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
//   write tests (if I have time)
//   ipc timeout value hardcoded fix.
//   Maybe rename.
//




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

#include "core/core.hpp"
#include "platform/posix.hpp"
#include "frontend/frontend.hpp"

#include "editor/editor.hpp"
#include <tree_sitter/api.h>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;


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
  client.DidOpen(path, text, "c", 0);

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


// extern "C" const TSLanguage* tree_sitter_c(void);
// extern "C" const TSLanguage* tree_sitter_javascript(void);

void tree_sitter_test() {

//   const TSLanguage* lang = tree_sitter_c();
//   const char* query_source = R"!(
//   [
//     "case"
//     "}"
//     "]"
//     ")"
//   ] @outdent
//   )!";
//   uint32_t error_offset;
//   TSQueryError err;

//   const TSQuery* query = ts_query_new(lang,
//     query_source,
//     strlen(query_source),
//     &error_offset,
//     &err
//   );

//   TSParser* parser = ts_parser_new();
//   ts_parser_set_language(parser, lang);

//   const char* source_code = R"!(
//   {}
//   )!";
//   TSTree* tree = ts_parser_parse_string(
//       parser,
//       NULL,
//       source_code,
//       strlen(source_code)
//       );
//   // Print.
//   TSNode root_node = ts_tree_root_node(tree);
//   char* dump = ts_node_string(root_node);
//   printf("Syntax tree: %s\n", dump);


//   TSQueryCursor* cursor = ts_query_cursor_new();
//   ts_query_cursor_exec(cursor, query, root_node);

//   TSQueryMatch match;
//   if (false) {
//     uint32_t capture_index;
//     while (ts_query_cursor_next_capture(cursor, &match, &capture_index)) {
//       printf("capture_count=%i\n", match.capture_count);
//       printf("pattern_index=%i\n", match.pattern_index);
//       for (int i = 0; i < match.capture_count; i++) {
//         uint32_t len;
//         const char* cap_name = ts_query_capture_name_for_id(query, match.captures[i].index, &len);
//         printf("capture_name=%*s\n", len, cap_name);
//       }
//       printf("\n\n");
//     }
//   } else {

//     while (ts_query_cursor_next_match(cursor, &match)) {
//       printf("capture_count=%i\n", match.capture_count);
//       printf("pattern_index=%i\n", match.pattern_index);
//       for (int i = 0; i < match.capture_count; i++) {
//         uint32_t len;
//         const char* cap_name = ts_query_capture_name_for_id(query, match.captures[i].index, &len);
//         printf("capture_name=%*s\n", len, cap_name);
//         char* s = ts_node_string(root_node);
//         printf("Syntax tree: %s\n\n", s);
//         printf("start=%i, end=%i\n\n",
//           ts_node_start_byte(match.captures[i].node),
//           ts_node_end_byte(match.captures[i].node)
//         );
//         free(s);
//       }
//       printf("\n");

//     }

//   }

//   // TSNode expr_stmnt_node = ts_node_named_child(root_node, 0);
//   // TSNode array_node = ts_node_named_child(expr_stmnt_node, 0);
//   // printf("%s\n", ts_node_type(array_node));

//   // for (int i = 0; i < ts_node_child_count(array_node); i++) {
//   //   TSNode n = ts_node_child(array_node, i);
//   //   printf("%s\n", ts_node_type(n));
//   // }

//   // printf("cc = %i\n", );
//   // printf("nc = %i\n", ts_node_named_child_count(array_node));

//   free(dump);
//   ts_tree_delete(tree);
//   ts_parser_delete(parser);
//   return;
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

  // fzf_things_test();
  // return 0;

  // theme_test();
  // return 0;

  // tree_sitter_test();
  // return 0;

  // lsp_test();
  // return 0;



  // FIXME: This shoul be called after the editor is initialized to send errors.

  std::string name;

  name = Ui::ClassName();

  Ui::keytree.RegisterAction(name, "popup_files_finder",  Ui::Action_PopupFilesFinder);
  Ui::keytree.RegisterAction(name, "popup_live_grep", Ui::Action_PopupLiveGrep);
  Ui::keytree.RegisterAction(name, "new_file", Ui::Action_NewDocument);
  Ui::keytree.RegisterAction(name, "tab_next", Ui::Action_TabNext);
  Ui::keytree.RegisterAction(name, "tab_prev", Ui::Action_TabPrev);
  Ui::keytree.RegisterAction(name, "vsplit", Ui::Action_Vsplit);
  Ui::keytree.RegisterAction(name, "hsplit", Ui::Action_Hsplit);
  Ui::keytree.RegisterAction(name, "next_window", Ui::Action_NextWindow);
  Ui::keytree.RegisterAction(name, "close_window", Ui::Action_CloseWindow);

  Ui::keytree.RegisterBinding(name, "<C-o>", "popup_files_finder");
  Ui::keytree.RegisterBinding(name, "<C-g>", "popup_live_grep");
  Ui::keytree.RegisterBinding(name, "<C-n>", "new_file");
  Ui::keytree.RegisterBinding(name, "<C-l>", "tab_next");
  Ui::keytree.RegisterBinding(name, "<C-h>", "tab_prev");
  Ui::keytree.RegisterBinding(name, "<C-w><C-w>", "next_window");
  Ui::keytree.RegisterBinding(name, "<C-w><C-v>", "vsplit");
  Ui::keytree.RegisterBinding(name, "<C-w>v", "vsplit");
  Ui::keytree.RegisterBinding(name, "<C-w><C-h>", "hsplit");
  Ui::keytree.RegisterBinding(name, "<C-w>h", "hsplit");
  Ui::keytree.RegisterBinding(name, "<C-w>w", "next_window");
  Ui::keytree.RegisterBinding(name, "<C-w>c", "close_window");

  name = DocumentWindow::ClassName();

  Ui::keytree.RegisterAction(name, "cursor_up", (FuncAction) DocumentWindow::Action_CursorUp);
  Ui::keytree.RegisterAction(name, "cursor_down", (FuncAction) DocumentWindow::Action_CursorDown);
  Ui::keytree.RegisterAction(name, "cursor_left", (FuncAction) DocumentWindow::Action_CursorLeft);
  Ui::keytree.RegisterAction(name, "cursor_right", (FuncAction) DocumentWindow::Action_CursorRight);
  Ui::keytree.RegisterAction(name, "cursor_end", (FuncAction) DocumentWindow::Action_CursorEnd);
  Ui::keytree.RegisterAction(name, "cursor_home", (FuncAction) DocumentWindow::Action_CursorHome);
  Ui::keytree.RegisterAction(name, "select_right", (FuncAction) DocumentWindow::Action_SelectRight);
  Ui::keytree.RegisterAction(name, "select_left", (FuncAction) DocumentWindow::Action_SelectLeft);
  Ui::keytree.RegisterAction(name, "select_up", (FuncAction) DocumentWindow::Action_SelectUp);
  Ui::keytree.RegisterAction(name, "select_down", (FuncAction) DocumentWindow::Action_SelectDown);
  Ui::keytree.RegisterAction(name, "select_home", (FuncAction) DocumentWindow::Action_SelectHome);
  Ui::keytree.RegisterAction(name, "select_end", (FuncAction) DocumentWindow::Action_SelectEnd);
  Ui::keytree.RegisterAction(name, "add_cursor_down", (FuncAction) DocumentWindow::Action_AddCursor_down);
  Ui::keytree.RegisterAction(name, "add_cursor_up", (FuncAction) DocumentWindow::Action_AddCursor_up);
  Ui::keytree.RegisterAction(name, "insert_space", (FuncAction) DocumentWindow::Action_InsertSpace);
  Ui::keytree.RegisterAction(name, "insert_newline", (FuncAction) DocumentWindow::Action_InsertNewline);
  Ui::keytree.RegisterAction(name, "insert_tab", (FuncAction) DocumentWindow::Action_InsertTab);
  Ui::keytree.RegisterAction(name, "backspace", (FuncAction) DocumentWindow::Action_Backspace);
  Ui::keytree.RegisterAction(name, "undo", (FuncAction) DocumentWindow::Action_Undo);
  Ui::keytree.RegisterAction(name, "redo", (FuncAction) DocumentWindow::Action_Redo);
  Ui::keytree.RegisterAction(name, "trigger_completion", (FuncAction) DocumentWindow::Action_TriggerCompletion);
  Ui::keytree.RegisterAction(name, "cycle_completion_list", (FuncAction) DocumentWindow::Action_CycleCompletionList);
  Ui::keytree.RegisterAction(name, "cycle_completion_list_reversed", (FuncAction) DocumentWindow::Action_CycleCompletionListReversed);
  Ui::keytree.RegisterAction(name, "clear", (FuncAction) DocumentWindow::Action_Clear);
  Ui::keytree.RegisterAction(name, "normal_mode", (FuncAction) DocumentWindow::Action_NormalMode);
  Ui::keytree.RegisterAction(name, "insert_mode", (FuncAction) DocumentWindow::Action_InsertMode);

  Ui::keytree.RegisterBinding(name, "insert", "<up>",        "cursor_up");
  Ui::keytree.RegisterBinding(name, "insert", "<down>",      "cursor_down");
  Ui::keytree.RegisterBinding(name, "insert", "<left>",      "cursor_left");
  Ui::keytree.RegisterBinding(name, "insert", "<right>",     "cursor_right");
  Ui::keytree.RegisterBinding(name, "insert", "<home>",      "cursor_home");
  Ui::keytree.RegisterBinding(name, "insert", "<end>",       "cursor_end");
  Ui::keytree.RegisterBinding(name, "insert", "<S-right>",   "select_right");
  Ui::keytree.RegisterBinding(name, "insert", "<S-left>",    "select_left");
  Ui::keytree.RegisterBinding(name, "insert", "<S-up>",      "select_up");
  Ui::keytree.RegisterBinding(name, "insert", "<S-down>",    "select_down");
  Ui::keytree.RegisterBinding(name, "insert", "<S-home>",    "select_home");
  Ui::keytree.RegisterBinding(name, "insert", "<S-end>",     "select_end");
  Ui::keytree.RegisterBinding(name, "insert", "<A-down>",    "add_cursor_down");
  Ui::keytree.RegisterBinding(name, "insert", "<A-up>",      "add_cursor_up");
  Ui::keytree.RegisterBinding(name, "insert", "<space>",     "insert_space");
  Ui::keytree.RegisterBinding(name, "insert", "<enter>",     "insert_newline");
  Ui::keytree.RegisterBinding(name, "insert", "<tab>",       "insert_tab");
  Ui::keytree.RegisterBinding(name, "insert", "<backspace>", "backspace");
  Ui::keytree.RegisterBinding(name, "insert", "<C-z>",       "undo");
  Ui::keytree.RegisterBinding(name, "insert", "<C-y>",       "redo");
  Ui::keytree.RegisterBinding(name, "insert", "<C-x><C-k>",  "trigger_completion");
  Ui::keytree.RegisterBinding(name, "insert", "<C-n>",       "cycle_completion_list");
  Ui::keytree.RegisterBinding(name, "insert", "<C-p>",       "cycle_completion_list_reversed");
  Ui::keytree.RegisterBinding(name, "insert", "<esc>",       "clear");

  Ui::keytree.RegisterBinding(name, "insert", "<C-d>",       "normal_mode");
  Ui::keytree.RegisterBinding(name, "normal", "i",           "insert_mode");
  Ui::keytree.RegisterBinding(name, "normal", "h",           "cursor_left");
  Ui::keytree.RegisterBinding(name, "normal", "l",           "cursor_right");
  Ui::keytree.RegisterBinding(name, "normal", "k",           "cursor_up");
  Ui::keytree.RegisterBinding(name, "normal", "j",           "cursor_down");
  Ui::keytree.RegisterBinding(name, "normal", "gh",          "cursor_home");
  Ui::keytree.RegisterBinding(name, "normal", "gl",          "cursor_end");
  Ui::keytree.RegisterBinding(name, "normal", "diw",         "backspace");

  name = FindWindow::ClassName();

  Ui::keytree.RegisterAction(name, "cursor_right", (FuncAction) FindWindow::Action_CursorRight);
  Ui::keytree.RegisterAction(name, "cursor_left", (FuncAction) FindWindow::Action_CursorLeft);
  Ui::keytree.RegisterAction(name, "cursor_home", (FuncAction) FindWindow::Action_CursorHome);
  Ui::keytree.RegisterAction(name, "cursor_end", (FuncAction) FindWindow::Action_CursorEnd);
  Ui::keytree.RegisterAction(name, "backspace", (FuncAction) FindWindow::Action_Backspace);
  Ui::keytree.RegisterAction(name, "cycle_selection", (FuncAction) FindWindow::Action_CycleSelection);
  Ui::keytree.RegisterAction(name, "cycle_selection_reversed", (FuncAction) FindWindow::Action_CycleSelectionReversed);
  Ui::keytree.RegisterAction(name, "accept_selection", (FuncAction) FindWindow::Action_AcceptSelection);
  Ui::keytree.RegisterAction(name, "close", (FuncAction) FindWindow::Action_Close);

  Ui::keytree.RegisterBinding(name, "<right>", "cursor_right");
  Ui::keytree.RegisterBinding(name, "<left>",  "cursor_left");
  Ui::keytree.RegisterBinding(name, "<home>",  "cursor_home");
  Ui::keytree.RegisterBinding(name, "<end>",  "cursor_end");
  Ui::keytree.RegisterBinding(name, "<backspace>",  "backspace");
  Ui::keytree.RegisterBinding(name, "<C-n>",  "cycle_selection");
  Ui::keytree.RegisterBinding(name, "<C-p>",  "cycle_selection_reversed");
  Ui::keytree.RegisterBinding(name, "<down>",  "cycle_selection");
  Ui::keytree.RegisterBinding(name, "<up>",  "cycle_selection_reversed");
  Ui::keytree.RegisterBinding(name, "<tab>",  "cycle_selection");
  Ui::keytree.RegisterBinding(name, "<S-tab>", "cycle_selection_reversed");
  Ui::keytree.RegisterBinding(name, "<enter>", "accept_selection");
  Ui::keytree.RegisterBinding(name, "<esc>", "close");

  // name = FileTreeWindow::ClassName();

  // Ui::keytree.RegisterAction(name, "cursor_up", (FuncAction) FileTreeWindow::Action_CursorUp);
  // Ui::keytree.RegisterAction(name, "cursor_down", (FuncAction) FileTreeWindow::Action_CursorDown);
  // Ui::keytree.RegisterAction(name, "select", (FuncAction) FileTreeWindow::Action_SelectPath);
  // Ui::keytree.RegisterAction(name, "goto_parent", (FuncAction) FileTreeWindow::Action_GotoParent);

  // Ui::keytree.RegisterBinding(name, "<up>",    "cursor_up");
  // Ui::keytree.RegisterBinding(name, "<down>",  "cursor_down");
  // Ui::keytree.RegisterBinding(name, "k",       "cursor_up");
  // Ui::keytree.RegisterBinding(name, "j",       "cursor_down");
  // Ui::keytree.RegisterBinding(name, "<enter>", "select");
  // Ui::keytree.RegisterBinding(name, "o",       "select");
  // Ui::keytree.RegisterBinding(name, "p",       "goto_parent");


  // For testing.
  Ui::keytree.RegisterBinding("DocumentWindow", "insert", "<C-e>a",     "cursor_up");
  Ui::keytree.RegisterBinding("Ui", "<C-e>b",     "next_window");


  std::shared_ptr<Editor> editor = Editor::Singleton();

  // Register themes.
  std::map<std::string, Json> theme_data = Platform::LoadThemes();
  for (auto& it : theme_data) {
    editor->RegisterTheme(it.first, std::make_shared<Theme>(it.second));
  }

  // Register languages.
  for (const LanguageLoadResult& result : Platform::LoadLanguages()) {
    std::shared_ptr<Language> lang = std::make_shared<Language>();
    if (result.query_highlight != NULL) {
      if (result.tree_sitter_loader) lang->data = result.tree_sitter_loader();

      // TODO: check for errors.
      uint32_t error_offset; TSQueryError err;

      TSQuery* query = ts_query_new(
          lang->data, // Note that the loader should be called above.
          result.query_highlight,
          strlen(result.query_highlight),
          &error_offset, &err);
      lang->query_highlight = query;

      ASSERT(query != NULL, OOPS); // FIXME: check for errors and handle properly.
    }
    editor->RegisterLanguage(result.language_id, lang);
  }

  // TODO: Load them from config file or somewhere.
  // Register LSP clients.
  {
    LspConfig config;
    config.id = "clangd";
    config.server_file = "clangd";
    editor->RegisterLspClient(config);
  }
  {
    LspConfig config;
    config.id = "pylsp";
    config.server_file = "pylsp";
    editor->RegisterLspClient(config);
  }

  std::unique_ptr<IFrontEnd> frontend = std::make_unique<Termbox2>();
  editor->SetFrontEnd(std::move(frontend));

  std::unique_ptr<Ui> ui = std::make_unique<Ui>();

#if 0 // Split test.

  // FIXME:
  Path path("/Users/thakeenathees/Desktop/thakee/repos/vmacs/build/main.cpp");
  std::shared_ptr<Document> doc = e->OpenDocument(path);
  ASSERT(doc != nullptr, OOPS);
  std::shared_ptr<const Language> lang = e->GetLanguage("cpp");
  ASSERT(lang != nullptr, OOPS);
  std::shared_ptr<LspClient> client = e->GetLspClient("clangd");
  ASSERT(client != nullptr, OOPS);
  doc->SetLanguage(lang);
  doc->SetLspClient(client);

  std::unique_ptr<DocumentWindow> docwindow1 = std::make_unique<DocumentWindow>(doc);
  std::unique_ptr<DocumentWindow> docwindow2 = std::make_unique<DocumentWindow>(doc);
  std::unique_ptr<DocumentWindow> docwindow3 = std::make_unique<DocumentWindow>(doc);

  std::unique_ptr<Split> root = std::make_unique<Split>();
  auto s1 = root->Vsplit(true);
  s1->Hsplit(true);
  auto it = root->Iterate();
  it.Get()->SetWindow(std::move(docwindow1));
  it.Next();
  it.Get()->SetWindow(std::move(docwindow2));
  it.Next();
  it.Get()->SetWindow(std::move(docwindow3));
  it.Next();
  ASSERT(it.Get() == nullptr, OOPS);

  std::unique_ptr<Tab> tab = std::make_unique<Tab>(std::move(root));
  ui->AddTab(std::move(tab));

#else
  // std::unique_ptr<DocumentWindow> docwindow = std::make_unique<DocumentWindow>(doc);
  // root->SetWindow(std::move(docwindow));

  // std::unique_ptr<IniWindow> win = std::make_unique<IniWindow>();
  // root->SetWindow(std::move(win));
#endif

  // std::unique_ptr<FileTreeWindow> filetree = std::make_unique<FileTreeWindow>(tree);
  // ui->AddTab(Tab::FromWindow(std::move(filetree)), -1);

  editor->SetUi(std::move(ui));
  editor->MainLoop();
  return 0;
}

