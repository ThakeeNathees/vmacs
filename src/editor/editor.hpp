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


// Note that since Pane is a subtype of event handler. And HandleEvent() is
// already defined.
class Pane : public EventHandler {

public:
  Pane(const KeyTree* keytree); // The static key tree registry of the child class.
  virtual ~Pane() = default;

  // The handler should return true if the event is consumed by the pane.
  virtual void Update() = 0;
  virtual void Draw(FrameBuffer buff, Position pos, Size area) = 0;
};


// FIXME: Re-write this class and the interface.
class Editor : public IEditor {

public:
  Editor();
  ~Editor();

  static std::shared_ptr<Editor> Singleton();

  int MainLoop() override;

  void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) override;
  void SetRootPane(std::unique_ptr<Pane> root_pane);

  std::shared_ptr<Document> OpenDocument(const Path& path);
  std::shared_ptr<const Language> GetLanguage(const LanguageId& id) const;
  std::shared_ptr<LspClient> GetLspClient(const LspClientId& id) const;

private:

  static std::shared_ptr<Editor> singleton;

  std::unique_ptr<Pane> root_pane;

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

