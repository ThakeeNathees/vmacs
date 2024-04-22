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


// FIXME: cleanup this mess.
// #define TEST_FILE_CONTENT "#include <stdio.h>\n\nint main() {\n  printf(\"%i\");\n  x\n  return 0;\n}\n"
#define TEST_FILE_CONTENT "#include <stdio.h>\n\nint main() {\n  printf(\"%llu\");\n  abcx\n  return 0;\n}\n"
Editor::Editor():
  docpane(std::make_shared<Document>(std::make_shared<Buffer>(std::string(TEST_FILE_CONTENT))))
{}


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
