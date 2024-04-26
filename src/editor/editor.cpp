//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "editor.hpp"

#include <chrono>
#include <thread>
#include <future>


// Static singleton definition goes here.
std::shared_ptr<Editor> Editor::singleton = nullptr;


std::shared_ptr<IEditor> IEditor::Singleton() {
  return std::make_shared<Editor>();
}

// FIXME: move this to somewhere else.
extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_javascript(void);

Editor::Editor() {


  // FIXME: Implement a proper language loading system with support to load from
  // dynamic linked library.
  std::shared_ptr<Language> lang_c = std::make_shared<Language>();
  lang_c->id = "c";
  lang_c->data = tree_sitter_c();

  uint32_t error_offset;
  TSQueryError err;
  const char* source =
#include "../../res/c.scm.hpp"
    ;
  TSQuery* query = ts_query_new(
    lang_c->data,
    source,
    strlen(source),
    &error_offset,
    &err
  );
  lang_c->query_highlight = query;
  languages["c"] = lang_c;

  // Fixme: This is temproary.
  Theme::Load();
}


Editor::~Editor() {
}


void Editor::SetFrontEnd(std::unique_ptr<FrontEnd> frontend) {
  this->frontend = std::move(frontend);
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
  OpenDocument("/Users/thakeenathees/Desktop/thakee/repos/vmacs/build/main.c");

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
      docpane.HandleEvent(event_queue.Dequeue());
    }

    // Update call.
    docpane.Update();

    // FIXME: This needs to be re-factored and cleaned up.
    // Draw to the front end buffer.
    DrawBuffer buff = frontend->GetDrawBuffer();

    // uint8_t color_bg = 0x272829;
    Color color_bg = Theme::Get()->entries["ui.background"].bg;

    // Clean the buffer.
    for (int i    = 0; i < buff.width*buff.height; i++) {
      Cell& cell  = buff.cells[i];
      cell.ch     = ' ';
      cell.fg     = 0;
      cell.bg     = color_bg;
      cell.attrib = 0;
    }
    docpane.Draw(buff, {0,0}, {buff.width, buff.height});
    frontend->Display(color_bg); // FIXME: background color for raylib.

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


uint8_t IEditor::RgbToXterm(uint32_t rgb) {
  return ::RgbToXterm(rgb);
}


uint32_t IEditor::XtermToRgb(uint8_t xterm) {
  return ::XtermToRgb(xterm);
}


// FIXME: Re implement this mess.
std::shared_ptr<Document> Editor::OpenDocument(const std::string& path) {

  Uri uri = std::string("file://") + path;
  std::string text = ReadAll(path);

  std::shared_ptr<Buffer> buff = std::make_shared<Buffer>(text);
  std::shared_ptr<Document> document = std::make_shared<Document>(uri, buff);

  document->SetLanguage(languages["c"]);

  LspConfig config;
  config.server = "clangd";

  documents[uri] = document;
  docpane.SetDocument(document);

  // FIXME: the document itself request for the client from the editor.
  auto client = std::make_shared<LspClient>(config);
  client->cb_diagnostics = [this](const Uri& uri, uint32_t version, std::vector<Diagnostic>&& diagnostics) {
    this->OnLspDiagnostics(uri, version, std::move(diagnostics));
  };
  client->cb_completion = [this](const Uri& uri, bool is_incomplete, std::vector<CompletionItem>&& items) {
    this->OnLspCompletion(uri, is_incomplete, std::move(items));
  };
  client->cb_signature_help = [this](const Uri& uri, SignatureItems&& items) {
    this->OnLspSignatureHelp(uri, std::move(items));
  };
  lsp_clients["clangd"] = client;
  client->StartServer(std::nullopt);
  document->SetLspClient(client);

  return document;
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
