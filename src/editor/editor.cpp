//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "editor.hpp"

#include <thread>
#include <future>

// TODO: Added for open documet which is temproary at the moment.
#include <iostream>
#include <fstream>
#include <sstream>


std::unique_ptr<IEditor> IEditor::New() {
  return std::make_unique<Editor>();
}


uint8_t IEditor::RgbToXterm(uint32_t rgb) {
  return ::RgbToXterm(rgb);
}


uint32_t IEditor::XtermToRgb(uint8_t xterm) {
  return ::XtermToRgb(xterm);
}


Editor::Editor() {
  // Register LSP content listeners.
  LspClient::cb_diagnostics = [this](const Uri& uri, std::vector<Diagnostic>&& diagnostics) {
    this->OnLspDiagnostics(uri, std::move(diagnostics));
  };

}


Editor::~Editor() {
  LspClient::cb_diagnostics = nullptr;
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
  OpenDocument("/Users/thakeenathees/Desktop/thakee/temp/lsp/main.c");

  // Async run event loop.
  std::thread event_loop([this]() { EventLoop(); });

  // Main thread hot loop.
  while (running) {

    // Handle Events.
    while (!event_queue.Empty()) {
      docpane.HandleEvent(event_queue.Dequeue());
    }

    // FIXME: This needs to be re-factored and cleaned up.
    // Draw to the front end buffer.
    DrawBuffer buff = frontend->GetDrawBuffer();
    uint8_t color_bg = RgbToXterm(0x272829);
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


// FIXME: Revmove this mess from here.
std::string ReadAll(const std::string& path) {
  std::ifstream inputFile(path.data());
  assert(inputFile.is_open() && "Failed to open file.");
  std::stringstream buffer;
  buffer << inputFile.rdbuf();
  inputFile.close();
  return buffer.str();
}


// FIXME: Re implement this mess.
std::shared_ptr<Document> Editor::OpenDocument(const std::string& path) {

  Uri uri = std::string("file://") + path;
  std::string text = ReadAll(path);

  std::shared_ptr<Buffer> buff = std::make_shared<Buffer>(text);
  std::shared_ptr<Document> document = std::make_shared<Document>(uri, buff);

  LspConfig config;
  config.server = "clangd";

  documents[uri] = document;
  docpane.SetDocument(document);

  auto client = std::make_shared<LspClient>(config);
  lsp_clients["clangd"] = client;
  client->StartServer(std::nullopt);

  document->SetLspClient(client);

  return document;
}


void Editor::OnLspDiagnostics(const Uri& uri, std::vector<Diagnostic>&& diagnostics) {
  auto it = documents.find(uri);
  if (it == documents.end()) return;
  it->second->PushDiagnostics(std::move(diagnostics));
}
