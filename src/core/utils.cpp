//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "core.hpp"
#include "color.xmacro.h"

#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>


event_t EncodeKeyEvent(Event::Key key) {
  uint32_t ret = 0x0;
  if (key.unicode != 0) {
    ret |= ((key.unicode & 0xff) << 16);
  } else {
    ret |= key.code;
    ret |= key.ctrl  ? 0x400  : 0;
    ret |= key.alt   ? 0x800  : 0;
    ret |= key.shift ? 0x1000 : 0;
  }
  return ret;
}


Event::Key DecodeKeyEvent(event_t key) {
  Event::Key ret;
  ret.code    = (Event::Keycode) (key & 0x3ff);
  ret.unicode = (key >> 16) & 0xff;
  ret.ctrl    = key & 0x400;
  ret.alt     = key & 0x800;
  ret.shift   = key & 0x1000;
  return ret;
}


static std::map<uint8_t, uint32_t> xterm_to_rgb = {
#define X(short, rgb) {short, rgb},
  COLOR_MAP(X)
#undef X
};


static std::map<uint32_t, uint8_t> rgb_to_xterm = {
#define X(short, rgb) {rgb, short},
  COLOR_MAP(X)
#undef X
};


static int _GetClosestColorPart(uint8_t byte) {
  static const int incs[] = {0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
  static const int incs_size = sizeof incs / sizeof *incs;

  for (int i = 0; i <= incs_size - 2; i++) {
    int low = incs[i], high = incs[i+1];
    if (low <= byte && byte <= high) {
      int dist_low = byte - low;
      int dist_high = high - byte;

      if (dist_low < dist_high) return low;
      return high;
    }
  }

  return -1;
}


uint8_t RgbToXterm(uint32_t rgb) {
  int r = (rgb >> 16) & 0xff;
  int g = (rgb >>  8) & 0xff;
  int b = (rgb >>  0) & 0xff;
  r = _GetClosestColorPart(r);
  g = _GetClosestColorPart(g);
  b = _GetClosestColorPart(b);
  return rgb_to_xterm[(r << 16) | (g << 8) | (b << 0)];
}


uint32_t XtermToRgb(uint8_t xterm) {
  return xterm_to_rgb[xterm];
}


int GetElapsedTime() {
  using hr_clock = std::chrono::high_resolution_clock;
  using hr_time_point = hr_clock::time_point;
  using hr_duration = hr_clock::duration;
  using milliseconds = std::chrono::milliseconds;

  static hr_time_point program_start = hr_clock::now();
  hr_duration duration = hr_clock::now() - program_start;
  return std::chrono::duration_cast<milliseconds>(duration).count();
}

// -----------------------------------------------------------------------------
// String functions.
// -----------------------------------------------------------------------------

// Copied from: https://stackoverflow.com/a/42844629/10846399
bool EndsWith(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}


bool StartsWith(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}


bool IsCharName(int c) {
  if (BETWEEN('a', c, 'z')) return true;
  if (BETWEEN('A', c, 'Z')) return true;
  if (BETWEEN('0', c, '9')) return true;
  if (c == '_') return true;
  return false;
}


bool IsCharWhitespace(int c) {
  return (c == ' ' || c == '\t' || c == '\n');
}


std::vector<std::string> StringSplit(const std::string& str, char delim) {
  std::vector<std::string> ret;

  // Start index to search for the next match.
  int start = 0;

  while (true) {
    size_t pos = str.find(delim, start);
    if (pos == std::string::npos) {
      ret.push_back(str.substr(start));
      break;
    } else {
      ret.push_back(str.substr(start, pos - start));
      start = pos + 1;
    }
  }

  return ret;
}


// -----------------------------------------------------------------------------
// Config related functions.
// -----------------------------------------------------------------------------


// FIXME: Remove this ugly logic.
LanguageId Config::GetLangIdFromFileName(const std::string& filename) const {
  if (EndsWith(filename, ".py")) return "python";

  if (EndsWith(filename, ".cpp")) return "cpp";
  if (EndsWith(filename, ".hpp")) return "cpp";
  if (EndsWith(filename, ".inl")) return "cpp";

  if (EndsWith(filename, ".c")) return "c";
  if (EndsWith(filename, ".h")) return "c";

  return "";
}


// FIXME: Remove this ugly logic.
LspClientId Config::GetLspClientIdFromFileName(const std::string& filename) const {
  if (EndsWith(filename, ".py")) return "pylsp";

  if (EndsWith(filename, ".cpp")) return "clangd";
  if (EndsWith(filename, ".hpp")) return "clangd";
  if (EndsWith(filename, ".inl")) return "clangd";
  if (EndsWith(filename, ".c"))   return "clangd";
  if (EndsWith(filename, ".h"))   return "clangd";

  return "";
}


// -----------------------------------------------------------------------------
// Draw primitives.
// -----------------------------------------------------------------------------


void DrawTextLine(
    FrameBuffer& buff,
    const char* text,
    Position pos,
    int width,
    Style style,
    const Icons& icons,
    bool fill_area) {

  if (text == NULL || *text == '\0') return;
  if (pos.col < 0 || pos.row < 0) return;
  if (pos.col >= buff.width || pos.row >= buff.height) return;
  if (pos.col + width > buff.width) width = buff.width - pos.col;
  if (width <= 0) return;

  int length = Utf8Strlen(text);
  int text_len = MIN(length, width);
  int trim_indicator = icons.trim_indicator;

  bool trimming = (length > width);
  if (trimming) text_len -= 1;

  // Current x position we're drawing.
  int x = pos.col;
  const char* c = text;

  for (; x < pos.col + text_len; x++) {
    uint32_t ch;
    c += Utf8CharToUnicode(&ch, c);
    if (ch == '\n' || ch == '\t') ch = ' ';
    SET_CELL(buff, x, pos.row, ch, style);
  }

  if (trimming) SET_CELL(buff, x++, pos.row, trim_indicator, style);
  if (fill_area) {
    while (x < (pos.col + width)) {
      SET_CELL(buff, x++, pos.row, ' ', style);
    }
  }
}


void DrawRectangleFill(FrameBuffer& buff, Position pos, Area area, Style style) {

  // Clip the rectangle to our frame and if the entire rectangle is out of the
  // current frame or the size after clip is zero we don't have to draw.
  if (pos.x >= buff.width || pos.y >= buff.height) return;
  pos.x = MAX(pos.x, 0);
  pos.y = MAX(pos.y, 0);
  area.width  = MIN(area.width, buff.width-pos.x);
  area.height = MIN(area.height, buff.height-pos.y);
  if (area.width <= 0 || area.height <= 0) return;

  for (int y = pos.y; y < pos.y+area.height; y++) {
    for (int x = pos.x; x < pos.x+area.width; x++) {
      SET_CELL(buff, x, y, ' ', style);
    }
  }
}


void DrawRectangleLine(FrameBuffer& buff, Position pos, Area area, Style style, const Icons& icons, bool fill) {
  // Clip the rectangle to our frame and if the entire rectangle is out of the
  // current frame or the size after clip is zero we don't have to draw.
  if (pos.x >= buff.width || pos.y >= buff.height) return;
  pos.x = MAX(pos.x, 0);
  pos.y = MAX(pos.y, 0);
  area.width  = MIN(area.width, buff.width-pos.x);
  area.height = MIN(area.height, buff.height-pos.y);
  if (area.width <= 0 || area.height <= 0) return;

  // For ease of use.
  const int x = pos.x;
  const int y = pos.y;
  const int w = area.width;
  const int h = area.height;

  const int tr = icons.tr;
  const int tl = icons.tl;
  const int br = icons.br;
  const int bl = icons.bl;
  const int hl = icons.hl;
  const int vl = icons.vl;

  SET_CELL(buff, x,     y,     tr, style); // Top right.
  SET_CELL(buff, x+w-1, y,     tl, style); // Top left.
  SET_CELL(buff, x,     y+h-1, bl, style); // Bottom left.
  SET_CELL(buff, x+w-1, y+h-1, br, style); // Bottom right.

  for (int col = x+1; col < x+w-1; col++) {
    SET_CELL(buff, col, y,     hl, style); // Tob horizontal bar.
    SET_CELL(buff, col, y+h-1, hl, style); // Bottom horizontal bar.
  }

  for (int row = y+1; row < y+h-1; row++) {
    SET_CELL(buff, x,     row, vl, style); // Left vertical bar.
    SET_CELL(buff, x+w-1, row, vl, style); // Right vertical bar.
  }

  if (fill) {
    pos.x++; pos.y++; area.width -= 2; area.height -= 2;
    DrawRectangleFill(buff, pos, area, style);
  }
}


void DrawHorizontalLine(FrameBuffer& buff, Position pos, int width, Style style, const Icons& icons) {
  // Clip the line and if it's out of the current frame, we don't have to draw.
  if (pos.x >= buff.width || pos.y >= buff.height) return;
  pos.x = MAX(pos.x, 0);
  pos.y = MAX(pos.y, 0);
  width = MIN(width, buff.width-pos.x);
  if (width <= 0) return;

  for (int x = pos.x; x < pos.x+width; x++) {
    SET_CELL(buff, x, pos.y, icons.hl, style);
  }
}


void DrawVerticalLine(FrameBuffer& buff, Position pos, int height, Style style, const Icons& icons) {
  // Clip the line and if it's out of the current frame, we don't have to draw.
  if (pos.x >= buff.width || pos.y >= buff.height) return;
  pos.x  = MAX(pos.x, 0);
  pos.y  = MAX(pos.y, 0);
  height = MIN(height, buff.height-pos.y);
  if (height <= 0) return;

  for (int y = pos.y; y < pos.y+height; y++) {
    SET_CELL(buff, pos.x, y, icons.vl, style);
  }
}


// ----------------------------------------------------------------------------
// Utf8 helper functions.
// ----------------------------------------------------------------------------

static const unsigned char utf8_length[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

static const unsigned char utf8_mask[6] = { 0x7F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };

int Utf8CharLength(char c) {
  return utf8_length[(unsigned char)c];
}

int Utf8CharToUnicode(uint32_t *out, const char *c) {
  if (*c == 0) return -1;

  int i;
  unsigned char len = Utf8CharLength(*c);
  unsigned char mask = utf8_mask[len-1];
  uint32_t result = c[0] & mask;
  for (i = 1; i < len; ++i) {
    result <<= 6;
    result |= c[i] & 0x3f;
  }

  *out = result;
  return (int)len;
}

int Utf8UnicodeToChar(char *out, uint32_t c) {
  int len = 0;
  int first;
  int i;

  if (c < 0x80) {
    first = 0;
    len = 1;
  } else if (c < 0x800) {
    first = 0xc0;
    len = 2;
  } else if (c < 0x10000) {
    first = 0xe0;
    len = 3;
  } else if (c < 0x200000) {
    first = 0xf0;
    len = 4;
  } else if (c < 0x4000000) {
    first = 0xf8;
    len = 5;
  } else {
    first = 0xfc;
    len = 6;
  }

  for (i = len - 1; i > 0; --i) {
    out[i] = (c & 0x3f) | 0x80;
    c >>= 6;
  }
  out[0] = c | first;

  return len;
}

int Utf8Strlen(const char* str) {
  int length = 0;
  while (*str) {
    str += Utf8CharLength(*str);
    length++;
  }
  return length;
}


std::string Utf8UnicodeToString(uint32_t c) {
  char bytes[4];
  int len = Utf8UnicodeToChar(bytes, c);
  return std::string(bytes, len);
}


// ----------------------------------------------------------------------------
// Key combination parsing.
// ----------------------------------------------------------------------------

#define STARTS_WITH(string, prefix) \
  (strncmp(string, prefix, strlen(prefix)) == 0)


#define CONSUME(ptr, prefix)        \
  ((STARTS_WITH(ptr, prefix))       \
    ? (ptr += strlen(prefix), true) \
    : (false)                       \
  )

// NOTE: This macro will break if we rename variables bellow.
#define PUSH_EVENT(keycode, codepoint)     \
  do {                                     \
    key.code = keycode;                    \
    key.unicode = codepoint;               \
    events.push_back(EncodeKeyEvent(key)); \
    memset(&key, 0, sizeof key);           \
  } while (false)


bool ParseKeyBindingString(std::vector<event_t>& events, const char* binding) {

  const char* ptr = binding;

  // current key we're passing, will be pushed to the list and resetted for the
  // next parsing event.
  Event::Key key {};

  while (*ptr) {
    char c = *ptr++;

    // Escape '<' and '\\'.
    // if (c == '\\') { TODO; continue; }

    // Parse special key or modifiers.
    if (c == '<') {

      // C-, M-, S-
      while (*ptr) {
        if (CONSUME(ptr, "C-")) { key.ctrl  = true; continue; }
        if (CONSUME(ptr, "A-")) { key.alt   = true; continue; }
        if (CONSUME(ptr, "S-")) { key.shift = true; continue; }
        break;
      }

      #define CONSUME_SECIAL_KEY(suffix, key) \
        if (CONSUME(ptr, suffix)) {           \
          PUSH_EVENT(Event::key, '\0');       \
          continue;                           \
        }
      CONSUME_SECIAL_KEY("esc>",       KEY_ESCAPE);
      CONSUME_SECIAL_KEY("space>",     KEY_SPACE);
      CONSUME_SECIAL_KEY("enter>",     KEY_ENTER);
      CONSUME_SECIAL_KEY("tab>",       KEY_TAB);
      CONSUME_SECIAL_KEY("backspace>", KEY_BACKSPACE);
      CONSUME_SECIAL_KEY("del>",       KEY_DELETE);
      CONSUME_SECIAL_KEY("up>",        KEY_UP);
      CONSUME_SECIAL_KEY("down>",      KEY_DOWN);
      CONSUME_SECIAL_KEY("left>",      KEY_LEFT);
      CONSUME_SECIAL_KEY("right>",     KEY_RIGHT);
      CONSUME_SECIAL_KEY("home>",      KEY_HOME);
      CONSUME_SECIAL_KEY("end>",       KEY_END);
      CONSUME_SECIAL_KEY("pageup>",    KEY_PAGE_UP);
      CONSUME_SECIAL_KEY("pagedown>",  KEY_PAGE_DOWN);
      #undef CONSUME_SECIAL_KEY

      c = *ptr++;

      // <C-c> consume a character after modifier and push the event.
      #define CONSUME_CHARACTER(character, key) \
        if (c == character) {                   \
          PUSH_EVENT(Event::key, '\0');         \
          if (*ptr++ != '>') return false;      \
          continue;                             \
        }

      CONSUME_CHARACTER('-', KEY_MINUS);
      CONSUME_CHARACTER('/', KEY_SLASH);
      CONSUME_CHARACTER('\\', KEY_BACKSLASH);
      CONSUME_CHARACTER('[', KEY_LEFT_BRACKET); // Same as <esc> in terminals.
      CONSUME_CHARACTER(']', KEY_RIGHT_BRACKET);
      #undef CONSUME_CHARACTER

      #define CONSUME_RANGE(c_min, c_max, key_min)                                         \
        if (c_min <= c && c <= c_max) {                                                    \
          Event::Keycode keycode = (Event::Keycode) (((int)Event::key_min) + (c - c_min)); \
          PUSH_EVENT(keycode, '\0');                                                       \
          if (*ptr++ != '>') return false;                                                 \
          continue;                                                                        \
        }

      // TODO: <S-f1><S-f12> is a possible, parse here.
      CONSUME_RANGE('0', '9', KEY_ZERO);
      CONSUME_RANGE('a', 'z', KEY_A);
      CONSUME_RANGE('A', 'Z', KEY_A);
      CONSUME_RANGE('A', 'Z', KEY_A);
      #undef CONSUME_RANGE

      continue;
    }

    // Capture single character.
    #define CONSUME_CHARACTER(character) \
      if (c == character) {              \
        PUSH_EVENT(Event::KEY_NULL, c);  \
        continue;                        \
      }
    CONSUME_CHARACTER('\'');
    CONSUME_CHARACTER(',');
    CONSUME_CHARACTER('-');
    CONSUME_CHARACTER('=');
    CONSUME_CHARACTER('.');
    CONSUME_CHARACTER('/');
    CONSUME_CHARACTER('\\');
    CONSUME_CHARACTER(';');
    CONSUME_CHARACTER('[');
    CONSUME_CHARACTER(']');
    CONSUME_CHARACTER('`');
    #undef CONSUME_CHARACTER


    #define CONSUME_RANGE(c_min, c_max) \
      if (c_min <= c && c <= c_max) {   \
        PUSH_EVENT(Event::KEY_NULL, c); \
        continue;                       \
      }
      // TODO: f1-f12 is a possible, parse here.
      CONSUME_RANGE('0', '9');
      CONSUME_RANGE('a', 'z');
      CONSUME_RANGE('A', 'Z');
      CONSUME_RANGE('A', 'Z');
    #undef CONSUME_RANGE
  }

  return true;
}

#undef STARTS_WITH
#undef PUSH_EVENT
#undef CONSUME

