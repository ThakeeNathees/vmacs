//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core/core.hpp"
#include "platform/platform.hpp"
#include "editor.hpp"

#include <chrono>
#include <thread>
#include <future>


// Static singleton definition goes here.
std::shared_ptr<Editor> Editor::singleton = nullptr;


Config& GetConfig() {
  return Editor::GetConfig();
}


std::shared_ptr<Editor> Editor::Singleton() {
  if (singleton == nullptr) {
    singleton = std::make_shared<Editor>();
  }
  return singleton;
}


void Editor::Info(const std::string& msg) {
  // IUi* ui = Singleton()->ui.get();
  // ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  // ui->Info(msg);
}


void Editor::Success(const std::string& msg) {
  // IUi* ui = Singleton()->ui.get();
  // ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  // ui->Success(msg);
}


void Editor::Warning(const std::string& msg) {
  // IUi* ui = Singleton()->ui.get();
  // ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  // ui->Warning(msg);
}


void Editor::Error(const std::string& error) {
  // IUi* ui = Singleton()->ui.get();
  // ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  // ui->Error(error);
}


void Editor::ReDraw() {
  Singleton()->redraw = true;
}


const Theme& Editor::GetTheme() {
  const Theme* theme = Singleton()->theme;
  ASSERT(theme != nullptr, OOPS);
  return *theme;
}


const Icons& Editor::GetIcons() {
  return Singleton()->icons;
}


Config& Editor::GetConfig() {
  return Singleton()->config;
}


Editor::Editor() {
}


Editor::~Editor() {
}


bool Editor::SetTheme(const std::string theme_name) {
  auto it = themes.find(theme_name);
  if (it == themes.end()) {
    std::string errmsg = std::string("Theme not found (name='") + theme_name + "')";
    Editor::Error(errmsg);
    return false;
  }
  theme = it->second.get();
  return true;
}


void Editor::RegisterTheme(const std::string& theme_name, std::shared_ptr<Theme> theme) {
  themes[theme_name] = std::move(theme);
}


void Editor::RegisterLanguage(LanguageId id, std::shared_ptr<Language> lang) {
  languages[id] = std::move(lang);
}


void Editor::SetFrontEnd(std::unique_ptr<IFrontEnd> frontend) {
  this->frontend = std::move(frontend);
}


void Editor::SetUi(std::unique_ptr<IUi> ui) {
  this->ui = std::move(ui);
}


IUi* Editor::GetUi() {
  return ui.get();
}


// TODO: Check the code quality of this method.
int Editor::MainLoop() {

  if (theme == nullptr && !SetTheme(config.theme)) {
    // TODO: Set a default theme which is "hardcoded" in the editor.
  }

  // Validate the requirnments.
  ASSERT(frontend != nullptr, "No frontend is available. Did you forgot to call editor.SetFrontEnd() ?");
  ASSERT(ui != nullptr, "ui is nullptr. Did you forget to set one?");
  ASSERT(theme != nullptr, OOPS);

  // FIXME: This should be handled properly via Platform interface (we should report this to platform).
  if (!frontend->Initialize()) {
    fprintf(stdout, "Editor initialize failed.\n");
    return 1;
  }

  // Async run event loop.
  std::thread event_loop([this]() { EventLoop(); });

  // Frame time is 1/fps. If the current frame takes less than frame time, we'll
  // wait for the delta before the next frame iteration.
  const int frame_time_ms = 1000 / config.fps;

  // This is the elaplsed time of the last frame. We'll use this to limit the
  // frame rate and sleep for other threads to run while we sleep.
  int last_frame_time_ms = GetElapsedTime();

  // Main thread hot loop.
  while (running) {

    // Handle Events.
    while (!event_queue.Empty()) {
      Event event = event_queue.Dequeue();
      // FIXME: Handle the CLOSE event properly ask user to save progress etc.
      if (event.type == Event::Type::CLOSE) {
        running = false;
      }
      ui->HandleEvent(event);
      redraw = true;
    }

    // Update call.
    ui->Update();

    // Draw call.
    if (redraw) {
      PrepareFrameBuffer();

      ui->Draw(buff);
      frontend->Display(buff);

      redraw = false;
    }

    // Wait till we reach the frame rate limit.
    int now = GetElapsedTime();
    int delta = now - last_frame_time_ms;
    if (delta < frame_time_ms) {
      last_frame_time_ms = now;
      std::this_thread::sleep_for(std::chrono::milliseconds(delta));
    }

  }

  frontend->Cleanup();

  event_loop.join();
  return 0;
}


void Editor::EventLoop() {
  while (running) {
    const std::vector<Event>& events = frontend->GetEvents();
    for (const Event& event : events) {
      event_queue.Enqueue(event);
    }
  }
}


void Editor::PrepareFrameBuffer() {
  Area area = frontend->GetDrawArea();

  // Resize the buffer.
  if (buff.cells.size() != area.width * area.height) {
    buff.cells.resize(area.width * area.height);
  }
  buff.width  = area.width;
  buff.height = area.height;

  // Clear the background.
  Style style_bg = Editor::GetTheme().GetStyle("ui.background"); // FIXME:
  DrawRectangleFill(buff, {0,0}, {buff.width, buff.height}, style_bg);
}


// TODO: This will return nullptr on error reading the file handle everywhere
// this is called.
std::shared_ptr<Document> Editor::OpenDocument(const Path& path) {

  // If the document is already opened, just return it.
  auto it = documents.find(path);
  if (it != documents.end()) return it->second;

  std::string text;
  if (!Platform::ReadFile(&text, path)) {
    Editor::Error("Error opening file at: " + path.String());
    return nullptr;
  }

  std::shared_ptr<Buffer> buff = std::make_shared<Buffer>(text);
  std::shared_ptr<Document> document = std::make_shared<Document>(path, buff);
  document->SetThemeGetter([](){ return &Editor::GetTheme(); });

  documents[path] = document;

  // Set language (for syntax highlighting).
  LanguageId language_id = config.GetLangIdFromFileName(path.FileName());
  if (!language_id.empty()) {
    std::shared_ptr<const Language> lang = GetLanguage(language_id);
    if (lang) document->SetLanguage(lang);
  }

  // Set LSP client.
  LspClientId client_id = config.GetLspClientIdFromFileName(path.FileName());
  if (!client_id.empty()) {
    std::shared_ptr<LspClient> client = GetLspClient(client_id);
    if (client) {
      client->StartServer(); // This will start the server only if not already.
      document->SetLspClient(client);
    }
  }

  return document;
}


std::shared_ptr<const Language> Editor::GetLanguage(const LanguageId& id) const {
  auto it = languages.find(id);
  if (it == languages.end()) return nullptr;
  return it->second;
}


std::shared_ptr<LspClient> Editor::GetLspClient(const LspClientId& id) const {
  auto it = lsp_clients.find(id);
  if (it == lsp_clients.end()) return nullptr;
  return it->second;
}


void Editor::RegisterLspClient(const LspConfig& config) {
  std::shared_ptr<LspClient> client = std::make_shared<LspClient>(config);

  // Register callbacks.
  client->cb_diagnostics = [this](const Path& path, uint32_t version, std::vector<Diagnostic>&& diagnostics) {
    this->OnLspDiagnostics(path, version, std::move(diagnostics));
  };
  client->cb_completion = [this](const Path& path, bool is_incomplete, std::vector<CompletionItem>&& items) {
    this->OnLspCompletion(path, is_incomplete, std::move(items));
  };
  client->cb_signature_help = [this](const Path& path, SignatureItems&& items) {
    this->OnLspSignatureHelp(path, std::move(items));
  };

  // Register the client.
  lsp_clients[config.id] = client;
}


void Editor::OnLspDiagnostics(const Path& path, uint32_t version, std::vector<Diagnostic>&& diagnostics) {
  auto it = documents.find(path);
  if (it == documents.end()) return;
  it->second->PushDiagnostics(version, std::move(diagnostics));
}


void Editor::OnLspCompletion(const Path& path, bool is_incomplete, std::vector<CompletionItem>&& items) {
  auto it = documents.find(path);
  if (it == documents.end()) return;
  it->second->PushCompletions(is_incomplete, std::move(items));
}


void Editor::OnLspSignatureHelp(const Path& path, SignatureItems&& items) {
  auto it = documents.find(path);
  if (it == documents.end()) return;
  it->second->PushSignatureHelp(std::move(items));
}
