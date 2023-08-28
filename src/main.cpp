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

#include "widgets/layout/layout.hpp"
#include "widgets/texteditor/texteditor.hpp"

void LoadResources();


int main() {

  Window::Init();
  FontManager::Init();
  ThemeManager::Init();
  LanguageManager::Init();

  LoadResources();


  std::shared_ptr<File> file = File::New();
  file->SetLanguage(LanguageManager::GetLanguage("c"));

  //auto hsplit = std::make_unique<HSplit>();
  //hsplit->AddChild(std::make_unique<TextEditor>(file));
  //hsplit->AddChild(std::make_unique<TextEditor>(file));
  //auto vsplit = std::make_unique<VSplit>();
  //vsplit->AddChild(std::make_unique<TextEditor>(file));
  //vsplit->AddChild(std::move(hsplit));
  //Window::SetMainWidget(std::make_unique<RootView>(std::move(vsplit)));

  Window::SetMainWidget(std::make_unique<TextEditor>(file));


  while (!Window::ShouldClose()) {
    Window::HandleInputs();
    Window::Update();
    Window::Draw();
  }

  Window::Close();

  return 0;
}
