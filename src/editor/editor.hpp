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


// An abstract interface for the Window class defined in the ui directory. A
// window is simply something that should be able to handle events, update each
// iteration and draw itself on the editor.
class IWindow {
public:
  virtual ~IWindow() = default;

  virtual bool HandleEvent(const Event& event) = 0;
  virtual void Update() = 0;
  virtual void Draw(FrameBuffer buff) = 0;

  // Methods to show in the info bar.
  virtual void Info(const std::string& error) = 0;
  virtual void Success(const std::string& error) = 0;
  virtual void Warning(const std::string& error) = 0;
  virtual void Error(const std::string& error) = 0;
};


// FIXME: Re-write this class and the interface.
class Editor : public IEditor {

public:
  Editor();
  ~Editor();

  static std::shared_ptr<Editor> Singleton();

  // Show the message at the info bar of the current window.
  static void Info(const std::string& msg);
  static void Success(const std::string& msg);
  static void Warning(const std::string& msg);
  static void Error(const std::string& msg);

  // Signale the editor to draw to the frame buffer since something is changed.
  static void ReDraw();

  // Since the resources are loaded at the start of the application and only
  // released at the very end, it's safe to use the raw pointers.
  static const Theme* GetCurrentTheme();

  int MainLoop() override;

  void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) override;
  void SetWindow(std::unique_ptr<IWindow> window);

  std::shared_ptr<Document> OpenDocument(const Path& path);
  std::shared_ptr<const Language> GetLanguage(const LanguageId& id) const;
  std::shared_ptr<LspClient> GetLspClient(const LspClientId& id) const;

private:

  static std::shared_ptr<Editor> singleton;

  std::unique_ptr<IWindow> window; // The ui root element.
  std::unique_ptr<FrontEnd> frontend;

  std::atomic<bool> redraw = true;
  std::atomic<bool> running = true;
  ThreadSafeQueue<Event> event_queue;

  // TODO: Note that the bellow maps are "global" registry where, and if it's
  // shared and modified between multiple threads, they needs to be locked and
  // unlocked. properly (at the moment It's not doing that).
  std::map<std::string, std::shared_ptr<const Theme>>   themes;
  std::map<LanguageId, std::shared_ptr<const Language>> languages;
  std::map<Path, std::shared_ptr<Document>>             documents;
  std::map<LspClientId, std::shared_ptr<LspClient>>     lsp_clients;

  friend class Global;

private:
  // Lsp listeners.
  void OnLspDiagnostics(const Path&, uint32_t version, std::vector<Diagnostic>&&);
  void OnLspCompletion(const Path&, bool is_incomplete, std::vector<CompletionItem>&&);
  void OnLspSignatureHelp(const Path&, SignatureItems&&);

  // Construct, configure, and register an lsp client.
  void RegisterLspClient(const LspConfig& config);

  // blocking loop that collect event (blocking) from the front end and push it
  // to our event queue. Run this asyncronusly.
  void EventLoop();

  void Draw(); // The draw call for each iteration.

};

