//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <memory>

#include "common.hpp"
#include "event.hpp"
#include "font.hpp"
#include "widget.hpp"


class Window {
public:

  // Life cycle.
  static void Init();
  static bool ShouldClose();
  static void HandleInputs();
  static void Update();
  static void Draw();
  static void Close();

  // Display an error popup.
  static void Error(std::string_view message);

private:
  Window() {}
  bool _HandleEvent(const Event& event);
  void _LoadConfig();

  static std::unique_ptr<Window> singleton;

  bool should_close = false;
  std::unique_ptr<Widget> main_widget;

  // Configs.
  const char* title = "vmacs";
  int width = 800, height = 450;
  int fps = 60;
  int font_size = 32;
};
