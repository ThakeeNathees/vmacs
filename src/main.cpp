//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include "core/common.hpp"
#include "core/window.hpp"

#include "core/font.hpp"
#include "core/theme.hpp"
#include "editor/language.hpp"
#include "widgets/texteditor/texteditor.hpp"

void LoadResources();


int main() {

  Window::Init();
  FontManager::Init();
  ThemeManager::Init();
  LanguageManager::Init();

  LoadResources();

  Window::SetMainWidget(std::make_unique<TextEditor>());

  while (!Window::ShouldClose()) {
    Window::HandleInputs();
    Window::Update();
    Window::Draw();
  }

  Window::Close();

  return 0;
}
