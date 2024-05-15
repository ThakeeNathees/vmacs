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


// FIXME: Re-write this class and the interface.
class Editor {

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

  // Returns the current theme. Since the resources are loaded at the start of
  // the application and only released at the very end, it's safe to use the raw
  // pointers.
  static const Theme& GetTheme();
  static const Icons& GetIcons();
  static Config& GetConfig();

  // Returns the frame buffer with the given area and clears the frame with the
  // background color.
  static FrameBuffer NewFrameBuffer(Area area);

  int MainLoop();

  void SetFrontEnd(std::unique_ptr<IFrontEnd> frontend);

  Area GetDrawArea() const; // Returns the size of the frame buffer.
  void SetUi(std::unique_ptr<IUi> window);
  IUi* GetUi();

  void RegisterTheme(const std::string& theme_name, std::shared_ptr<Theme> theme);
  void RegisterLanguage(LanguageId id, std::shared_ptr<Language> lang);
  void RegisterLspClient(const LspConfig& config);

  // Returns true on success, otherwise will set the error message to editor
  // info bar and return false.
  bool SetTheme(const std::string theme_name);

  std::shared_ptr<Document> OpenDocument(const Path& path);
  std::shared_ptr<const Language> GetLanguage(const LanguageId& id) const;
  std::shared_ptr<LspClient> GetLspClient(const LspClientId& id) const;

private:

  static std::shared_ptr<Editor> singleton;

  std::unique_ptr<IUi> ui; // The ui root element.
  std::unique_ptr<IFrontEnd> frontend;

  std::atomic<bool> redraw = true;
  std::atomic<bool> running = true;

  ThreadSafeQueue<Event> event_queue;
  FrameBuffer buff; // The frame we'll be drawing on.

  // TODO: Note that the bellow maps are "global" registry where, and if it's
  // shared and modified between multiple threads, they needs to be locked and
  // unlocked. properly (at the moment It's not doing that).
  std::map<std::string, std::shared_ptr<const Theme>>   themes;
  std::map<LanguageId, std::shared_ptr<const Language>> languages;
  std::map<Path, std::shared_ptr<Document>>             documents;
  std::map<LspClientId, std::shared_ptr<LspClient>>     lsp_clients;

  // Themes are stored in the above themes registry and we have a raw pointer
  // of the theme here. This will be initialized with a non nullptr value when
  // the editor is initialized.
  const Theme* theme = nullptr;
  Icons icons;

  // The editor's configuration.
  Config config;

private:
  // Lsp listeners.
  void OnLspDiagnostics(const Path&, uint32_t version, std::vector<Diagnostic>&&);
  void OnLspCompletion(const Path&, bool is_incomplete, std::vector<CompletionItem>&&);
  void OnLspSignatureHelp(const Path&, SignatureItems&&);

  // This will re-size the framebuffer if it's not up to date with the front end's
  // draw area size and clear the frame with the background color.
  void PrepareFrameBuffer();

  // blocking loop that collect event (blocking) from the front end and push it
  // to our event queue. Run this asyncronusly.
  void EventLoop();
};

