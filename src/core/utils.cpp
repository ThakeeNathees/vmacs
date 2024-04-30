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


void DrawTextLine(FrameBuffer buff, const char* text, int x, int y, int width, Color fg, Color bg, uint8_t attrib, bool fill_area) {
  if (text == NULL || *text == '\0') return;
  if (x < 0 || y < 0) return;
  if (x >= buff.width || y >= buff.height) return;
  if (x + width > buff.width) width = buff.width - x;
  if (width <= 0) return;

  // Current x position we're drawing.
  int curr = x;

  // FIXME: Move this somewhere general.
  // We draw an indicator that the text is trimmed with three dot unicode.
  // 0x2026 : '…' 
  int trim_indicator = 0x2026;

  int length = Utf8Strlen(text);
  int text_len = MIN(length, width);

  bool trimming = (length > width);
  if (trimming) text_len -= 1;

  int xcurr = x;
  const char* c = text;
  for (; xcurr < x + text_len; xcurr++) {
    uint32_t ch;
    c += Utf8CharToUnicode(&ch, c);
    if (ch == '\n' || ch == '\t') ch = ' ';
    SET_CELL(buff, xcurr, y, ch, fg, bg, attrib);
  }
  if (trimming) SET_CELL(buff, xcurr++, y, trim_indicator, fg, bg, attrib);
  if (fill_area) {
    while (xcurr < (x + width)) {
      SET_CELL(buff, xcurr++, y, ' ', fg, bg, attrib);
    }
  }
}


void DrawRectangleFill(FrameBuffer buff, int x, int y, int width, int height, Color bg) {
  if (x < 0 || y < 0) return;
  if (x >= buff.width || y >= buff.height) return;
  if (x + width > buff.width) width = buff.width - x;
  if (y + height > buff.height) height = buff.height - y;
  if (width <= 0) return;

  for (int row = y; row < y+height; row++) {
    for (int col = x; col < x+width; col++) {
      SET_CELL(buff, col, row, ' ', 0, bg, 0);
    }
  }
}


void DrawRectangleLine(FrameBuffer buff, int x, int y, int width, int height, Color fg, Color bg, bool fill) {
  if (x < 0 || y < 0) return;
  if (x >= buff.width || y >= buff.height) return;
  if (x + width > buff.width) width = buff.width - x;
  if (y + height > buff.height) height = buff.height - y;
  if (width <= 0) return;

  // FIXME: The box characters are hardcoded.
  // int TR = 0x256d; // Curved corners.
  // int TL = 0x256e;
  // int BR = 0x256f;
  // int BL = 0x2570;
  int TR = 0x250c; // Sharp corners.
  int TL = 0x2510;
  int BR = 0x2518;
  int BL = 0x2514;

  // horizontal, vertical lines.
  int HL = 0x2500;
  int VL = 0x2502;

  SET_CELL(buff, x,         y,          TR, fg, bg, 0); // Top right.
  SET_CELL(buff, x+width-1, y,          TL, fg, bg, 0); // Top left.
  SET_CELL(buff, x,         y+height-1, BL, fg, bg, 0); // Bottom left.
  SET_CELL(buff, x+width-1, y+height-1, BR, fg, bg, 0); // Bottom right.

  for (int col = x+1; col < x+width-1; col++) {
    SET_CELL(buff, col, y,          HL, fg, bg, 0); // Tob horizontal bar.
    SET_CELL(buff, col, y+height-1, HL, fg, bg, 0); // Bottom horizontal bar.
  }

  for (int row = y+1; row < y+height-1; row++) {
    SET_CELL(buff, x,         row, VL, fg, bg, 0); // Left vertical bar.
    SET_CELL(buff, x+width-1, row, VL, fg, bg, 0); // Right vertical bar.
  }

  if (fill) DrawRectangleFill(buff, x+1, y+1, width-2, height-2, bg);
}


void DrawHorizontalLine(FrameBuffer buff, int x, int y, int width, Color fg, Color bg) {
  if (x < 0 || y < 0) return;
  if (x >= buff.width || y >= buff.height) return;
  if (x + width > buff.width) width = buff.width - x;
  if (width <= 0) return;

  int HL = 0x2500;
  for (int col = x+1; col < x+width-1; col++) {
    SET_CELL(buff, col, y, HL, fg, bg, 0);
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


// ----------------------------------------------------------------------------
// Key combination parsing.
// ----------------------------------------------------------------------------

// Copied from: https://stackoverflow.com/a/42844629/10846399
bool EndsWith(StringView str, StringView suffix) {
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}


bool StartsWith(StringView str, StringView prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}


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
        if (CONSUME(ptr, "M-")) { key.alt   = true; continue; }
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

