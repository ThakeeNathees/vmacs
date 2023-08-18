// Copyright (c) 2023 Thakee Nathees

#include <stdio.h>

#include "raylib.h"
#include "core/window.h"

#include "core/window.h"

#include "editor/editor.h"
#include "filemanager/filemanager.h"
#include "minibuffer/minibuffer.h"


int main(void) {

  Window window;
  window.Init();

  //TextEditor editor(&window);
  //window.widget = &editor;

  MiniBuffer minibuffer(&window);
  window.widget = &minibuffer;

  //FileManager filemanager(&window);
  //window.widget = &filemanager;

  while (!window.ShouldClose()) {

    window.HandleInputs();
    window.Update();

    BeginDrawing();
    window.Draw();
    EndDrawing();
  }

  window.Close();

  return 0;
}
