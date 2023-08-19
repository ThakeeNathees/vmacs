// Copyright (c) 2023 Thakee Nathees

#include <stdio.h>

#include "editor.h"
#include "core/highligther.h"


std::shared_ptr<Mode> TextEditor::mode_normal = std::make_shared<Mode>(
  "normal",
  nullptr,
  KeyBindings::New({
    { "esc" , TextEditor::_ClearCursors },
  })
);


void TextEditor::_ClearCursors(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  e->textbox->ClearCursors();
}


TextEditor::TextEditor(Window* window) : Widget(window) {

  // TODO: Move this.
  Language::LoadLanguages();

  std::unique_ptr<File> file = File::New(window);
  file->SetLanguage(Language::GetLanguage("c"));

  std::unique_ptr<TextBox> tb = std::make_unique<TextBox>(window, std::move(file), true);
  int idx = AddChild(std::move(tb));
  textbox = static_cast<TextBox*>(GetChild(idx));
  SetFocusedChild(idx);
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

  const Buffer* buffer = textbox->GetFile()->GetBuffer();
  Font font = window->GetFont();
  Vector2i char_size = window->GetFontCharSize();
  int font_size = window->GetFontSize();
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
  Coord view_start = textbox->GetViewStart();

  bool has_null_line = buffer->HasNullLine();
  int line_count = buffer->GetLineCount();
  int ln_digit_count = _ToStrReversed(line_count, nullptr);
  ln_digit_count = MAX(ln_digit_count, gutter_min_width);


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


void TextEditor::_DrawStatusLine(Vector2i pos, Size area) {
  const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
  DrawRectangle(pos.x, pos.y, area.width, area.height, ui.statusline.bg);
}


void TextEditor::_Draw(Size area) {

  ASSERT(theme_ptr != nullptr && *theme_ptr != nullptr, OOPS);

  const Theme* theme = *theme_ptr;
  const UiThemeCache& ui = theme->GetUiEntries();
  const Buffer* buffer = textbox->GetFile()->GetBuffer();

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

  textbox->Draw(this, draw_area);
  DrawRenderTexture(textbox->GetCanvas(), pos.Float());

  pos.x = 0;
  pos.y = area.height - char_size.y * 1;
  draw_area.height = char_size.y * 1;
  draw_area.width = area.width;
  _DrawStatusLine(pos, draw_area);

}
