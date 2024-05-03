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

  // This will internally call _Draw(), and the subclasses should override it.
  void Draw(FrameBuffer buff, Position pos, Size area);
  bool HandleEvent(const Event& event) final override;

private:
  Position pos = {0};
  Size area    = {0};

  virtual void _Draw(FrameBuffer buff, Position pos, Size area) = 0;
  virtual bool _HandleEvent(const Event& event) = 0;
};


// Split is a tree of child splits and the leaf node contains a pane.
// If the split type is Vertical all of it's children would be horizontal splits
// or leaf nodes and vice versa.
//
// Note that the split is just the view of the panes in the 'tab.panes' array.
class Split {

public:
  enum class Type {
    LEAF       = 0,
    VERTICAL   = 1,
    HORIZONTAL = 2,
  };

private:
  Type type = Type::LEAF;

  // TODO: There is no size (w, h) for the split and will take equally at the
  // moment, fix it.

  Split* parent = nullptr; // If parent is nullptr, its the root of the tab.
  std::vector<std::unique_ptr<Split>> children;

  // A leaf node will have 0 children and the pane* will be available. However
  // it could be nullptr if the pane is not initialized yet.
  Pane* pane = nullptr;

  friend class Tab;
};


class Tab : public EventHandler {
public:
  Tab();

  bool HandleEvent(const Event& event) override;
  void Update();
  void Draw(FrameBuffer buff, Position pos, Size area);

  // "Constructors".
  static std::unique_ptr<Tab> FromPane(std::unique_ptr<Pane> pane);

  // Key tree is public so we can register action and bind to keys outside. I
  // don't like the OOP getters and setters (what's the point)?
  static KeyTree keytree;

private:
  Split root;

  // TODO: Don't know how to handle popup, for now it's exists here.
  std::unique_ptr<Pane> popup;

  // The panes in the splits, where each split will have a weak reference to the
  // pane in this list.
  std::vector<std::unique_ptr<Pane>> panes;

  // Currently active pane in the split tree. If the popup is not nullptr, we'll
  // prioratize the popup to handle the events and ignore the active pane.
  Pane* active = nullptr;

private:
  void DrawSplit(FrameBuffer buff, Split* split, Position pos, Size area);

public: // Actions.
  static bool Action_ClosePopup(Tab* self);
  static bool Action_PopupFilesFinder(Tab* self);
};


// FIXME: Re-write this class and the interface.
class Editor : public IEditor {

public:
  Editor();
  ~Editor();

  static std::shared_ptr<Editor> Singleton();

  int MainLoop() override;

  void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) override;
  void SetTab(std::unique_ptr<Tab> tab);

  std::shared_ptr<Document> OpenDocument(const Path& path);
  std::shared_ptr<const Language> GetLanguage(const LanguageId& id) const;
  std::shared_ptr<LspClient> GetLspClient(const LspClientId& id) const;

private:

  static std::shared_ptr<Editor> singleton;

  // ---------------------------------------------------------------------------
  // TODO: Implement window and that contain a lsit of tabs.
  std::unique_ptr<Tab> tab;
  std::string message; // FIXME: a message to show at the bottom info bar.
  public:
  void SetMessage(const std::string& message) { this->message = message; }
  // void ClosePopup() { tab->ClosePopup();  }
  private:
  // ---------------------------------------------------------------------------

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

