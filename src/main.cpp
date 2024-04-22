//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT


// TODO:
//
// Now:
//   cleanup diagnostic mess make it properly structured.
//   range include check algo.
//
//
// LSP:
//   Lsp working proto:
//     Diagnos & goto def should work
//
//
// Top priority:
//   Editor has a "registry" of {uri: document}. (can be used with lsp)
//   LSP methods instead of direct json.
//   Each response from server should be sent to the Editor and will be handled. there.
//
// Bugs:
//  Remove all the lsp clients when the editor die, or it'll block forever.
//  <right> <left> on empty selection.
//  startup time is a bit high, maybe because of raylib ??? try disabling.
//
// Pending:
//
//   multi cursor.
//   line numbers
//   global configs (tabwidth, ).
//   debug print that accumilate logs and dumps at the end.
//   pair and auto indent
//   termbox non-blocking event polling.
//   editor modes implement properly.
//   default bindnigs and tmeme settings.
//   treesitter and syntax highlighting.
//   status bar
//   prompt bar
//   global atomic bool to say all the while (true) loop thread to end.
//
//   Can't bind to 12j, d10|, ... with numbers.
//
// Code quality:
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


// std::string ReadAll(const std::string& path) {
//   std::ifstream inputFile(path.data());
//   assert(inputFile.is_open() && "Failed to open file.");
//   std::stringstream buffer;
//   buffer << inputFile.rdbuf();
//   inputFile.close();
//   return buffer.str();
// }

// void lsp_test() {

//   LspConfig config;
//   config.server = "clangd";

//   LspClient client(config);
//   client.StartServer(std::nullopt);

//   std::string path = "/Users/thakeenathees/Desktop/thakee/temp/lsp/main.c";
//   std::string x;

//   // std::cin >> x;
//   client.SendNotification(
//       "textDocument/didOpen", {
//       {
//         "textDocument", {
//           { "uri",  std::string("file://") + path },
//           { "text", ReadAll(path) },
//           { "languageId", "c" },
//         }
//       }
//   });

//   // goto definition.
//   // std::cin >> x;
//   client.SendRequest(
//     "textDocument/definition", {
//     {
//       "textDocument", {
//         { "uri",  std::string("file://") + path },
//       },
//     },
//     {
//       "position", {
//         { "line", 3  },
//         { "character", 2 },
//       },
//     }
//   });
  
// }





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

  std::unique_ptr<FrontEnd> fe;

  // fe = std::make_unique<Termbox2>();
  if (argc == 2) fe = std::make_unique<Termbox2>();
  else           fe = std::make_unique<Raylib>();

  std::unique_ptr<IEditor> editor = IEditor::New();
  editor->SetFrontEnd(std::move(fe));

  editor->MainLoop();
  
  // if (!editor->Initialize()) {
  //   printf("initialize failed\n");
  //   return 1;
  // }

  // // What a mess.
  // Editor* e = (Editor*) editor.get();
  // e->OpenDocument("/Users/thakeenathees/Desktop/thakee/temp/lsp/main.c");

  // while (editor->Running()) {
  //   // FIXME: Draw first and handle because termbox is blocking, fix and
  //   // change the order.
  //   editor->Draw();
  //   editor->HandleEvents();

  // }
  // editor->Cleanup();

  return 0;
}

