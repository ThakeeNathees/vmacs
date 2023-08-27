//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include "core/common.hpp"
#include "core/window.hpp"


int main() {

  Window::Init();

  while (!Window::ShouldClose()) {
    Window::HandleInputs();
    Window::Update();
    Window::Draw();
  }

  Window::Close();

  return 0;
}
