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


std::string ReadAll(const std::string& path) {
  std::ifstream inputFile(path.data());
  assert(inputFile.is_open() && "Failed to open file.");
  std::stringstream buffer;
  buffer << inputFile.rdbuf();
  inputFile.close();
  return buffer.str();
}


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

