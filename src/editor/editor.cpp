// Copyright (c) 2023 Thakee Nathees

#include <stdio.h>

#include "editor.h"
#include "core/highligther.h"


TextEditor::TextEditor(Window* window) : Widget(window) {

  _LoadModes();

  // TODO: Move this.
  Language::LoadLanguages();

  file->SetLanguage(Language::GetLanguage("c"));
  file->themecache->SetThemePtr(window->GetThemeManager().GetThemePtr());
  window->GetThemeManager().RegisterListener(file.get());
}


void TextEditor::_EnsureCursorsOnView() {
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


void TextEditor::Update() {
  cursors.BlinkUpdate();
}


// Write the buffer the digits in a reversed order if present and returns the count
// of the digits, assuming that the buffer has enought space.
static inline int _ToStrReversed(int number, char* buff) {
  ASSERT(number >= 0, OOPS);
  if (number == 0) {
    if (buff != nullptr) buff[0] = '0';
    return 1;
  }

  int count = 0;
  while (number > 0) {
    int digit = number % 10;
    if (buff != nullptr) buff[count] = '0' + digit;
    count++;
    number /= 10;
  }

  return count;
}


int TextEditor::_DrawLineNumbers(Vector2i pos, Size area) {
  ASSERT(theme_ptr != nullptr && *theme_ptr != nullptr, OOPS);

  if (area.height == 0) return 0;

  bool has_null_line = file->GetBuffer()->HasNullLine();
  int line_count = file->GetBuffer()->GetLineCount();
  int ln_digit_count = _ToStrReversed(line_count, nullptr);
  ln_digit_count = MAX(ln_digit_count, gutter_min_width);

  Font font = window->GetFont();
  const Buffer* buffer = file->GetBuffer();
  Vector2i char_size = window->GetFontCharSize();
  int font_size = window->GetFontSize();
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();

  // Not enought space to draw the lines.
  if ((area.width / char_size.x) < ln_digit_count) return 0;

  // We add 1 and let the last character cropped out at the middle.
  int visible_line_count = (area.height / char_size.y) + 1;
  int total_line_count = buffer->GetLineCount();

  // Enough to hold all digits of a 32-bit integer.
  char buff[12] = { '\0' };

  Vector2i curr = pos; // Current cursor position.

  for (int i = 0; i < visible_line_count; i++) {

    // view_start is 0 based so we add 1.
    int line_number = view_start.row + i + 1;
    if (line_number > total_line_count) break;
    
    int count = _ToStrReversed(line_number, buff);
    if (line_number == line_count && has_null_line) {
      buff[0] = '~';
      count = 1;
    }

    curr.x += (ln_digit_count - count) * char_size.x;
    for (int j = count - 1; j >= 0; j--) {
      DrawTextCodepoint(font, (int)buff[j], curr.Float(), (float)font_size, ui.linenr.fg);
      curr.x += char_size.x;
    }

    curr.x = pos.x;
    curr.y += char_size.y;
  }

  return ln_digit_count;
}


void TextEditor::_DrawSelections(Vector2i pos, Size area) {

  // TODO: This is in efficient and run inside a hot loop, refactor.

  const Buffer* buffer = file->GetBuffer();
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
      if (col < 0 || col > (area.width / char_size.x)) continue;

      for (const Cursor& cursor : cursors.Get()) {
        Slice selection = cursor.GetSelection();
        if (BETWEEN(selection.start, j, selection.end - 1)) {

          DrawRectangle(
            pos.x + col * char_size.x,
            pos.y + row * char_size.y,
            char_size.x,
            char_size.y,
            ui.selection.bg);

        }
      }

    }
  }
}


void TextEditor::_DrawCursors(Vector2i pos, Size area) {

  if (!cursors.IsBlinkShow()) return;

  const Buffer* buffer = file->GetBuffer();
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
  
    int draw_x = pos.x + coord.col * char_size.x;
    int draw_y = pos.y + coord.row * char_size.y;
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
        { (float) draw_x, (float) draw_y },
        (float) window->GetFontSize(),
        ui.background.bg);
    }

  }

}


void TextEditor::_DrawText(Vector2i pos, Size area) {

  const Buffer* buffer = file->GetBuffer();
  Vector2i char_size = window->GetFontCharSize();
  int font_size = window->GetFontSize();
  Font font = window->GetFont();
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
  const auto& themelets = file->themecache->GetThemeles();

  // We add 1 and let the last character cropped out at the middle.
  int visible_line_count = (area.height / char_size.y) + 1;
  int total_line_count = buffer->GetLineCount();

  Vector2i curr = pos;
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

          default: {
            Color color = (index < themelets.size()) ? themelets[index].color : WHITE;
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
    curr.x = pos.x;
  }

}


void TextEditor::_DrawStatusLine(Vector2i pos, Size area) {
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
  DrawRectangle(pos.x, pos.y, area.width, area.height, ui.statusline.bg);
}


void TextEditor::_Draw(Size area) {

  ASSERT(theme_ptr != nullptr, OOPS);
  const Theme* theme = *theme_ptr;
  ASSERT(theme != nullptr, OOPS);

  const UiThemeCache& ui = theme->GetUiEntries();
  const Buffer* buffer = file->GetBuffer();

  Font font = window->GetFont();
  int font_size = window->GetFontSize();
  Vector2i char_size = window->GetFontCharSize();

  ClearBackground(ui.background.bg);

  Vector2i pos = { 0, 0 };
  Size draw_area = area;

  pos.x += gutter_margin_left * char_size.x;
  draw_area.width -= pos.x;
  
  int gutter_width = _DrawLineNumbers(pos, draw_area);
  pos.x += (gutter_width + text_margin_left) * char_size.x;

  draw_area.width -= pos.x;
  draw_area.height -= char_size.y * 1; // status line at the bottom.

  // "Cache" the text area dimentions.
  text_area = {
     draw_area.width / char_size.x,
     draw_area.height / char_size.y
  };

  _DrawSelections(pos, draw_area);
  _DrawText(pos, draw_area);
  _DrawCursors(pos, draw_area);

  pos.x = 0;
  pos.y = area.height - char_size.y * 1;
  draw_area.height = char_size.y * 1;
  draw_area.width = area.width;

  _DrawStatusLine(pos, draw_area);

}
