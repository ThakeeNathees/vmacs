//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/core.hpp"
#include "lsp/client.hpp"
#include "document/document.hpp"

#include "pane/pane.hpp"


class Editor : public IEditor {

public:
  Editor();
  ~Editor();

  static std::shared_ptr<Editor> Singleton();

  void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) override;
  int MainLoop() override;

private:

  static std::shared_ptr<Editor> singleton;

  // FIXME(mess):
  // DocPane docpane;
  // FindPane findpane;
  RootPane root;

  std::unique_ptr<FrontEnd> frontend;

  std::atomic<bool> redraw = true;
  std::atomic<bool> running = true;
  ThreadSafeQueue<Event> event_queue;

  // TODO: Note that the bellow maps are "global" registry where, and if it's
  // shared and modified between multiple threads, they needs to be locked and
  // unlocked. properly (at the moment It's not doing that).
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

  // blocking loop that collect event (blocking) from the front end and push it
  // to our event queue. Run this asyncronusly.
  void EventLoop();

  void Draw(); // The draw call for each iteration.

};

