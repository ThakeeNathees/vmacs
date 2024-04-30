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


class Pane {

public:
  virtual void HandleEvent(const Event& event) = 0; 
  virtual void Update() = 0;
  virtual void Draw(FrameBuffer buff, Position pos, Size area) = 0;

  virtual ~Pane() = default;
};


// BufferPane is the Pane that handles events and display the undeling buffer
// it's more of a text editor with number line and scroll bar etc.
class DocPane : public Pane {

public:
  DocPane();

  void SetDocument(std::shared_ptr<Document> document);

  void HandleEvent(const Event& event) override;
  void Update() override;
  void Draw(FrameBuffer buff, Position pos, Size area) override;


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
  void DrawAutoCompletions(FrameBuffer buff, Position docpos);

  // Check the given index is within selection or inside the cursor and sets the pointers.
  // This is needed to set the background color of a cell and re-usable.
  void CheckCellStatus(int index, bool* in_cursor, bool* in_selection);
};


class FindPane : public Pane {

public:
  FindPane();

  void HandleEvent(const Event& event) override;
  void Update() override;
  void Draw(FrameBuffer buff, Position pos, Size area) override;

private:
  std::unique_ptr<IPC> ipc;
  std::mutex mutex_results;
  std::vector<std::string> results;

  std::unique_ptr<IPC> ipc_filter;
  std::mutex mutex_filters;
  std::vector<std::string> filters;

  // The cursor inside the search bar.
  int cursor_index = 0;
  std::string input_text; // The text that was inputted.

private:
  static void StdoutCallbackResults(void* data, const char* buff, size_t length);
  static void StdoutCallbackFilter(void* data, const char* buff, size_t length);

  void TriggerFuzzyFilter();

  // Note that these methods are only called from the Draw function and before
  // calling it, the required mutexes needs to be locked. Since these functions
  // Don't call the mutexes. (Designed this way so I don't lock on the same mutex
  // twise or Don't want to borrow unique_lock and make the code unreadable).
  void DrawResults(FrameBuffer buff, int x, int y, int w, int h);
  void DrawFilter(FrameBuffer buff, int x, int y, int w, int h);
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

  // FIXME(mess):
  DocPane docpane;
  FindPane findpane;

  std::unique_ptr<FrontEnd> frontend;

  std::atomic<bool> redraw = true;
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

  // FIXME: remove this mess.
  std::shared_ptr<Document> OpenDocument(const std::string& path);

  // blocking loop that collect event (blocking) from the front end and push it
  // to our event queue. Run this asyncronusly.
  void EventLoop();

  void Draw(); // The draw call for each iteration.

};

