//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "buffer/buffer.hpp"
#include "lsp/lsp.hpp"


// TODO: Move this to somewhere else.
#define BUFF_CELL(buff, x, y)\
  (buff).cells[ (buff).width * ((y)) + (x)  ]

#define SET_CELL(buff, x, y, c, fg_, bg_, attrib_) \
  do {                                             \
    Cell& cell  = BUFF_CELL((buff), (x), (y));     \
    cell.ch     = (c);                             \
    cell.fg     = (fg_);                           \
    cell.bg     = (bg_);                           \
    cell.attrib = (attrib_);                       \
  } while (false)


class Document {

public:
  Document(std::shared_ptr<Buffer> buffer);

  // Actions.
  void CursorRight();
  void CursorLeft();
  void CursorUp();
  void CursorDown();
  void CursorHome();
  void CursorEnd();

  void SelectRight();
  void SelectLeft();
  void SelectUp();
  void SelectDown();
  void SelectHome();
  void SelectEnd();

  void InsertText(const std::string& text);
  void Backspace();
  void Undo();
  void Redo();

private:
  Cursor cursor;
  std::shared_ptr<Buffer> buffer;
  History history;

  // The path of the current document. If the file is a new buffer the path will
  // be empty and the editor will ask a path before save. If we started the editor
  // like `vim test.c` the path is `path/to/cwd/test.c` even thought the file
  // isn't exists in the disk we'll create a new one.
  std::string path;

  std::shared_ptr<LspClient> lsp_client;
  std::vector<Diagnostic> diagnostics;

  // TODO:
  // Encoding: utf8, utf16, etc.
  // Line Ending.
  // Indent style (\t, or ' ').
  // readonly;
  // language.
  //

  // DocPane need cursor and buffer to draw the document, this might not be the
  // "oop" way I don't know.
  friend class DocPane;
};



// BufferPane is the Pane that handles events and display the undeling buffer
// it's more of a text editor with number line and scroll bar etc.
class DocPane {

public:
  DocPane(std::shared_ptr<Document> document);

  void HandleEvent(const Event& event);
  void Draw(DrawBuffer buff, Coord pos, Size area);


private:
  // The document we're editing on this pane.
  std::shared_ptr<Document> document;

  // TODO: The bindings are not belong here (temp).
  KeyTree keytree;
  std::unordered_map<String, FuncAction> actions;

  // The coordinate where we start drawing the buffer from, this will change
  // after h-scroll and v-scroll.
  Coord view_start = {0, 0};

  // We "cache" the draw area for the text needed in some method.
  Size text_area = {0, 0};


private:
  void EnsureCursorOnView();

};


class Editor : public IEditor {

public:
  Editor();

  void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) override;
  bool Initialize() override;
  bool Running() override;
  void HandleEvents() override;
  void Draw() override;
  bool Cleanup() override;

private:
  DocPane docpane;
  bool running = true;
  std::unique_ptr<FrontEnd> frontend;
};

