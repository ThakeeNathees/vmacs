//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <vector>
#include <string>
#include <functional>

// Actions are functions which are "bindable" to keys, and associated with a
// key/key combination that invoke the action.
typedef std::function<void()> FuncAction;


typedef uint32_t Color;

// Attribute of the cells.
#define VMACS_CELL_BOLD      0x01
#define VMACS_CELL_UNDERLINE 0x02
#define VMACS_CELL_ITALIC    0x04
#define VMACS_CELL_REVERSE   0x08


// The UI is considered as a grid of cells, each cell has foreground and
// background colors. The color values are rgb values structured as 0xrrggbb
// if you want the xterm-256 compatible single byte value. RgbToXterm() function
// defined bellow.
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
  Cell* cells;
  int width;
  int height;
} DrawBuffer;


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
    MOUSE_BUTTON_SIDE    = 3,   // Mouse button side (advanced mouse device)
    MOUSE_BUTTON_EXTRA   = 4,   // Mouse button extra (advanced mouse device)
    MOUSE_BUTTON_FORWARD = 5,   // Mouse button forward (advanced mouse device)
    MOUSE_BUTTON_BACK    = 6,   // Mouse button back (advanced mouse device)
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

  struct MouseWheel {
    int delta;
    int x;
    int y;
  };

  enum class Type {
    CLOSE,
    RESIZE,
    KEY,
    WHEEL,
    MOUSE,
  };

  Type type;
  bool handled = false;

  union {
    Resize     resize;
    Key        key;
    Mouse      mouse;
    MouseWheel mouse_wheel;
  };

  Event(Event::Type type) : type(type), key(Key()) {}

};


// The abstract class representing the front end. Any front end implementation
// should be able to provide events and handle drawings.
class FrontEnd {
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
  virtual DrawBuffer GetDrawBuffer() = 0;
 
  // TODO: Remove the clear color and get the color for raylib from the theme
  // config or somewhere.
  virtual void Display(uint32_t clear_color) = 0; // Display the drawbuffer.

  virtual ~FrontEnd() = default;
};


class IEditor {

public:
  static std::shared_ptr<IEditor> Singleton();

  virtual void SetFrontEnd(std::unique_ptr<FrontEnd> frontend) = 0;

  // The Main loop of our application, this is blocking. If you integrate this
  // into your own application, run this in a different thread.
  virtual int MainLoop() = 0;

  virtual ~IEditor() = default;

  // Utility functions.
  static uint8_t RgbToXterm(uint32_t rgb);
  static uint32_t XtermToRgb(uint8_t xterm);
};


