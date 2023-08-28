//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "texteditor.hpp"
#include "core/font.hpp"
#include "core/theme.hpp"

#include "editor/highlighter.hpp"


std::shared_ptr<ModeList> TextEditor::_GetModes() {
  static std::shared_ptr<ModeList> modes = std::make_shared<ModeList>(
    std::initializer_list<std::shared_ptr<Mode>>({

      Mode::New("normal", {
        { "esc" , TextEditor::_ClearCursors },
      }),

    })
  );
  return modes;
}


TextEditor::TextEditor(std::shared_ptr<File> file) {
  _SetModes(_GetModes());

  std::unique_ptr<TextBox> tb = std::make_unique<TextBox>(true, file->GetBuffer());
  tb->SetHighlighter(file->GetHighlighter());
  textbox = static_cast<TextBox*>(AddChild(std::move(tb)));
}


void TextEditor::_ClearCursors(Widget* w, CommandArgs args) {
  TextEditor* e = static_cast<TextEditor*>(w);
  e->textbox->ClearCursors();
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


int TextEditor::_DrawLineNumbers(Vector2 pos, Size area) {

  if (area.height == 0) return 0;

  
  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();
  Font font = FontManager::GetFont()->regular;
  Size char_size = FontManager::GetFont()->GetCharSize();
  int font_size = FontManager::GetFontSize();

  const Buffer* buffer = textbox->GetBuffer();
  Coord view_start = textbox->GetViewStart();

  bool has_null_line = buffer->HasNullLine();
  int line_count = buffer->GetLineCount();
  int ln_digit_count = _ToStrReversed(line_count, nullptr);
  ln_digit_count = MAX(ln_digit_count, gutter_min_width);


  // Not enought space to draw the lines.
  if ((area.width / char_size.width) < ln_digit_count) return 0;

  // We add 1 and let the last character cropped out at the middle.
  int visible_line_count = (area.height / char_size.height) + 1;
  int total_line_count = buffer->GetLineCount();

  // Enough to hold all digits of a 32-bit integer.
  char buff[12] = { '\0' };

  Vector2 curr = pos; // Current cursor position.

  for (int i = 0; i < visible_line_count; i++) {

    // view_start is 0 based so we add 1.
    int line_number = view_start.row + i + 1;
    if (line_number > total_line_count) break;

    int count = _ToStrReversed(line_number, buff);
    if (line_number == line_count && has_null_line) {
      buff[0] = '~';
      count = 1;
    }

    curr.x += (ln_digit_count - count) * char_size.width;
    for (int j = count - 1; j >= 0; j--) {
      DrawTextCodepoint(
        font,
        (int) buff[j],
        curr,
        (float) font_size,
        ui.linenr.fg);
      curr.x += char_size.width;
    }

    curr.x = pos.x;
    curr.y += char_size.height;
  }

  return ln_digit_count;
}


void TextEditor::_DrawStatusLine(Vector2 pos, Size area) {
  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();
  DrawRectangle(
    (int) pos.x,
    (int) pos.y,
    area.width,
    area.height,
    ui.statusline.bg);
}


void TextEditor::_Draw(Size area) {

  const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();

  const Buffer* buffer = textbox->GetBuffer();

  Font font = FontManager::GetFont()->regular;
  Size char_size = FontManager::GetFont()->GetCharSize();
  
  ClearBackground(ui.background.bg);

  Vector2 pos = { 0, 0 };
  Size draw_area = area;

  pos.x += gutter_margin_left * char_size.width;
  draw_area.width -= (int) pos.x;

  int gutter_width = _DrawLineNumbers(pos, draw_area);
  pos.x += (gutter_width + text_margin_left) * char_size.width;

  draw_area.width -= (int) pos.x;
  draw_area.height -= char_size.height * 1; // status line at the bottom.

  textbox->Draw(this, draw_area);
  DrawRenderTexture(textbox->GetCanvas(), pos);

  pos.x = 0;
  pos.y = (float)(area.height - char_size.height * 1);
  draw_area.height = char_size.height * 1;
  draw_area.width = area.width;
  _DrawStatusLine(pos, draw_area);

}
