//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <vmacs.hpp>

// Commonly used standard libraries.
#include <stdio.h>
#include <stdlib.h>

#include <optional>
#include <memory>

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <queue>

#include <algorithm>
#include <regex>

#include <thread>
#include <atomic>
#include <future>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;

#include "thread_safe_queue.hpp"


// -----------------------------------------------------------------------------
// Common macros and typedefs.
// -----------------------------------------------------------------------------


#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define BETWEEN(a, b, c) ((a) <= (b) && (b) <= (c))

#define CLAMP(a, b, c) \
  (b) < (a)            \
  ? (a)                \
  : ((b) > (c)         \
     ? (c)             \
     : (b))


#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#define NO_OP do {} while (false)

// CONCAT_LINE(X) will result evaluvated X<__LINE__>.
#define __CONCAT_LINE_INTERNAL_R(a, b) a ## b
#define __CONCAT_LINE_INTERNAL_F(a, b) __CONCAT_LINE_INTERNAL_R(a, b)
#define CONCAT_LINE(X) __CONCAT_LINE_INTERNAL_F(X, __LINE__)


// The internal assertion macro, this will print error and break regardless of
// the build target (debug or release). Use ASSERT() for debug assertion and
// use __ASSERT() for TODOs.
#define __ASSERT(condition, message)                                 \
  do {                                                               \
    if (!(condition)) {                                              \
      fprintf(stderr, "Assertion failed: %s\n\tat %s() (%s:%i)\n"    \
                      "\tcondition: %s",                             \
        message, __func__, __FILE__, __LINE__, #condition);          \
      DEBUG_BREAK();                                                 \
      abort();                                                       \
    }                                                                \
  } while (false)


// Using __ASSERT() for make it crash in release binary too.
#define TODO __ASSERT(false, "TODO: It hasn't implemented yet.")
#define OOPS "Oops a bug!! report please."


#ifdef DEBUG

#ifdef _MSC_VER
  #define DEBUG_BREAK() __debugbreak()
#else
  #define DEBUG_BREAK() __builtin_trap()
#endif

// This will terminate the compilation if the [condition] is false, because of
// char _assertion_failure_<__LINE__>[-1] evaluated.
#define STATIC_ASSERT(condition) \
  static char CONCAT_LINE(_assertion_failure_)[2*!!(condition) - 1]

#define ASSERT(condition, message) __ASSERT(condition, message)

#define ASSERT_INDEX(index, size) \
  ASSERT(index >= 0 && index < size, "Index out of bounds.")

#define UNREACHABLE()                                                \
  do {                                                               \
    fprintf(stderr, "Execution reached an unreachable path\n"        \
      "\tat %s() (%s:%i)\n", __func__, __FILE__, __LINE__);          \
    DEBUG_BREAK();                                                   \
    abort();                                                         \
  } while (false)

#else // #ifdef DEBUG


#define STATIC_ASSERT(condition) NO_OP

#define DEBUG_BREAK() NO_OP
#define ASSERT(condition, message) NO_OP
#define ASSERT_INDEX(index, size) NO_OP

#if defined(_MSC_VER)
  #define UNREACHABLE() __assume(0)
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
  #define UNREACHABLE() __builtin_unreachable()
#elif defined(__EMSCRIPTEN__) || defined(__clang__)
  #if __has_builtin(__builtin_unreachable)
    #define UNREACHABLE() __builtin_unreachable()
  #else
    #define UNREACHABLE() NO_OP
  #endif
#else
  #define UNREACHABLE() NO_OP
#endif

#endif // #ifdef DEBUG


// This will mark a class non-copiable by deleting the copy constructor and
// copy assignment.
#define NO_COPY_CONSTRUCTOR(T) \
  T (const T&) = delete; \
  T operator= (const T&) = delete


#define BUFF_CELL(buff, x, y)\
  (buff).cells[ (buff).width * ((y)) + (x)  ]

// FIXME: make this as a function and make sure x, y are in the bounds.
#define SET_CELL(buff, x, y, c, style)             \
  do {                                             \
    Cell& cell  = BUFF_CELL((buff), (x), (y));     \
    cell.ch     = (c);                             \
    cell.fg     = (style).fg.value_or(0x000000);   \
    cell.bg     = (style).bg.value_or(0xffffff);   \
    cell.attrib = (style).attrib;                  \
  } while (false)

// FIXME: Make this a function and overload the above with this.
#define SET_CELL_I(buff, i, c, style)              \
  do {                                             \
    Cell& cell  = (buff).cells[i];                 \
    cell.ch     = (c);                             \
    cell.fg     = (style).fg.value_or(0x000000);   \
    cell.bg     = (style).bg.value_or(0xffffff);   \
    cell.attrib = (style).attrib;                  \
  } while (false)


// The key event is serialized into a single integer to be used in map keys and
// compact form where the ctrl/alt/shift are set as bits in the integer.
typedef uint32_t event_t;

// TODO:
// This is incomplete as there are resize events and mouse events. Also I
// need to re-consider that in key events, only unicode or keycode exists at a
// time so we don't need more space for storing both at the same time.
//
// Largest key code we use is 336 whish will fit into 9 bits so we're using 10
// bits for the keycode here.
//
//   0000 0000 0000 0000   0000 0000 0000 0000
//             ^       ^      ^ ^^^-- keycode  mask = 0x3ff, shift = 0
//             '-------'      | |`---- ctrl    mask = 0x400
//                 |          | '----- alt     mask = 0x800
//                 |          '------- shift   mask = 0x1000
//                 '------------------ ascii   mask = 0xff0000, shift = 16
//
#define KEY_CODE(code, ascii, ctrl, alt, shift) \
  Event::Keycode::code | \
  (ctrl?0x400:0) | (alt?0x800:0) | (shift?0x1000:0) | ((ascii & 0xff) << 13)


// The coordinate inside a buffer, we can't use row, col since the tab character
// span over multiple columns.
typedef struct {
  int line;
  int character;
} Coord;


typedef struct {
  int row;
  int col;
} Position;


typedef struct {
  int start;
  int end;
} Slice;


typedef struct {
  int width;
  int height;
} Size;


// Style is a foreground, background color and attribute (bold, italic, etc).
// Each capture "keyword", "constant", "string-literal", has it's own style.
// which we'll use to highlight a buffer.
struct Style {
  std::optional<Color> fg;
  std::optional<Color> bg;
  uint8_t attrib = 0;

  // Apply this style with the given style, which will override the fg, bg and
  // bitwise or the attribute.
  Style Apply(const Style& other) const;
  void ApplyInplace(const Style& other);
};

// -----------------------------------------------------------------------------
// KeyTree.
// -----------------------------------------------------------------------------

// Actions are functions which are "bindable" to keys, and associated with a
// key/key combination that invoke the action. The function should return true
// when the event is consumed and no need to propegate to the other listeners.
class EventHandler;
typedef bool (*FuncAction)(EventHandler*);


// The tree acts like a Trie for Events to handle sequence events.
class KeyTree {
public:

  // A Node in the KeyTree (trie). Contains a map of child nodes and a map of
  // actions mapped from mode. The action for a specific key combination can
  // change depend on the mode it's in.
  struct Node {
    std::map<event_t, std::unique_ptr<Node>> children;
    std::map<std::string, FuncAction> actions;
  };

  KeyTree();

  // Before calling register binding, the action must be registered with the given name.
  void RegisterAction(const std::string& action_name, FuncAction action);
  void RegisterBinding(const std::string& mode, const std::string& key_combination, const std::string& action_name);

private:
  // A registry of actions associated with it's name.
  std::unordered_map<std::string, FuncAction> actions;
  std::unique_ptr<Node> root; // Root node of the tree.

  // Cursor can access private things so I don't have to have public getter and
  // setter for others.
  friend class KeyTreeCursor;
};


// KeyTreeCursor is a statefull object that'll keep track of where we're in
// while processing a key combination.
class KeyTreeCursor {
public:
  KeyTreeCursor(const KeyTree* tree);
  void SetMode(const std::string& mode);

  // Try to execute the event with the bindings, if success returns true.
  bool TryEvent(EventHandler* handler, const Event& event);

private:
  const KeyTree* tree = nullptr; // The tree we're traversing.
  std::string mode;              // Current mode we're in.
  KeyTree::Node* node;           // Node we're currenly in initially the root.

  std::vector<event_t> recorded_events; // Previous events which leads here.

private:
  // Clear all the tracks of cursor and set back to the root node.
  void ResetCursor();

  // Returns true if the cursor is at the root node and not listening to any key
  // sequences at the moment.
  bool IsCursorRoot() const;

  // When an event is encountered we feed that event to the keytree here to
  // update it's internal cursor, this will return the action for this event
  // if exists otherwise nullptr.
  //
  // It'll set the boolean more true if there are more sequence of keys down the
  // tree starting with the current sequence. The calling function can decide
  // what to do about the returned action and what to do with the reset of the
  // sequence (e.g. use a timeout).
  FuncAction ConsumeEvent(event_t event, bool* more);
};


// An abstract definition of classes that can register events to key tree and
// execute with keytreecursor.
class EventHandler {
public:
  EventHandler(const KeyTree* keytree);
  virtual ~EventHandler() = default;

  // It'll try execute the event with the cursor and return the result.
  virtual bool HandleEvent(const Event& event);

  void SetMode(const std::string& mode);

private:
  // This supposed to be the static key tree registry of the class of this
  // isntance. Will be initialized at the constructor.
  const KeyTree* keytree = nullptr;
  KeyTreeCursor cursor;
};


// -----------------------------------------------------------------------------
// Theme.
// -----------------------------------------------------------------------------

class Theme {

public:

  // Load the theme from a json object, unlike "Helix" editor (which is where
  // we've "borrowed" the syntax highlighting and themes) this json doesn't have
  // inherits, if so that needs to be resolved beforehand by the caller.
  Theme(const Json& json);

  // Returns the Style for the given capture. For a key like
  // "keyword.control.conditional" if not exists, we again search for
  // "keyword.control" and then "keyword". If found it'll return true and the
  // style parameter will be updated, otherwise, return false. If the capture
  // doesn't exists the return style will be empty.
  Style GetStyle(const std::string& capture) const;

  // Converts a hex string of color values and returns as the equelevent numeric value.
  // as 0x00rrggbb value.
  static bool StringToColor(const char* str, Color* rgb);

private:
  // An entry is a pair of capture name ("keyword", "constant", "string-literal")
  // and the style for that cpature.
  std::map<std::string, Style> entries;

};


// ----------------------------------------------------------------------------
// Util function definitions.
// ----------------------------------------------------------------------------


// TODO: Define a Config class.
#define TABSIZE_ 4


unsigned constexpr ConstHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) + 33 * ConstHash(input + 1) : 5381;
}


std::size_t constexpr operator "" _hash(const char* s, size_t) {
  return ConstHash(s);
}


// Returns the Elapsed time since the program starts in milliseconds.
int GetElapsedTime();

bool IsCharName(int c); // Returns the given codepoint [a-zA-Z0-9_].
bool IsCharWhitespace(int c);
bool EndsWith(std::string_view str, std::string_view suffix);
bool StartsWith(std::string_view str, std::string_view suffix);

uint8_t RgbToXterm(uint32_t rgb);
uint32_t XtermToRgb(uint8_t xterm);

event_t EncodeKeyEvent(Event::Key key);
Event::Key DecodeKeyEvent(event_t key);

// UTF8 helper functions (Borrowed from termbox, see thridparty/termbox/LICENSE).
int Utf8CharLength(char c);
int Utf8CharToUnicode(uint32_t *out, const char *c);
int Utf8UnicodeToChar(char *out, uint32_t c);
int Utf8Strlen(const char* str);

// Draw a NULL terminated text (utf8) to the specified position and width.
void DrawTextLine(
    FrameBuffer buff,
    const char* text,
    int x,
    int y,
    int width,       // If the text goes beyond the width it'll terminate.
    Style style,
    bool fill_area); // If true all the width is filled with the bg othereise
                     // only the text is drawn with the given bg.


void DrawRectangleFill(FrameBuffer buff, int x, int y, int width, int height, Style style);
void DrawRectangleLine(FrameBuffer buff, int x, int y, int width, int height, Style style, bool fill=false);
void DrawHorizontalLine(FrameBuffer buff, int x, int y, int width, Style style);
void DrawVerticalLine(FrameBuffer buff, int x, int y, int height, Style style);


// On a successfull parse, it'll return true and push all the keys are parsed
// from that string.
//
// - Note that it'll not clear the events vector, but just push along with
//   whatever is already in the vector, so make sure it's empty before calling.
//
// - Note that an empty string is valid here and result empty events.
//
bool ParseKeyBindingString(std::vector<event_t>& events, const char* binding);


