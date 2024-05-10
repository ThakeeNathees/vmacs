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


std::shared_ptr<IEditor> IEditor::Singleton() {
  return std::static_pointer_cast<IEditor>(Editor::Singleton());
}


std::shared_ptr<Editor> Editor::Singleton() {
  if (singleton == nullptr) {
    singleton = std::make_shared<Editor>();
  }
  return singleton;
}


void Editor::Info(const std::string& msg) {
  IUi* ui = Singleton()->ui.get();
  ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  ui->Info(msg);
}


void Editor::Success(const std::string& msg) {
  IUi* ui = Singleton()->ui.get();
  ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  ui->Success(msg);
}


void Editor::Warning(const std::string& msg) {
  IUi* ui = Singleton()->ui.get();
  ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  ui->Warning(msg);
}


void Editor::Error(const std::string& error) {
  IUi* ui = Singleton()->ui.get();
  ASSERT(ui != nullptr, "Editor::singleton.ui was nullptr, did you forget to initialize one?");
  ui->Error(error);
}


void Editor::ReDraw() {
  Singleton()->redraw = true;
}


const Theme* Editor::GetTheme() {
  const Theme* theme = Singleton()->theme;
  ASSERT(theme != nullptr, OOPS);
  return theme;
}


const Icons* Editor::GetIcons() {
  return &Singleton()->icons;
}


// FIXME: This is not how we do it.
Editor::Editor() {

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

  // FIXME(mess, config): load current from config and use a default theme if not found.
  SetTheme("varua");
  ASSERT(theme != nullptr, OOPS);

  // FIXME(mess,registry): Register LSP clients. TODO(grep): load the config from RESLOAD.
  LspConfig config;
  config.id = "clangd";
  config.server_file = "clangd";
  RegisterLspClient(config);
}


Editor::~Editor() { }


void Editor::SetTheme(const std::string theme_name) {
  auto it = themes.find(theme_name);
  if (it == themes.end()) {
    std::string errmsg = std::string("Theme not found (name='") + theme_name + "')";
    Editor::Error(errmsg);
    return;
  }
  theme = it->second.get();
}


void Editor::SetFrontEnd(std::unique_ptr<FrontEnd> frontend) {
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

  ASSERT(frontend != nullptr, "No frontend is available. Did you forgot to "
                              "call IEditor::SetFrontEnd() ?");

  ASSERT(ui != nullptr, "ui is nullptr. Did you forget to set one?");

  if (!frontend->Initialize()) {
    fprintf(stdout, "Editor initialize failed.\n");
    return 1;
  }

  // Async run event loop.
  std::thread event_loop([this]() { EventLoop(); });

  // We'll use this to limit the frame rate and sleep for other threads to run
  // while we sleep.
  int time_ms = GetElapsedTime();

  // FIXME(mess,config): move this to config.
  // 30 FPS : 33 milliseconds
  // 60 FPS : 16 milliseconds
  const int WAIT_TIME_MS = 33;

  // Main thread hot loop.
  while (running) {

    // Handle Events.
    while (!event_queue.Empty()) {
      Event event = event_queue.Dequeue();
      // FIXME: Handle the CLOSE event properly.
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
      Draw();
      redraw = false;
    }

    // Wait till we reach the frame rate limit.
    int now = GetElapsedTime();
    int delta = now - time_ms;
    if (delta < WAIT_TIME_MS) {
      time_ms = now;
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


// FIXME: This needs to be re-factored and cleaned up.
// Draw to the front end buffer.
void Editor::Draw() {
  Style style_bg = Editor::GetTheme()->GetStyle("ui.background");
  FrameBuffer& buff = frontend->GetDrawBuffer();

  // Clear the background.
  for (int i = 0; i < buff.width * buff.height; i++) {
    SET_CELL_I(buff, i, ' ', style_bg);
  }

  ui->Draw(buff);
  frontend->Display();
}


// FIXME: Properly re-implement this method.
std::shared_ptr<Document> Editor::OpenDocument(const Path& path) {

  // If the document is already opened, just return it.
  auto it = documents.find(path);
  if (it != documents.end()) return it->second;

  std::string text;
  if (!Platform::ReadFile(&text, path)) return nullptr;

  std::shared_ptr<Buffer> buff = std::make_shared<Buffer>(text);
  std::shared_ptr<Document> document = std::make_shared<Document>(path, buff);
  document->SetThemeGetter([](){ return GetTheme(); });

  documents[path] = document;
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

  // FIXME(grep): We shouldn't start all the servers at the start. Move this to
  // somewhere else.
  client->StartServer();
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
