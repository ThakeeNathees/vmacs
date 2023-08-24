// Copyright (c) 2023 Thakee Nathees

#include "textbox.h"

#include "core/window.h"


TextBox::TextBox(
  Window* window,
  bool multiline,
  std::shared_ptr<Buffer> buffer,
  std::shared_ptr<BufferThemeCache> themecache)
	: Widget(window), multiline(multiline), buffer(buffer), themecache(themecache) {

  _UpdateModes(_GetModes());
  if (multiline) SetMode("multi_line");
  else SetMode("single_line");
}


void TextBox::Update() {
  cursors.BlinkUpdate();
}


void TextBox::OnFocusChanged() {
  cursors.ResetBlinkTimer();
}


void TextBox::ClearCursors() {
  cursors.ClearMultiCursors();
  cursors.ClearSelections();
}


void TextBox::Clean() {
  int buff_size = buffer->GetSize();
  buffer->RemoveText(0, buff_size);
  History& history = buffer->GetHistory();
  history.Clean();
  ClearCursors();
  cursors.Get()[0].SetIndex(buffer.get(), 0);
}


void TextBox::_EnsureCursorsOnView() {
  Coord coord = cursors.GetPrimaryCursor().GetCoord();

  if (coord.col < view_start.col) {
    view_start.col = coord.col;
  } else if (view_start.col + text_area.width <= coord.col) {
    view_start.col = coord.col - text_area.width + 1;
  }

  if (coord.row < view_start.row) {
    view_start.row = coord.row;
  } else if (view_start.row + text_area.height <= coord.row) {
    view_start.row = coord.row - text_area.height + 1;
  }
}


void TextBox::_DrawSelections(Size area) {
  // TODO: This is in efficient and run inside a hot loop, refactor.

  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
  Vector2i char_size = window->GetFontCharSize();

  int line_count = buffer->GetLineCount();
  int first_line = view_start.row;
  int last_line = MIN(view_start.row + (area.height / char_size.y) - 1, line_count - 1);

  for (int i = first_line; i <= last_line; i++) {
    Slice line = buffer->GetLine(i);

    int row = i - view_start.row; // Current row.

    for (int j = line.start; j <= line.end; j++) {

      int col = (j - line.start) - view_start.col;
      if (col < 0 || col >(area.width / char_size.x)) continue;

      for (const Cursor& cursor : cursors.Get()) {
        Slice selection = cursor.GetSelection();
        if (BETWEEN(selection.start, j, selection.end - 1)) {

          DrawRectangle(
            col * char_size.x,
            row * char_size.y,
            char_size.x,
            char_size.y,
            ui.selection.bg);

        }
      }
    }
  }
}


void TextBox::_DrawCursors(Size area) {
  if (!IsFocused() || !cursors.IsBlinkShow()) return;

  Vector2i char_size = window->GetFontCharSize();
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();

  for (const Cursor& cursor : cursors.Get()) {

    // Get the coordinate and make it relative to view.
    Coord coord = cursor.GetCoord();
    coord.col -= view_start.col;
    coord.row -= view_start.row;

    // If out of the area skip.
    if (coord.col < 0 || coord.row < 0) continue;
    if (coord.col > area.width / char_size.x ||
        coord.row > area.height / char_size.y) continue;

    int draw_x = coord.col * char_size.x;
    int draw_y = coord.row * char_size.y;
    DrawRectangle(
      draw_x,
      draw_y,
      char_size.x,
      char_size.y,
      ((ui.cursor_primary.modifier & Theme::Modifier::REVERSED)
       ? ui.cursor_primary.fg
       : ui.cursor_primary.bg));

    int codepoint = buffer->At(cursor.GetIndex());
    if (codepoint != '\0' && codepoint != '\n') {
      DrawTextCodepoint(
        window->GetFont(),
        codepoint,
        { (float)draw_x, (float)draw_y },
        (float)window->GetFontSize(),
        ui.background.bg);
    }
  }
}


void TextBox::_DrawText(Size area) {
  Vector2i char_size = window->GetFontCharSize();
  int font_size = window->GetFontSize();
  Font font = window->GetFont();
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();

  const std::vector<BufferThemeCache::Themelet>* themelets = ((themecache == nullptr)
    ? nullptr
    : themecache->GetThemeles()
  );

  // We add 1 and let the last character cropped out at the middle.
  int visible_line_count = (area.height / char_size.y) + 1;
  int total_line_count = buffer->GetLineCount();

  Vector2i curr = { 0, 0 };
  for (int i = 0; i <= visible_line_count; i++) {

    int line_index = view_start.row + i;
    if (line_index >= total_line_count) break;

    Slice line = buffer->GetLine(line_index);

    int line_len = line.end - line.start;
    if (line_len >= view_start.col) {

      // How many visible characters in this line.
      int char_count = line_len - view_start.col;

      for (int j = 0; j < char_count; j++) {
        int index = line.start + view_start.col + j;
        int c = buffer->At(index);

        switch (c) {

          default:
          {
            Color color = ((themelets != nullptr && index < themelets->size())
                       ? (*themelets)[index].color
                       : WHITE);
            DrawTextCodepoint(font, c, curr.Float(), (float)font_size, color);
            curr.x += char_size.x;
          } break;

          case '\0':
          case '\n':
          UNREACHABLE();
          break;

          // TODO: \t
        }
      }
    }

    curr.y += char_size.y;
    curr.x = 0;
  }
}


void TextBox::_Draw(Size area) {
  ASSERT(theme_ptr != nullptr && *theme_ptr != nullptr, OOPS);

  const Theme* theme = *theme_ptr;
  const UiThemeCache& ui = theme->GetUiEntries();
  Vector2i char_size = window->GetFontCharSize();

  // "Cache" the text area dimentions.
  text_area = {
     area.width / char_size.x,
     area.height / char_size.y
  };

  ClearBackground(ui.background.bg);

  _DrawSelections(area);
  _DrawText(area);
  _DrawCursors(area);
}
