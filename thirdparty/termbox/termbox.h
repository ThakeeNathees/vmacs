#ifndef TERMBOX_H
#define TERMBOX_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The following compile-time options are supported:
 *
 *     TB_OPT_ATTR_W: Integer width of fg and bg attributes. Valid values
 *                    (assuming system support) are 16, 32, and 64. (See
 *                    uintattr_t). 32 or 64 enables output mode
 *                    TB_OUTPUT_TRUECOLOR. 64 enables additional style
 *                    attributes. (See tb_set_output_mode.) Larger values
 *                    consume more memory in exchange for more features.
 *                    Defaults to 16.
 *
 * TB_OPT_PRINTF_BUF: Write buffer size for printf operations. Represents the
 *                    largest string that can be sent in one call to tb_print*
 *                    and tb_send* functions. Defaults to 4096.
 *
 *   TB_OPT_READ_BUF: Read buffer size for tty reads. Defaults to 64.
 */

 // Ensure consistent compile-time options when using as a shared library
#if defined(TB_LIB_OPTS) || 0 // __tb_lib_opts
  #undef TB_OPT_ATTR_W
  #undef TB_OPT_PRINTF_BUF
  #undef TB_OPT_READ_BUF
  #define TB_OPT_ATTR_W 64
#endif

// Ensure sane TB_OPT_ATTR_W (16, 32, or 64)
#if defined TB_OPT_ATTR_W && TB_OPT_ATTR_W == 16
#elif defined TB_OPT_ATTR_W && TB_OPT_ATTR_W == 32
#elif defined TB_OPT_ATTR_W && TB_OPT_ATTR_W == 64
#else
  #undef TB_OPT_ATTR_W
  #define TB_OPT_ATTR_W 16
#endif

/* Define these to swap in a different allocator */
#ifndef tb_malloc
#define tb_malloc  malloc
#define tb_realloc realloc
#define tb_free    free
#endif

/* Define this to set the size of the buffer used in tb_printf()
 * and tb_sendf()
 */
#ifndef TB_OPT_PRINTF_BUF
  #define TB_OPT_PRINTF_BUF 4096
#endif

#if TB_OPT_ATTR_W == 64
  typedef uint64_t uintattr_t;
#elif TB_OPT_ATTR_W == 32
  typedef uint32_t uintattr_t;
#else // 16
  typedef uint16_t uintattr_t;
#endif


/* Define this to set the size of the read buffer used when reading
 * from the tty.
 */
#ifndef TB_OPT_READ_BUF
  #define TB_OPT_READ_BUF 64
#endif

/* Input modes (bitwise) (tb_set_input_mode) */
#define TB_INPUT_CURRENT    0
#define TB_INPUT_ESC        1
#define TB_INPUT_ALT        2
#define TB_INPUT_MOUSE      4

/* Output modes (tb_set_output_mode) */
#define TB_OUTPUT_CURRENT   0
#define TB_OUTPUT_NORMAL    1
#define TB_OUTPUT_256       2
#define TB_OUTPUT_216       3
#define TB_OUTPUT_GRAYSCALE 4

#if TB_OPT_ATTR_W >= 32
  #define TB_OUTPUT_TRUECOLOR 5
#endif

/* Colors (numeric) and attributes (bitwise) (tb_cell.fg, tb_cell.bg) */
#define TB_DEFAULT              0x0000
#define TB_BLACK                0x0001
#define TB_RED                  0x0002
#define TB_GREEN                0x0003
#define TB_YELLOW               0x0004
#define TB_BLUE                 0x0005
#define TB_MAGENTA              0x0006
#define TB_CYAN                 0x0007
#define TB_WHITE                0x0008

/* ASCII key constants (tb_event.key) */
typedef enum {

  TB_KEY_CTRL_TILDE = 0x00,
  TB_KEY_CTRL_2 = 0x00,

  TB_KEY_CTRL_A = 0x01,
  TB_KEY_CTRL_B = 0x02,
  TB_KEY_CTRL_C = 0x03,
  TB_KEY_CTRL_D = 0x04,
  TB_KEY_CTRL_E = 0x05,
  TB_KEY_CTRL_F = 0x06,
  TB_KEY_CTRL_G = 0x07,

  TB_KEY_BACKSPACE = 0x08,
  TB_KEY_CTRL_H = 0x08,

  TB_KEY_TAB = 0x09,
  TB_KEY_CTRL_I = 0x09,

  TB_KEY_CTRL_J = 0x0a,
  TB_KEY_CTRL_K = 0x0b,
  TB_KEY_CTRL_L = 0x0c,

  TB_KEY_ENTER = 0x0d,
  TB_KEY_CTRL_M = 0x0d,

  TB_KEY_CTRL_N = 0x0e,
  TB_KEY_CTRL_O = 0x0f,
  TB_KEY_CTRL_P = 0x10,
  TB_KEY_CTRL_Q = 0x11,
  TB_KEY_CTRL_R = 0x12,
  TB_KEY_CTRL_S = 0x13,
  TB_KEY_CTRL_T = 0x14,
  TB_KEY_CTRL_U = 0x15,
  TB_KEY_CTRL_V = 0x16,
  TB_KEY_CTRL_W = 0x17,
  TB_KEY_CTRL_X = 0x18,
  TB_KEY_CTRL_Y = 0x19,
  TB_KEY_CTRL_Z = 0x1a,

  TB_KEY_ESC = 0x1b,
  TB_KEY_CTRL_LSQ_BRACKET = 0x1b,
  TB_KEY_CTRL_3 = 0x1b,

  TB_KEY_CTRL_4 = 0x1c,
  TB_KEY_CTRL_BACKSLASH = 0x1c,

  TB_KEY_CTRL_5 = 0x1d,
  TB_KEY_CTRL_RSQ_BRACKET = 0x1d,

  TB_KEY_CTRL_6 = 0x1e,

  TB_KEY_CTRL_7 = 0x1f,
  TB_KEY_CTRL_SLASH = 0x1f,
  TB_KEY_CTRL_UNDERSCORE = 0x1f,

  TB_KEY_SPACE = 0x20,

  TB_KEY_BACKSPACE2 = 0x7f,
  TB_KEY_CTRL_8 = 0x7f,

  #define tb_key_i(i)      0xffff - (i)
  /* Terminal-dependent key constants (tb_event.key) and terminfo capabilities */
  /* BEGIN codegen h */
  /* Produced by ./codegen.sh on Thu, 13 Jul 2023 05:46:13 +0000 */
  TB_KEY_F1 = (0xffff - 0),
  TB_KEY_F2 = (0xffff - 1),
  TB_KEY_F3 = (0xffff - 2),
  TB_KEY_F4 = (0xffff - 3),
  TB_KEY_F5 = (0xffff - 4),
  TB_KEY_F6 = (0xffff - 5),
  TB_KEY_F7 = (0xffff - 6),
  TB_KEY_F8 = (0xffff - 7),
  TB_KEY_F9 = (0xffff - 8),
  TB_KEY_F10 = (0xffff - 9),
  TB_KEY_F11 = (0xffff - 10),
  TB_KEY_F12 = (0xffff - 11),
  TB_KEY_INSERT = (0xffff - 12),
  TB_KEY_DELETE = (0xffff - 13),
  TB_KEY_HOME = (0xffff - 14),
  TB_KEY_END = (0xffff - 15),
  TB_KEY_PGUP = (0xffff - 16),
  TB_KEY_PGDN = (0xffff - 17),
  TB_KEY_ARROW_UP = (0xffff - 18),
  TB_KEY_ARROW_DOWN = (0xffff - 19),
  TB_KEY_ARROW_LEFT = (0xffff - 20),
  TB_KEY_ARROW_RIGHT = (0xffff - 21),
  TB_KEY_BACK_TAB = (0xffff - 22),
  TB_KEY_MOUSE_LEFT = (0xffff - 23),
  TB_KEY_MOUSE_RIGHT = (0xffff - 24),
  TB_KEY_MOUSE_MIDDLE = (0xffff - 25),
  TB_KEY_MOUSE_RELEASE = (0xffff - 26),
  TB_KEY_MOUSE_WHEEL_UP = (0xffff - 27),
  TB_KEY_MOUSE_WHEEL_DOWN = (0xffff - 28),
  /* END codegen h */
} tb_key;

typedef enum {
#if TB_OPT_ATTR_W == 16
  TB_BOLD = 0x0100,
  TB_UNDERLINE = 0x0200,
  TB_REVERSE = 0x0400,
  TB_ITALIC = 0x0800,
  TB_BLINK = 0x1000,
  TB_HI_BLACK = 0x2000,
  TB_BRIGHT = 0x4000,
  TB_DIM = 0x8000,
#else // 32 or 64
  TB_BOLD = 0x01000000,
  TB_UNDERLINE = 0x02000000,
  TB_REVERSE = 0x04000000,
  TB_ITALIC = 0x08000000,
  TB_BLINK = 0x10000000,
  TB_HI_BLACK = 0x20000000,
  TB_BRIGHT = 0x40000000,
  TB_DIM = 0x80000000,
#endif

#if TB_OPT_ATTR_W == 64
  TB_STRIKEOUT = 0x0000000100000000,
  TB_UNDERLINE_2 = 0x0000000200000000,
  TB_OVERLINE = 0x0000000400000000,
  TB_INVISIBLE = 0x0000000800000000,
#endif
} tb_attr;

// Since posix terminals are different we have the index of the caps here and
// depend on the terminal we initialize with the caps, for compatibility we define
// it here and windows will only have one mappping (I guess?).
#define TB_CAP_F1               0
#define TB_CAP_F2               1
#define TB_CAP_F3               2
#define TB_CAP_F4               3
#define TB_CAP_F5               4
#define TB_CAP_F6               5
#define TB_CAP_F7               6
#define TB_CAP_F8               7
#define TB_CAP_F9               8
#define TB_CAP_F10              9
#define TB_CAP_F11              10
#define TB_CAP_F12              11
#define TB_CAP_INSERT           12
#define TB_CAP_DELETE           13
#define TB_CAP_HOME             14
#define TB_CAP_END              15
#define TB_CAP_PGUP             16
#define TB_CAP_PGDN             17
#define TB_CAP_ARROW_UP         18
#define TB_CAP_ARROW_DOWN       19
#define TB_CAP_ARROW_LEFT       20
#define TB_CAP_ARROW_RIGHT      21
#define TB_CAP_BACK_TAB         22
#define TB_CAP__COUNT_KEYS      23
#define TB_CAP_ENTER_CA         23
#define TB_CAP_EXIT_CA          24
#define TB_CAP_SHOW_CURSOR      25
#define TB_CAP_HIDE_CURSOR      26
#define TB_CAP_CLEAR_SCREEN     27
#define TB_CAP_SGR0             28
#define TB_CAP_UNDERLINE        29
#define TB_CAP_BOLD             30
#define TB_CAP_BLINK            31
#define TB_CAP_ITALIC           32
#define TB_CAP_REVERSE          33
#define TB_CAP_ENTER_KEYPAD     34
#define TB_CAP_EXIT_KEYPAD      35
#define TB_CAP_DIM              36
#define TB_CAP_INVISIBLE        37
#define TB_CAP__COUNT           38

/* Event types (tb_event.type) */
typedef enum {
  TB_EVENT_KEY = 1,
  TB_EVENT_RESIZE = 2,
  TB_EVENT_MOUSE = 3,
} tb_event_type;

/* Key modifiers (bitwise) (tb_event.mod) */
typedef enum {
  TB_MOD_ALT = 1,
  TB_MOD_CTRL = 2,
  TB_MOD_SHIFT = 4,
  TB_MOD_MOTION = 8,
} tb_mod;

/* Common function return values unless otherwise noted.
 *
 * Library behavior is undefined after receiving TB_ERR_MEM. Callers may
 * attempt reinitializing by freeing memory, invoking tb_shutdown, then
 * tb_init.
 */
typedef enum {
  TB_OK = 0,
  TB_ERR = -1,
  TB_ERR_NEED_MORE = -2,
  TB_ERR_INIT_ALREADY = -3,
  TB_ERR_INIT_OPEN = -4,
  TB_ERR_MEM = -5,
  TB_ERR_NO_EVENT = -6,
  TB_ERR_NO_TERM = -7,
  TB_ERR_NOT_INIT = -8,
  TB_ERR_OUT_OF_BOUNDS = -9,
  TB_ERR_READ = -10,
  TB_ERR_RESIZE_IOCTL = -11,
  TB_ERR_RESIZE_PIPE = -12,
  TB_ERR_RESIZE_SIGACTION = -13,
  TB_ERR_POLL = -14,
  TB_ERR_TCGETATTR = -15,
  TB_ERR_TCSETATTR = -16,
  TB_ERR_UNSUPPORTED_TERM = -17,
  TB_ERR_RESIZE_WRITE = -18,
  TB_ERR_RESIZE_POLL = -19,
  TB_ERR_RESIZE_READ = -20,
  TB_ERR_RESIZE_SSCANF = -21,
  TB_ERR_CAP_COLLISION = -22,
  TB_ERR_SELECT = TB_ERR_POLL,
  TB_ERR_RESIZE_SELECT = TB_ERR_RESIZE_POLL,

  TB_ERR_WIN32 = -24, // Added to indicate win32 api calls failed.
} tb_err;

#define if_err_return(rv, expr)                                                \
    if (((rv) = (expr)) != TB_OK) return (rv)
#define if_err_break(rv, expr)                                                 \
    if (((rv) = (expr)) != TB_OK) break
#define if_ok_return(rv, expr)                                                 \
    if (((rv) = (expr)) == TB_OK) return (rv)
#define if_ok_or_need_more_return(rv, expr)                                    \
    if (((rv) = (expr)) == TB_OK || (rv) == TB_ERR_NEED_MORE) return (rv)

#define send_literal(rv, a)                                                    \
    if_err_return((rv), bytebuf_nputs(&global.out, (a), sizeof(a) - 1))

#define send_num(rv, nbuf, n)                                                  \
    if_err_return((rv),                                                        \
        bytebuf_nputs(&global.out, (nbuf), convert_num((n), (nbuf))))

#define snprintf_or_return(rv, str, sz, fmt, ...)                              \
    do {                                                                       \
        (rv) = snprintf((str), (sz), (fmt), __VA_ARGS__);                      \
        if ((rv) < 0 || (rv) >= (int)(sz)) return TB_ERR;                      \
    } while (0)

#define if_not_init_return()                                                   \
    if (!global.initialized) return TB_ERR_NOT_INIT


/* The terminal screen is represented as 2d array of cells. The structure is
 * optimized for dealing with single-width (wcwidth()==1) Unicode codepoints.
 * See tb_present() for implementation.
 */
struct tb_cell {
  uint32_t ch;   /* a Unicode codepoint */
  uintattr_t fg; /* bitwise foreground attributes */
  uintattr_t bg; /* bitwise background attributes */
};

struct bytebuf_t {
  char* buf;
  size_t len;
  size_t cap;
};

struct cellbuf_t {
  int width;
  int height;
  struct tb_cell* cells;
};

/* An incoming event from the tty.
 *
 * Given the event type, the following fields are relevant:
 *
 *      when TB_EVENT_KEY: (key XOR ch, one will be zero), mod. Note there is
 *                         overlap between TB_MOD_CTRL and TB_KEY_CTRL_*.
 *                         TB_MOD_CTRL and TB_MOD_SHIFT are only set as
 *                         modifiers to TB_KEY_ARROW_*.
 *
 *   when TB_EVENT_RESIZE: w, h
 *
 *    when TB_EVENT_MOUSE: key (TB_KEY_MOUSE_*), x, y
 */
struct tb_event {
  uint8_t type; /* one of TB_EVENT_* constants */
  uint8_t mod;  /* bitwise TB_MOD_* constants */
  tb_key key;   /* one of TB_KEY_* constants */
  uint32_t ch;  /* a Unicode codepoint */
  int32_t w;    /* resize width */
  int32_t h;    /* resize height */
  int32_t x;    /* mouse x */
  int32_t y;    /* mouse y */
};

/* Sets the input mode. Termbox has two input modes:
 *
 * 1. TB_INPUT_ESC
 *    When escape (\x1b) is in the buffer and there's no match for an escape
 *    sequence, a key event for TB_KEY_ESC is returned.
 *
 * 2. TB_INPUT_ALT
 *    When escape (\x1b) is in the buffer and there's no match for an escape
 *    sequence, the next keyboard event is returned with a TB_MOD_ALT modifier.
 *
 * You can also apply TB_INPUT_MOUSE via bitwise OR operation to either of the
 * modes (e.g., TB_INPUT_ESC | TB_INPUT_MOUSE) to receive TB_EVENT_MOUSE events.
 * If none of the main two modes were set, but the mouse mode was, TB_INPUT_ESC
 * mode is used. If for some reason you've decided to use
 * (TB_INPUT_ESC | TB_INPUT_ALT) combination, it will behave as if only
 * TB_INPUT_ESC was selected.
 *
 * If mode is TB_INPUT_CURRENT, the function returns the current input mode.
 *
 * The default input mode is TB_INPUT_ESC.
 */
int tb_set_input_mode(int mode);

/* Sets the termbox output mode. Termbox has multiple output modes:
 *
 * 1. TB_OUTPUT_NORMAL     => [0..8]
 *
 *    This mode provides 8 different colors:
 *      TB_BLACK, TB_RED, TB_GREEN, TB_YELLOW,
 *      TB_BLUE, TB_MAGENTA, TB_CYAN, TB_WHITE
 *
 *    Plus TB_DEFAULT which skips sending a color code (i.e., uses the
 *    terminal's default color).
 *
 *    Colors (including TB_DEFAULT) may be bitwise OR'd with attributes:
 *      TB_BOLD, TB_UNDERLINE, TB_REVERSE, TB_ITALIC, TB_BLINK, TB_BRIGHT,
 *      TB_DIM
 *
 *    The following style attributes are also available if compiled with
 *    TB_OPT_ATTR_W set to 64:
 *      TB_STRIKEOUT, TB_UNDERLINE_2, TB_OVERLINE, TB_INVISIBLE
 *
 *    As in all modes, the value 0 is interpreted as TB_DEFAULT for
 *    convenience.
 *
 *    Some notes: TB_REVERSE can be applied as either fg or bg attributes for
 *    the same effect. TB_BRIGHT can be applied to either fg or bg. The rest of
 *    the attributes apply to fg only and are ignored as bg attributes.
 *
 *    Example usage:
 *      tb_set_cell(x, y, '@', TB_BLACK | TB_BOLD, TB_RED);
 *
 * 2. TB_OUTPUT_256        => [0..255] + TB_HI_BLACK
 *
 *    In this mode you get 256 distinct colors (plus default):
 *                0x00   (1): TB_DEFAULT
 *         TB_HI_BLACK   (1): TB_BLACK in TB_OUTPUT_NORMAL
 *          0x01..0x07   (7): the next 7 colors as in TB_OUTPUT_NORMAL
 *          0x08..0x0f   (8): bright versions of the above
 *          0x10..0xe7 (216): 216 different colors
 *          0xe8..0xff  (24): 24 different shades of gray
 *
 *    All TB_* style attributes except TB_BRIGHT may be bitwise OR'd as in
 *    TB_OUTPUT_NORMAL.
 *
 *    Note TB_HI_BLACK must be used for black, as 0x00 represents default.
 *
 * 3. TB_OUTPUT_216        => [0..216]
 *
 *    This mode supports the 216-color range of TB_OUTPUT_256 only, but you
 *    don't need to provide an offset:
 *                0x00   (1): TB_DEFAULT
 *          0x01..0xd8 (216): 216 different colors
 *
 * 4. TB_OUTPUT_GRAYSCALE  => [0..24]
 *
 *    This mode supports the 24-color range of TB_OUTPUT_256 only, but you
 *    don't need to provide an offset:
 *                0x00   (1): TB_DEFAULT
 *          0x01..0x18  (24): 24 different shades of gray
 *
 * 5. TB_OUTPUT_TRUECOLOR  => [0x000000..0xffffff] + TB_HI_BLACK
 *
 *    This mode provides 24-bit color on supported terminals. The format is
 *    0xRRGGBB.
 *
 *    All TB_* style attributes except TB_BRIGHT may be bitwise OR'd as in
 *    TB_OUTPUT_NORMAL.
 *
 *    Note TB_HI_BLACK must be used for black, as 0x000000 represents default.
 *
 * If mode is TB_OUTPUT_CURRENT, the function returns the current output mode.
 *
 * The default output mode is TB_OUTPUT_NORMAL.
 *
 * To use the terminal default color (i.e., to not send an escape code), pass
 * TB_DEFAULT. For convenience, the value 0 is interpreted as TB_DEFAULT in
 * all modes.
 *
 * Note, cell attributes persist after switching output modes. Any translation
 * between, for example, TB_OUTPUT_NORMAL's TB_RED and TB_OUTPUT_TRUECOLOR's
 * 0xff0000 must be performed by the caller. Also note that cells previously
 * rendered in one mode may persist unchanged until the front buffer is cleared
 * (such as after a resize event) at which point it will be re-interpreted and
 * flushed according to the current mode. Callers may invoke tb_invalidate if
 * it is desirable to immediately re-interpret and flush the entire screen
 * according to the current mode.
 *
 * Note, not all terminals support all output modes, especially beyond
 * TB_OUTPUT_NORMAL. There is also no very reliable way to determine color
 * support dynamically. If portability is desired, callers are recommended to
 * use TB_OUTPUT_NORMAL or make output mode end-user configurable. The same
 * advice applies to style attributes.
 */
int tb_set_output_mode(int mode);

/* Initializes the termbox library. This function should be called before any
 * other functions. tb_init() will use "/dev/tty" on posix and
 * use stdin, stdout in windows. After successful initialization, the library
 * must be finalized using the tb_shutdown() function.
 * 
 * On windows tb_init() is equlavent to:
 *   tb_init_rwfd(fileno(stdout), fileno(stdin));
 */
tb_err tb_init(void);
tb_err tb_init_rwfd(int rfd, int wfd);

tb_err tb_shutdown(void);

/* Returns the size of the internal back buffer (which is the same as terminal's
 * window size in rows and columns). The internal buffer can be resized after
 * tb_clear() or tb_present() function calls. Both dimensions have an
 * unspecified negative value when called before tb_init() or after
 * tb_shutdown().
 */
int tb_width(void);
int tb_height(void);

/* Clears the internal back buffer using TB_DEFAULT color or the
 * color/attributes set by tb_set_clear_attrs() function.
 */
tb_err tb_clear(void);
tb_err tb_set_clear_attrs(uintattr_t fg, uintattr_t bg);

/* Synchronizes the internal back buffer with the terminal by writing to tty. */
tb_err tb_present(void);

/* Clears the internal front buffer effectively forcing a complete re-render of
 * the back buffer to the tty. It is not necessary to call this under normal
 * circumstances. */
tb_err tb_invalidate(void);

/* Sets the position of the cursor. Upper-left character is (0, 0). */
tb_err tb_set_cursor(int cx, int cy);
tb_err tb_hide_cursor(void);

/* Set cell contents in the internal back buffer at the specified position. */
tb_err tb_set_cell(int x, int y, uint32_t ch, uintattr_t fg, uintattr_t bg);

/* POSIX implementation:
 *   Wait for an event up to timeout_ms milliseconds and fill the event structure
 *   with it. If no event is available within the timeout period, TB_ERR_NO_EVENT
 *   is returned. On a resize event, the underlying select(2) call may be
 *   interrupted, yielding a return code of TB_ERR_POLL. In this case, you may
 *   check errno via tb_last_errno(). If it's EINTR, you can safely ignore that
 *   and call tb_peek_event() again.
 * WIN32 implementation:
 */
tb_err tb_peek_event(struct tb_event* event, int timeout_ms);

/* Same as tb_peek_event except no timeout. */
tb_err tb_poll_event(struct tb_event* event);

/* Print and printf functions. Specify param out_w to determine width of printed
 * string. Incomplete trailing UTF-8 byte sequences are replaced with U+FFFD.
 * For finer control, use tb_set_cell().
 */
tb_err tb_print(int x, int y, uintattr_t fg, uintattr_t bg, const char* str);
tb_err tb_printf(int x, int y, uintattr_t fg, uintattr_t bg, const char* fmt, ...);
tb_err tb_print_ex(int x, int y, uintattr_t fg, uintattr_t bg, size_t* out_w,
                const char* str);
tb_err tb_printf_ex(int x, int y, uintattr_t fg, uintattr_t bg, size_t* out_w,
                 const char* fmt, ...);

/* Send raw bytes to terminal. */
tb_err tb_send(const char* buf, size_t nbuf);
tb_err tb_sendf(const char* fmt, ...);

/* Return byte length of codepoint given first byte of UTF-8 sequence (1-6). */
tb_err tb_utf8_char_length(char c);

/* Convert UTF-8 null-terminated byte sequence to UTF-32 codepoint.
 *
 * If `c` is an empty C string, return 0. `out` is left unchanged.
 *
 * If a null byte is encountered in the middle of the codepoint, return a
 * negative number indicating how many bytes were processed. `out` is left
 * unchanged.
 *
 * Otherwise, return byte length of codepoint (1-6).
 */
tb_err tb_utf8_char_to_unicode(uint32_t* out, const char* c);

/* Convert UTF-32 codepoint to UTF-8 null-terminated byte sequence.
 *
 * `out` must be char[7] or greater. Return byte length of codepoint (1-6).
 */
tb_err tb_utf8_unicode_to_char(char* out, uint32_t c);

/* Library utility functions */
int tb_last_errno(void);
const char* tb_strerror(int err);
struct tb_cell* tb_cell_buffer(void);
int tb_has_truecolor(void);
int tb_attr_width(void);
const char* tb_version(void);


#ifdef __cplusplus
}
#endif

#endif // TERMBOX_H
