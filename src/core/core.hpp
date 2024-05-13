//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

// -----------------------------------------------------------------------------
// Include required headers.
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include <optional>
#include <memory>

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <queue>

#include <functional>
#include <algorithm>
#include <regex>

#include <thread>
#include <atomic>
#include <future>

#include <nlohmann/json.hpp>
using Json = nlohmann::json;


// -----------------------------------------------------------------------------
// Common macros.
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


// -----------------------------------------------------------------------------
// Application specific macros.
// -----------------------------------------------------------------------------


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


// Attribute of the cells.
#define VMACS_CELL_BOLD      0x01
#define VMACS_CELL_UNDERLINE 0x02
#define VMACS_CELL_ITALIC    0x04
#define VMACS_CELL_REVERSE   0x08


// -----------------------------------------------------------------------------
// Core types, typedefs and structs.
// -----------------------------------------------------------------------------


// The language id specified by the LSP specification.
typedef std::string LanguageId;  // c, cpp, python, js, etc.
typedef std::string LspClientId; // clangd, ccls, pyright, etc.


// FIXME: The naming of the bellow is violating our naming convention but we
// already have a class called Event so this serialized event integre should be
// called something else.
//
// The key event is serialized into a single integer to be used in map keys and
// compact form where the ctrl/alt/shift are set as bits in the integer.
typedef uint32_t event_t;


// The color value is stored as an integer, it could be either an rgb (0xrrggbb)
// value or 256 color, depends on the context.
typedef uint32_t Color;


// The coordinate inside a buffer, we can't use row, col since the tab character
// span over multiple columns.
struct Coord {
  Coord(int line=0, int character=0) : line(line), character(character) {}
  int line;
  int character;
};


// TODO: Remove row, col and use x, y only. And the name Position is confusing
// with Coord, maybe change to understand better.
struct Position {
  Position(int x=0, int y=0) : x(x), y(y) {}
  union {
    int row;
    int y;
  };
  union {
    int x;
    int col;
  };
};


struct Slice {
  Slice(int start=0, int end=0) : start(start), end(end) {}
  int start;
  int end;
};


struct Area {
  Area(int width=0, int height=0) : width(width), height(height) {}
  int width;
  int height;
};


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


// The UI is considered as a grid of cells, each cell has foreground and
// background colors. The color values are rgb values structured as 0xrrggbb
// if you want the xterm-256 compatible single byte value. RgbToXterm() function
// defined in the utils.
typedef struct {
  uint32_t ch;     // Unicode codepoint.
  Color    fg;     // Foreground color.
  Color    bg;     // Background color.
  uint8_t  attrib; // Attribute of the cell. Set with VMACS_CELL_* macros.
} Cell;


// DrawBuffer will be provided by the front end which will be filled with the
// cell values, at the backend draw call which then will be displaied by the
// front end.
typedef struct {
  std::vector<Cell> cells;
  int width;
  int height;
} FrameBuffer;


// All the event enums here are copied from raylib, since the core should be
// independed from any frontend (reusable and not coupled with anything).
struct Event {

  typedef enum {
    KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    KEY_APOSTROPHE      = 39,       // Key: '
    KEY_COMMA           = 44,       // Key: ,
    KEY_MINUS           = 45,       // Key: -
    KEY_PERIOD          = 46,       // Key: .
    KEY_SLASH           = 47,       // Key: /
    KEY_ZERO            = 48,       // Key: 0
    KEY_ONE             = 49,       // Key: 1
    KEY_TWO             = 50,       // Key: 2
    KEY_THREE           = 51,       // Key: 3
    KEY_FOUR            = 52,       // Key: 4
    KEY_FIVE            = 53,       // Key: 5
    KEY_SIX             = 54,       // Key: 6
    KEY_SEVEN           = 55,       // Key: 7
    KEY_EIGHT           = 56,       // Key: 8
    KEY_NINE            = 57,       // Key: 9
    KEY_SEMICOLON       = 59,       // Key: ;
    KEY_EQUAL           = 61,       // Key: =
    KEY_A               = 65,       // Key: A | a
    KEY_B               = 66,       // Key: B | b
    KEY_C               = 67,       // Key: C | c
    KEY_D               = 68,       // Key: D | d
    KEY_E               = 69,       // Key: E | e
    KEY_F               = 70,       // Key: F | f
    KEY_G               = 71,       // Key: G | g
    KEY_H               = 72,       // Key: H | h
    KEY_I               = 73,       // Key: I | i
    KEY_J               = 74,       // Key: J | j
    KEY_K               = 75,       // Key: K | k
    KEY_L               = 76,       // Key: L | l
    KEY_M               = 77,       // Key: M | m
    KEY_N               = 78,       // Key: N | n
    KEY_O               = 79,       // Key: O | o
    KEY_P               = 80,       // Key: P | p
    KEY_Q               = 81,       // Key: Q | q
    KEY_R               = 82,       // Key: R | r
    KEY_S               = 83,       // Key: S | s
    KEY_T               = 84,       // Key: T | t
    KEY_U               = 85,       // Key: U | u
    KEY_V               = 86,       // Key: V | v
    KEY_W               = 87,       // Key: W | w
    KEY_X               = 88,       // Key: X | x
    KEY_Y               = 89,       // Key: Y | y
    KEY_Z               = 90,       // Key: Z | z
    KEY_LEFT_BRACKET    = 91,       // Key: [
    KEY_BACKSLASH       = 92,       // Key: '\'
    KEY_RIGHT_BRACKET   = 93,       // Key: ]
    KEY_GRAVE           = 96,       // Key: `
    // Function keys
    KEY_SPACE           = 32,       // Key: Space
    KEY_ESCAPE          = 256,      // Key: Esc
    KEY_ENTER           = 257,      // Key: Enter
    KEY_TAB             = 258,      // Key: Tab
    KEY_BACKSPACE       = 259,      // Key: Backspace
    KEY_INSERT          = 260,      // Key: Ins
    KEY_DELETE          = 261,      // Key: Del
    KEY_RIGHT           = 262,      // Key: Cursor right
    KEY_LEFT            = 263,      // Key: Cursor left
    KEY_DOWN            = 264,      // Key: Cursor down
    KEY_UP              = 265,      // Key: Cursor up
    KEY_PAGE_UP         = 266,      // Key: Page up
    KEY_PAGE_DOWN       = 267,      // Key: Page down
    KEY_HOME            = 268,      // Key: Home
    KEY_END             = 269,      // Key: End
    KEY_CAPS_LOCK       = 280,      // Key: Caps lock
    KEY_SCROLL_LOCK     = 281,      // Key: Scroll down
    KEY_NUM_LOCK        = 282,      // Key: Num lock
    KEY_PRINT_SCREEN    = 283,      // Key: Print screen
    KEY_PAUSE           = 284,      // Key: Pause
    KEY_F1              = 290,      // Key: F1
    KEY_F2              = 291,      // Key: F2
    KEY_F3              = 292,      // Key: F3
    KEY_F4              = 293,      // Key: F4
    KEY_F5              = 294,      // Key: F5
    KEY_F6              = 295,      // Key: F6
    KEY_F7              = 296,      // Key: F7
    KEY_F8              = 297,      // Key: F8
    KEY_F9              = 298,      // Key: F9
    KEY_F10             = 299,      // Key: F10
    KEY_F11             = 300,      // Key: F11
    KEY_F12             = 301,      // Key: F12
    KEY_LEFT_SHIFT      = 340,      // Key: Shift left
    KEY_LEFT_CONTROL    = 341,      // Key: Control left
    KEY_LEFT_ALT        = 342,      // Key: Alt left
    KEY_LEFT_SUPER      = 343,      // Key: Super left
    KEY_RIGHT_SHIFT     = 344,      // Key: Shift right
    KEY_RIGHT_CONTROL   = 345,      // Key: Control right
    KEY_RIGHT_ALT       = 346,      // Key: Alt right
    KEY_RIGHT_SUPER     = 347,      // Key: Super right
    KEY_KB_MENU         = 348,      // Key: KB menu
    // Keypad keys
    KEY_KP_0            = 320,      // Key: Keypad 0
    KEY_KP_1            = 321,      // Key: Keypad 1
    KEY_KP_2            = 322,      // Key: Keypad 2
    KEY_KP_3            = 323,      // Key: Keypad 3
    KEY_KP_4            = 324,      // Key: Keypad 4
    KEY_KP_5            = 325,      // Key: Keypad 5
    KEY_KP_6            = 326,      // Key: Keypad 6
    KEY_KP_7            = 327,      // Key: Keypad 7
    KEY_KP_8            = 328,      // Key: Keypad 8
    KEY_KP_9            = 329,      // Key: Keypad 9
    KEY_KP_DECIMAL      = 330,      // Key: Keypad .
    KEY_KP_DIVIDE       = 331,      // Key: Keypad /
    KEY_KP_MULTIPLY     = 332,      // Key: Keypad *
    KEY_KP_SUBTRACT     = 333,      // Key: Keypad -
    KEY_KP_ADD          = 334,      // Key: Keypad +
    KEY_KP_ENTER        = 335,      // Key: Keypad Enter
    KEY_KP_EQUAL        = 336,      // Key: Keypad =
    // Android key buttons
    KEY_BACK            = 4,        // Key: Android back button
    KEY_MENU            = 82,       // Key: Android menu button
    KEY_VOLUME_UP       = 24,       // Key: Android volume up button
    KEY_VOLUME_DOWN     = 25        // Key: Android volume down button
  } Keycode;

  // Mouse buttons
  typedef enum {
    MOUSE_BUTTON_LEFT    = 0,   // Mouse button left
    MOUSE_BUTTON_RIGHT   = 1,   // Mouse button right
    MOUSE_BUTTON_MIDDLE  = 2,   // Mouse button middle (pressed wheel)

    // Not supporting these buttons from (raylib). Uncomment if needed.
    // MOUSE_BUTTON_SIDE    = 3,   // Mouse button side (advanced mouse device)
    // MOUSE_BUTTON_EXTRA   = 4,   // Mouse button extra (advanced mouse device)
    // MOUSE_BUTTON_FORWARD = 5,   // Mouse button forward (advanced mouse device)
    // MOUSE_BUTTON_BACK    = 6,   // Mouse button back (advanced mouse device)

    // Additional buttons apart from what raylib provide (they use different
    // event for wheel). Modify the event if raylib used as a frontend.
    MOUSE_WHEEL_UP       = 7,
    MOUSE_WHEEL_DOWN     = 8,
    MOUSE_RELEASED       = 9,
  } MouseButton;

  struct Resize {
    unsigned int width;
    unsigned int height;
  };

  // Note that it should be either unicode, or normal key. And if unicode is set
  // the reset of the values should be 0 (ignored otherwise). and if unicode is
  // non zero key code will be evaluvated.
  struct Key {
    int unicode;

    Keycode code;
    bool alt;
    bool ctrl;
    bool shift;
  };

  struct Mouse {
    MouseButton button;
    int x;
    int y;
  };

  enum class Type {
    CLOSE,
    RESIZE,
    KEY,
    MOUSE,
  };

  Type type;
  bool handled = false;

  union {
    Resize     resize;
    Key        key;
    Mouse      mouse;
  };

  Event(Event::Type type) : type(type), key(Key()) {}

};


// TODO: This sould be a json object / lua table where the user can "register"
// wheever they want and and use in some plugins.
//
// The editors global configuration.
struct Config {

  // How many spaces should be used to render a tab character. Note that this
  // value should be at least 1.
  int tabsize = 4;
  int scrolloff = 5; // Margin between the cursor and the view edge (vertical).
  int fps = 30;
  std::string theme = "dark_plus";

  // TODO: Create a map of filename to id and filename to lsp client.
  //
  // filename_to_lang : {
  //   "*.cpp"          : "cpp",
  //   "CMakeLists.txt" : "cmake",
  //   "*.txt"          : "txt",
  // }
  //
  LanguageId GetLangIdFromFileName(const std::string& filename) const;
  LspClientId GetLspClientIdFromFileName(const std::string& filename) const;
};


// -----------------------------------------------------------------------------
// Thread Safe Queue.
// -----------------------------------------------------------------------------


template <class T>
class ThreadSafeQueue {

public:
  void Enqueue(const T& t) {
    std::lock_guard<std::mutex>lock(mutex);
    queue.push(t);
    cond.notify_one();
  }


  T Dequeue() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.empty()) {
      cond.wait(lock);
    }
    T value = queue.front();
    queue.pop();
    return value;
  }


  bool Empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
  }


private:
  std::queue<T> queue;
  mutable std::mutex mutex;
  std::condition_variable cond;
};

// -----------------------------------------------------------------------------
// KeyTree.
// -----------------------------------------------------------------------------


// Use This macros on ActionExecutor classes to define the GetClassName method.
#define DEFINE_GET_CLASS_NAME(T)               \
  public:                                      \
  static const std::string& ClassName() {      \
    static const std::string& class_name = #T; \
    return class_name;                         \
  }                                            \
                                               \
  const std::string& GetClassName() const override { \
    return ClassName();                        \
  }                                            \
  private:


// Actions are functions which are "bindable" to keys, and associated with a
// key/key combination that invoke the action. The function should return true
// when the event is consumed and no need to propegate to the other listeners.

class ActionExecutor {
public:
  virtual ~ActionExecutor() = default;
  virtual const std::string& GetClassName() const = 0;

  const std::string& GetMode() const;
  void SetMode(const std::string& mode);

protected:
  std::string mode;
};


// Just to make the action struct more readable. No need to create structs and
// make it more complicated.
typedef std::string ActExName;
typedef std::string ModeName;
typedef std::string ActionName;
typedef std::pair<ActExName, ModeName> BindingKey;
typedef std::pair<ActExName, ActionName> ActionKey;
typedef bool (*FuncAction)(ActionExecutor*);


// The tree acts like a Trie for Events to handle sequence events.
class KeyTree {
public:

  // A Node in the KeyTree (trie). Contains a map of child nodes and a map of
  // actions mapped from mode. The action for a specific key combination can
  // change depend on the mode it's in.
  struct Node {
    std::map<event_t, std::unique_ptr<Node>> children;
    std::map<BindingKey, FuncAction> bindings;
  };

  KeyTree();

  // Before calling register binding, the action must be registered with the given name.
  void RegisterAction(const ActExName& class_name, const ActionName& action_name, FuncAction action);

  // If registered without specifing a mode an empty string will be used as the mode.
  void RegisterBinding(const ActExName& actex_name, const std::string& key_combination, const std::string& action_name);
  void RegisterBinding(const ActExName& actex_name, const ModeName& mode, const std::string& key_combination, const std::string& action_name);

private:
  // A registry of actions associated with it's name.
  std::map<ActionKey, FuncAction> actions;
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

  // When an event is encountered we feed that event to the keytree here to
  // update it's internal cursor, this will return true if the event is consumed.
  //
  // Note that It'll take an actex instance and check if the event can be further
  // consumed by the given actex instance, and if not it'll return false. If the
  // actex is nullptr it won't crash but returns false.
  bool ConsumeEvent(ActionExecutor* actex, const Event& event);

  // Try to execute action at the current node on the give acetion executor, returns
  // true if an even is bind to the current node and run action was success otherwise
  // false.
  bool TryEvent(ActionExecutor* actex);

  // Returns true if the current node we're in has more bindings to listen, This
  // is usefull to check if the current event is not consumed by any handlers
  // we know that there are more bindings along the tree.
  bool HasMore() const;

  // Clear all the tracks of cursor and set back to the root node.
  void ResetCursor();

  // Returns true if the cursor is at the root node and not listening to any key
  // sequences at the moment.
  bool IsCursorRoot() const;

private:
  const KeyTree* tree = nullptr;   // The tree we're traversing.
  KeyTree::Node* node;             // Node we're currenly in initially the root.

  std::vector<event_t> recorded_events; // Previous events which leads here.

private:
  // Returns true if the actex+mode can further consume the event from the given node.
  bool CanConsume(const BindingKey& key, KeyTree::Node* curr) const;
};


// -----------------------------------------------------------------------------
// Theme.
// -----------------------------------------------------------------------------


class Icons {
public:
  // Sharp corners.
  int tr = 0x250c; // ┌
  int tl = 0x2510; // ┐
  int br = 0x2518; // ┘
  int bl = 0x2514; // └

  // Curved corners.
  // int tr = 0x256d; // ╭
  // int tl = 0x256e; // ╮
  // int br = 0x256f; // ╯
  // int bl = 0x2570; // ╰

  // horizontal, vertical lines.
  int hl = 0x2500; // ─
  int vl = 0x2502; // │

  int scrollbar = 0x2590; // ▐

  int trim_indicator = 0x2026; // …
  int whitespace_tab = 0x2192; // →

  // Ui Related icons.
  int empty_file   = 0xf15b;  // 
  int find         = 0xe68f;  // 
  int find_in_file = 0xf021e; // 󰈞
  int textbox      = 0xf021a; // 󰈚
  int Bookmark     = 0xf02e;  // 
  int palette      = 0xe22b;  // 
  int pin          = 0xf0403; // 󰐃

  int angle_up     = 0xf106;  // 
  int angle_down   = 0xf107;  // 
  int angle_left   = 0xf104;  // 
  int angle_right  = 0xf105;  // 

  // TODO: The count 25 is hardcoded which is got from the LSP specification
  // as of May of 2024.
  int completion_kind[25] = {
    0xea93,  //  Text          
    0xea8c,  //  Method        
    0xf0295, //  Function      󰊕
    0xea8c,  //  Constructor   
    0xeb5f,  //  Field         
    0xea88,  //  Variable      
    0xeb5b,  //  Class         
    0xeb61,  //  Interface     
    0xea8b,  //  Module        
    0xeb65,  //  Property      
    0xea96,  //  Unit          
    0xea95,  //  Value         
    0xea95,  //  Enum          
    0xeb62,  //  Keyword       
    0xeb66,  //  Snippet       
    0xeb5c,  //  Color         
    0xea7b,  //  File          
    0xea94,  //  Reference     
    0xea83,  //  Folder        
    0xea95,  //  EnumMember    
    0xeb5d,  //  Constant      
    0xea91,  //  Struct        
    0xea86,  //  Event         
    0xeb64,  //  Operator      
    0xea92,  //  TypeParameter 
  };

  int brail_spinning_wheel[8] = {
    0x28bf, // ⢿
    0x28fb, // ⣻
    0x28fd, // ⣽
    0x28fe, // ⣾
    0x28f7, // ⣷
    0x28ef, // ⣯
    0x28df, // ⣟
    0x287f, // ⡿
  };

};


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
// Abstract Frontend and Ui interface.
// ----------------------------------------------------------------------------


// The abstract class representing the front end. Any front end implementation
// should be able to provide events and handle drawings.
class IFrontEnd {
public:
  virtual bool Initialize() = 0;
  virtual bool Cleanup() = 0;

  virtual std::vector<Event> GetEvents() = 0;

  // The front end should own a draw buffer which needs to be passed to the
  // backend by the bellow method. We'll be fill the buffer with the cells and
  // call the display method, that'll display the filled buffer. Note that
  // the display takes a cleaer color argument which needed to clear the color
  // outside of the grid otherwise the color outside gride will be different and
  // also raylib requires a clear_color swap a new buffer and start drawing.
  virtual FrameBuffer& GetDrawBuffer() = 0;

  // TODO: Remove the clear color and get the color for raylib from the theme
  // config or somewhere.
  virtual void Display() = 0; // Display the drawbuffer.

  virtual ~IFrontEnd() = default;
};


// An abstract interface for the Ui class defined in the ui directory. A
// Ui is simply something that should be able to handle events, update each
// iteration and draw itself on the editor.
class IUi {
public:
  virtual ~IUi() = default;

  virtual bool HandleEvent(const Event& event) = 0;
  virtual void Update() = 0;
  virtual void Draw(FrameBuffer& buff) = 0;
};


// ----------------------------------------------------------------------------
// Util function definitions.
// ----------------------------------------------------------------------------


unsigned constexpr ConstHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) + 33 * ConstHash(input + 1) : 5381;
}


std::size_t constexpr operator "" _hash(const char* s, size_t) {
  return ConstHash(s);
}

// Returns the Elapsed time since the program starts in milliseconds.
int GetElapsedTime();

// Color vaule conversion functions.
uint8_t RgbToXterm(uint32_t rgb);
uint32_t XtermToRgb(uint8_t xterm);

// String related util functions.
bool IsCharName(int c); // Returns the given codepoint [a-zA-Z0-9_].
bool IsCharWhitespace(int c); // Returns the given codepoint [\t\n' '].
bool EndsWith(std::string_view str, std::string_view suffix);
bool StartsWith(std::string_view str, std::string_view suffix);
std::vector<std::string> StringSplit(const std::string& str, char delim);

// UTF8 helper functions (Borrowed from termbox, see thridparty/termbox/LICENSE).
int Utf8CharLength(char c);
int Utf8CharToUnicode(uint32_t *out, const char *c);
int Utf8UnicodeToChar(char *out, uint32_t c);
int Utf8Strlen(const char* str);
std::string Utf8UnicodeToString(uint32_t c);

// Draw a NULL terminated text (utf8) to the specified position and width.
void DrawTextLine(
    FrameBuffer& buff,
    const char* text,
    Position pos,
    int width,          // If the text goes beyond the width it'll terminate.
    Style style,
    const Icons& icons, // Needed to draw text terminated indicator.
    bool fill_area,     // If true all the width is filled with the bg othereise only the text is drawn with the given bg.
    bool tail=false);   // If the text is too long it'll draw the tail of the text.

// Draws an icon which is a single codepoint.
void DrawIcon(FrameBuffer& buff, int icon, Position pos, Style style);

void DrawRectangleFill(FrameBuffer& buff, Position pos, Area area, Style style);
void DrawRectangleLine(FrameBuffer& buff, Position pos, Area area, Style style, const Icons& icons, bool fill);
void DrawHorizontalLine(FrameBuffer& buff, Position pos, int width, Style style, const Icons& icons);
void DrawVerticalLine(FrameBuffer& buff, Position pos, int height, Style style, const Icons& icons);


// On a successfull parse, it'll return true and push all the keys are parsed
// from that string.
//
// - Note that it'll not clear the events vector, but just push along with
//   whatever is already in the vector, so make sure it's empty before calling.
//
// - Note that an empty string is valid here and result empty events.
//
bool ParseKeyBindingString(std::vector<event_t>& events, const char* binding);

// Event encoding/decoding functions.
event_t EncodeKeyEvent(Event::Key key);
Event::Key DecodeKeyEvent(event_t key);

// TODO: This maybe ugly, refactor.
//
// Since Config is needed allover the source even where we cannot access Editor
// singleton, so this "Global" function will returns the editor singleton's config.
Config& GetConfig();
