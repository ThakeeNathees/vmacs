//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "lsp/lsp.hpp"
#include "document/document.hpp"


// BufferPane is the Pane that handles events and display the undeling buffer
// it's more of a text editor with number line and scroll bar etc.
class DocPane {

public:
  DocPane();

  void SetDocument(std::shared_ptr<Document> document);

  void HandleEvent(const Event& event);
  void Update();
  void Draw(FrameBuffer buff, Position pos, Size area);


private:
  // The document we're editing on this pane.
  std::shared_ptr<Document> document;

  // TODO: The bindings are not belong here (temp).
  KeyTree keytree;
  std::unordered_map<String, FuncAction> actions;

  // The coordinate where we start drawing the buffer from, this will change
  // after h-scroll and v-scroll.
  Position view_start = {0, 0};

  // We "cache" the draw area for the text needed in some method.
  Size text_area = {0, 0};

  // We set it to the time it was blinked last time and
  // if (time_now - last_blink > blink_period) we blink again.
  // if cursor_blink_period <= 0, that means we won't blink.
  int cursor_blink_period = 0;  // Time in milliseconds.
  int cursor_last_blink   = 0;
  bool cursor_blink_show  = true;

private:
  void ResetCursorBlink();
  void EnsureCursorOnView();

  void DrawBuffer(FrameBuffer buff, Position pos, Size area);
  void DrawAutoCompletions(FrameBuffer buff, Position docpos); // FIXME: split and draw primitive.
};


class Editor : public IEditor {

public:
  Editor();
  ~Editor();

  static std::shared_ptr<Editor> Singleton();

  void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) override;
  int MainLoop() override;

private:

  static std::shared_ptr<Editor> singleton;

  DocPane docpane;
  std::unique_ptr<FrontEnd> frontend;

  std::atomic<bool> running = true;
  ThreadSafeQueue<Event> event_queue;

  // TODO:
  // Note that the bellow maps are "global" registry where, and if it's shared
  // and modified between multiple threads, they needs to be locked and unlocked.
  // properly (at the moment It's not doing that).
  std::map<std::string, std::shared_ptr<const Theme>>   themes;
  std::map<LanguageId, std::shared_ptr<const Language>> languages;
  std::map<Uri, std::shared_ptr<Document>>              documents;
  std::map<LspClientId, std::shared_ptr<LspClient>>     lsp_clients;

  friend class Global;

private:
  // Lsp listeners.
  void OnLspDiagnostics(const Uri&, uint32_t version, std::vector<Diagnostic>&&);
  void OnLspCompletion(const Uri&, bool is_incomplete, std::vector<CompletionItem>&&);
  void OnLspSignatureHelp(const Uri&, SignatureItems&&);

  // Construct, configure, and register an lsp client.
  void RegisterLspClient(const LspConfig& config);

  // TODO: These are subjected to change.
  std::shared_ptr<Document> OpenDocument(const std::string& path);

  // blocking loop that collect event (blocking) from the front end and push it
  // to our event queue. Run this asyncronusly.
  void EventLoop();

};

