//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT


#include "fe_raylib.hpp"

// FIXME: for now to get the color.
#include "core/core.hpp"


bool Raylib::Initialize() {
  int width = 800, height = 450;
  const char* title = "vmacs";

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(width, height, title);
  SetTargetFPS(60);
  SetExitKey(0); // Don't close on escape.

  const char* font_path = "./res/SauceCodeProNerdFontPropo-Medium.ttf";
  font = LoadFontEx(font_path, font_size, NULL, 0);

  return true;
}


bool Raylib::Cleanup() {
  UnloadFont(font);
  CloseWindow();
  return true;
}


DrawBuffer Raylib::GetDrawBuffer() {

  int char_width = font.recs[0].width;
  int char_height = font.recs[0].height;

  int width = GetScreenWidth()/char_width;
  int height = GetScreenHeight()/char_height;

  if (cells.size() != width * height) {
    cells.resize(width*height);
  }

  draw_buffer.width  = width;
  draw_buffer.height = height;
  draw_buffer.cells  = cells.data();

  return draw_buffer;
}


void Raylib::Display(Color clear_color) {

  int width = draw_buffer.width;
  int height = draw_buffer.height;

  int char_width = font.recs[0].width;
  int char_height = font.recs[0].height;


  BeginDrawing();
  ClearBackground(GetColor((clear_color << 8) | 0xff));

  // This should be optimized with memmove of the buffer with the termbox
  // buffer and they provide an api to do so.
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Cell& cell = cells[y*width + x];

        float cx = x * char_width;
        float cy = y * char_height;

        // Convert to rgb color and add alpah channel to the rgb value and
        // create color.
        // TODO: Handle attributes.
        ColorRaylib fg = GetColor((cell.fg << 8) | 0xff);
        ColorRaylib bg = GetColor((cell.bg << 8) | 0xff);

        DrawRectangle( cx, cy, char_width, char_height, bg);
        DrawTextCodepoint(font, cell.ch, {cx, cy}, (float)font_size, fg);

    }
  }

  EndDrawing();

}

std::vector<Event> Raylib::GetEvents() {

  std::vector<Event> events; // All the events for the current frame.

  if (WindowShouldClose()) events.push_back(Event(Event::Type::CLOSE));

  do {

    if (IsWindowResized()) {
      Event e(Event::Type::RESIZE);
      e.resize.width = GetScreenWidth();
      e.resize.height = GetScreenHeight();
      events.push_back(e);
    }

    // Ignore all the ctrl, shift, alt, super keys. (handled by the modifiers).
    int key = GetKeyPressed();
    if (KEY_LEFT_SHIFT <= key && key <= KEY_RIGHT_SUPER) continue;
    int chr = GetCharPressed();

    if (key == 0 && chr == 0) break;

    // Since our key is one-to-one mapping of raylib keys we just type cast it
    // and it'll work.
    Event e(Event::Type::KEY);

    // Space is not considered as printable character and we treat that as a
    // space key differently like enter and tab.
    e.key.unicode = (chr != ' ') ? chr : 0;

    e.key.code = (Event::Keycode) key;
    e.key.alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    e.key.shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    e.key.ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    events.push_back(e);

  } while (true);

  return events;
}

