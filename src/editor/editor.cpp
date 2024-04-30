//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "platform//platform.hpp"
#include "pane/finder.hpp"
#include "editor.hpp"

#include <chrono>
#include <thread>
#include <future>


// FIXME: Maybe create another file for pane (pane.cpp).

void Pane::RegisterAction(const std::string& action_name, FuncAction action) {
  actions[action_name] = action;
}


void Pane::RegisterBinding(const std::string& mode, const std::string& key_combination, const std::string& action_name) {
  std::vector<event_t> events;

  if (!ParseKeyBindingString(events, key_combination.c_str())) {
    // TODO: Report error to user.
    return;
  }

  auto it = actions.find(action_name);
  if (it == actions.end()) {
    // TODO: Error(""); // Report to user.
    return;
  }

  keytree.RegisterBinding(mode, events, it->second);
  events.clear();
}


void Pane::SetMode(const std::string& mode) {
  keytree.SetMode(mode);
}


bool Pane::TryEvent(const Event& event) {
  bool more = false;

  FuncAction action = keytree.ConsumeEvent(EncodeKeyEvent(event.key), &more);
  // if (action && more) { } // TODO: Timeout and perform action.

  if (action) {
    action();
    keytree.ResetCursor();
    return true;
  }

  // Don't do anything, just wait for the next keystroke and perform on it.
  if (more) return true;

  // Sequence is not registered, reset and listen from start. We'll be sending
  // true and treat this event to reset the cursor.
  if (!keytree.IsCursorRoot()) {
    keytree.ResetCursor();
    return true;
  }

  return false;
}


// Static singleton definition goes here.
std::shared_ptr<Editor> Editor::singleton = nullptr;


std::shared_ptr<IEditor> IEditor::Singleton() {
  return std::static_pointer_cast<IEditor>(Editor::Singleton());
}


std::shared_ptr<Editor> Editor::Singleton() {
  if (singleton == nullptr) {
    singleton = std::make_shared<Editor>();
  }
  return singleton;
}


void Global::ReDraw() {
  Editor::Singleton()->redraw = true;
}


// FIXME: This is temproary. Set current theme and get from there.
const Theme* Global::GetCurrentTheme() {
  return Editor::Singleton()->themes["dracula_at_night"].get();
}


// FIXME: This is not how we do it.
Editor::Editor() : findpane(std::make_unique<FilesFinder>()) {

  // Load the themes.
  std::map<std::string, Json> theme_data = Platform::LoadThemes();
  for (auto& it : theme_data) {
    themes[it.first] = std::make_shared<Theme>(it.second);
  }

  // Load languages.
  for (const LanguageLoadResult& result : Platform::LoadLanguages()) {
    std::shared_ptr<Language> lang = std::make_shared<Language>();
    if (result.query_highlight != NULL) {
      if (result.tree_sitter_loader) lang->data = result.tree_sitter_loader();
      uint32_t error_offset; TSQueryError err; // TODO: check for errors.
      TSQuery* query = ts_query_new(
          lang->data, // Note that the loader should be called above.
          result.query_highlight,
          strlen(result.query_highlight),
          &error_offset, &err);
      lang->query_highlight = query;
    }
    languages[result.language_id] = lang;
  }

  // Register LSP clients. TODO(grep): load the config from RESLOAD.
  LspConfig config;
  config.id = "clangd";
  config.server_file = "clangd";
  RegisterLspClient(config);
}


Editor::~Editor() { }


void Editor::SetFrontEnd(std::unique_ptr<FrontEnd> frontend) {
  this->frontend = std::move(frontend);
}


uint8_t IEditor::RgbToXterm(uint32_t rgb) {
  return ::RgbToXterm(rgb);
}


uint32_t IEditor::XtermToRgb(uint8_t xterm) {
  return ::XtermToRgb(xterm);
}


// TODO: Check the code quality of this method.
int Editor::MainLoop() {

  ASSERT(frontend != nullptr, "No frontend is available. Did you forgot to "
                              "call IEditor::SetFrontEnd() ?");

  if (!frontend->Initialize()) {
    fprintf(stdout, "Editor initialize failed.\n");
    return 1;
  }

  // What a mess.
  OpenDocument("/Users/thakeenathees/Desktop/thakee/repos/vmacs/build/main.cpp");

  // Async run event loop.
  std::thread event_loop([this]() { EventLoop(); });

  // We'll use this to limit the frame rate and sleep for other threads to run
  // while we sleep.
  int time_ms = GetElapsedTime();

  // 30 FPS : 33 milliseconds
  // 60 FPS : 16 milliseconds
  const int WAIT_TIME_MS = 33;

  // Main thread hot loop.
  while (running) {

    // Handle Events.
    while (!event_queue.Empty()) {
      Event e = event_queue.Dequeue();
      docpane.HandleEvent(e);
      findpane.HandleEvent(e);
      redraw = true;
    }

    // Update call.
    docpane.Update();
    findpane.Update();

    // FIXME: Because of raylib we can't do this. The fron end should own the main
    // loop and use the eidtor as a instance to run at each iteration.
    //
    // Draw call.
    // if (redraw)
      Draw();
    redraw = false;

    // Wait till we reach the frame rate limit.
    int now = GetElapsedTime();
    int delta = now - time_ms;
    if (delta < WAIT_TIME_MS) {
      time_ms = now;
      std::this_thread::sleep_for(std::chrono::milliseconds(delta));
    }

  }

  frontend->Cleanup();

  global_thread_stop = true;
  event_loop.join();
  return 0;
}


void Editor::EventLoop() {
  while (running.load() && !global_thread_stop.load()) {
    const std::vector<Event>& events = frontend->GetEvents();
    for (const Event& event : events) {
      // FIXME: Handle the CLOSE event properly.
      if (event.type == Event::Type::CLOSE) {
        running = false;
      }
      event_queue.Enqueue(event);
    }
  }
}


void Editor::Draw() {
  // FIXME: This needs to be re-factored and cleaned up.
  // Draw to the front end buffer.
  FrameBuffer buff = frontend->GetDrawBuffer();

  // uint8_t color_bg = 0x272829;
  Color color_bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg=0, .bg=0xffffff, .attrib=0}).bg;

  // Clean the buffer.
  for (int i    = 0; i < buff.width*buff.height; i++) {
    Cell& cell  = buff.cells[i];
    cell.ch     = ' ';
    cell.fg     = 0;
    cell.bg     = color_bg;
    cell.attrib = 0;
  }

  Position pos = {0, 0};
  Size size = {buff.width, buff.height};
  docpane.Draw(buff, pos, size);
  findpane.Draw(buff, pos, size);

  frontend->Display(color_bg); // FIXME: background color for raylib.

  // Draw a spinning indicator yell it's re-drawn.
  // ⡿ ⣟ ⣯ ⣷ ⣾ ⣽ ⣻ ⢿
  static int curr = 0;
  int icons[] = { 0x287f, 0x28df, 0x28ef, 0x28f7, 0x28fe, 0x28fd, 0x28fb, 0x28bf };
  int icon_count = sizeof icons / sizeof *icons;
  if (curr >= icon_count) curr = 0;
  SET_CELL(buff, 0, buff.height-1, icons[curr++], 0xffffff, 0, 0); // FIXME:

}


// FIXME: Re implement this mess.
std::shared_ptr<Document> Editor::OpenDocument(const std::string& path) {

  Uri uri = std::string("file://") + path;
  std::string text;
  ASSERT(Platform::ReadFile(&text, path), "My ugly code");

  std::shared_ptr<Buffer> buff = std::make_shared<Buffer>(text);
  std::shared_ptr<Document> document = std::make_shared<Document>(uri, buff);
  documents[uri] = document;

  document->SetLanguage(languages["cpp"]);
  docpane.SetDocument(document);

  document->SetLspClient(lsp_clients["clangd"]);

  return document;
}


void Editor::RegisterLspClient(const LspConfig& config) {
  std::shared_ptr<LspClient> client = std::make_shared<LspClient>(config);

  // Register callbacks.
  client->cb_diagnostics = [this](const Uri& uri, uint32_t version, std::vector<Diagnostic>&& diagnostics) {
    this->OnLspDiagnostics(uri, version, std::move(diagnostics));
  };
  client->cb_completion = [this](const Uri& uri, bool is_incomplete, std::vector<CompletionItem>&& items) {
    this->OnLspCompletion(uri, is_incomplete, std::move(items));
  };
  client->cb_signature_help = [this](const Uri& uri, SignatureItems&& items) {
    this->OnLspSignatureHelp(uri, std::move(items));
  };

  // Register the client.
  lsp_clients[config.id] = client;

  // FIXME(grep): We shouldn't start all the servers at the start. Move this to
  // somewhere else.
  client->StartServer(std::nullopt);
}


void Editor::OnLspDiagnostics(const Uri& uri, uint32_t version, std::vector<Diagnostic>&& diagnostics) {
  auto it = documents.find(uri);
  if (it == documents.end()) return;
  it->second->PushDiagnostics(version, std::move(diagnostics));
}


void Editor::OnLspCompletion(const Uri& uri, bool is_incomplete, std::vector<CompletionItem>&& items) {
  auto it = documents.find(uri);
  if (it == documents.end()) return;
  it->second->PushCompletions(is_incomplete, std::move(items));
}


void Editor::OnLspSignatureHelp(const Uri& uri, SignatureItems&& items) {
  auto it = documents.find(uri);
  if (it == documents.end()) return;
  it->second->PushSignatureHelp(std::move(items));
}
