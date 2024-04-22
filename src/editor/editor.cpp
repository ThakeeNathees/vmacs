//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "editor.hpp"


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
  LspClient::cb_diagnostics = [&](const Uri& uri, std::vector<Diagnostic>&& diagnostics) {
    this->OnLspDiagnostics(uri, std::move(diagnostics));
  };

}


Editor::~Editor() {
  LspClient::cb_diagnostics = nullptr;
}


void Editor::SetFrontEnd(std::unique_ptr<FrontEnd> frontend) {
  this->frontend = std::move(frontend);
}


bool Editor::Running() {
  return running;
}


bool Editor::Initialize() {
  return frontend->Initialize();
}


bool Editor::Cleanup() {
  return frontend->Cleanup();
}


void Editor::HandleEvents() {

  ASSERT(frontend != nullptr, "No frontend is available. Did you forgot to "
                              "call IEditor::SetFrontEnd() ?");
  const std::vector<Event>& events = frontend->GetEvents();

  for (const Event& event : events) {
    if (event.type == Event::Type::CLOSE) running = false;
    docpane.HandleEvent(event);
  }
}


void Editor::Draw() {

  // FIXME: This needs to be re-factored and cleaned up.
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
  frontend->Display(color_bg); // FIXME: background color.
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

  std::string text = ReadAll(path);
  std::shared_ptr<Buffer> buff = std::make_shared<Buffer>(text);
  std::shared_ptr<Document> document = std::make_shared<Document>(buff);
  Uri uri = std::string("file://") + path;

  LspConfig config;
  config.server = "clangd";

  documents[uri] = document;
  docpane.SetDocument(document);

  auto client = std::make_shared<LspClient>(config);
  lsp_clients["clangd"] = client;
  client->StartServer(std::nullopt);
  client->DidOpen(uri, text, "c");

  return document;
}


void Editor::OnLspDiagnostics(const Uri& uri, std::vector<Diagnostic>&& diagnostics) {
  auto it = documents.find(uri);
  if (it == documents.end()) return;
  it->second->PushDiagnostics(std::move(diagnostics));
}
