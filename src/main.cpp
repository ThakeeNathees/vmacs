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
//   cursor blink
//
//
// Top priority:
//
//
// Bugs:
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

