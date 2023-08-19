// Copyright (c) 2023 Thakee Nathees

#include <memory>
#include <stdio.h>

#include "raylib.h"
#include "core/window.h"

#include "core/window.h"

#include "widgets/layout/layout.h"
#include "widgets/editor/editor.h"
#include "widgets/filemanager/filemanager.h"
#include "widgets/minibuffer/minibuffer.h"


int main(void) {

  Window window;
  window.Init();

  std::unique_ptr<TextEditor> editor = std::make_unique<TextEditor>(&window);
  std::unique_ptr<MiniBuffer> minibuffer = std::make_unique<MiniBuffer>(&window);

  HSplit split(&window, std::move(editor), std::move(minibuffer));
  window.widget = &split;
  split.SetFocused(true);

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
