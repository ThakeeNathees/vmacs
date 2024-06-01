/*
MIT License

Copyright (c) 2010-2020 nsf <no.smile.face@gmail.com>
              2015-2024 Adam Saponara <as@php.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "termbox.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#elif defined(_WIN32)
#include <Windows.h>
#include <io.h>

#define fileno _fileno
#define write _write

typedef int64_t ssize_t;

#ifndef ENABLE_VIRTUALINAL_PROCESSING
#define ENABLE_VIRTUALINAL_PROCESSING 0x0004
#endif

#else
  #error "Unsupported platform."
#endif

#define TB_VERSION_STR "2.5.0-dev"

#if defined(__unix__) || defined(__APPLE__)
struct cap_trie_t {
  char c;
  struct cap_trie_t* children;
  size_t nchildren;
  int is_leaf;
  uint16_t key;
  uint8_t mod;
};
#endif

struct tb_global_t {
  int rfd;
  int wfd;
  int width;
  int height;
  int cursor_x;
  int cursor_y;
  int last_x;
  int last_y;
  int output_mode;

  uintattr_t fg;
  uintattr_t bg;
  uintattr_t last_fg;
  uintattr_t last_bg;

  struct bytebuf_t out;
  struct cellbuf_t back;
  struct cellbuf_t front;

  const char* caps[TB_CAP__COUNT];

#if defined(__unix__) || defined(__APPLE__)
  int ttyfd_open;
  int ttyfd;
  int resize_pipefd[2];
  int input_mode;
  char* terminfo;
  size_t nterminfo;
  struct cap_trie_t cap_trie;
  struct termios orig_tios;
  struct bytebuf_t in;
  int has_orig_tios;
#endif

  int last_errno;
  int initialized;
  int (*fn_extract_esc_pre)(struct tb_event*, size_t*);
  int (*fn_extract_esc_post)(struct tb_event*, size_t*);
  char errbuf[1024];


#ifdef _WIN32
  HANDLE h_stdout, h_stdin; // Handles for stdin, out.
  DWORD mode_out, mode_in;  // Backup modes.
  DWORD button_state;       // Mouse button state.
#endif

};

static struct tb_global_t global = { 0 };

static tb_err tb_reset(void);
static tb_err tb_printf_inner(int x, int y, uintattr_t fg, uintattr_t bg,
                              size_t* out_w, const char* fmt, va_list vl);

static tb_err send_clear(void);

static tb_err init_cellbuf(void);
static tb_err tb_deinit(void);


static tb_err resize_cellbufs(void);
static tb_err send_attr(uintattr_t fg, uintattr_t bg);
static tb_err send_sgr(uint32_t fg, uint32_t bg, int fg_is_default,
                       int bg_is_default);
static tb_err send_cursor_if(int x, int y);
static tb_err send_char(int x, int y, uint32_t ch);
static tb_err send_cluster(int x, int y, uint32_t* ch, size_t nch);
static tb_err convert_num(uint32_t num, char* buf);
static tb_err cell_cmp(struct tb_cell* a, struct tb_cell* b);
static tb_err cell_copy(struct tb_cell* dst, struct tb_cell* src);
static tb_err cell_set(struct tb_cell* cell, uint32_t* ch,
                       uintattr_t fg, uintattr_t bg);
static tb_err cell_free(struct tb_cell* cell);
static tb_err cellbuf_init(struct cellbuf_t* c, int w, int h);
static tb_err cellbuf_free(struct cellbuf_t* c);
static tb_err cellbuf_clear(struct cellbuf_t* c);
static tb_err cellbuf_get(struct cellbuf_t* c, int x, int y, struct tb_cell** out);
static tb_err cellbuf_resize(struct cellbuf_t* c, int w, int h);
static tb_err bytebuf_puts(struct bytebuf_t* b, const char* str);
static tb_err bytebuf_nputs(struct bytebuf_t* b, const char* str, size_t nstr);
static tb_err bytebuf_shift(struct bytebuf_t* b, size_t n);
static tb_err bytebuf_flush(struct bytebuf_t* b, int fd);
static tb_err bytebuf_reserve(struct bytebuf_t* b, size_t sz);
static tb_err bytebuf_free(struct bytebuf_t* b);

// ----------------------------------------------------------------------------
// SECTION POSIX
// ----------------------------------------------------------------------------

#if defined(__unix__) || defined(__APPLE__)

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

/* Function types to be used with tb_set_func() */
#define TB_FUNC_EXTRACT_PRE     0
#define TB_FUNC_EXTRACT_POST    1

#ifdef PATH_MAX
#define TB_PATH_MAX PATH_MAX
#else
#define TB_PATH_MAX 4096
#endif

/* Some hard-coded caps */
#define TB_HARDCAP_ENTER_MOUSE  "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define TB_HARDCAP_EXIT_MOUSE   "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"
#define TB_HARDCAP_STRIKEOUT    "\x1b[9m"
#define TB_HARDCAP_UNDERLINE_2  "\x1b[21m"
#define TB_HARDCAP_OVERLINE     "\x1b[53m"

/* Set custom functions. fn_type is one of TB_FUNC_* constants, fn is a
 * compatible function pointer, or NULL to clear.
 *
 * TB_FUNC_EXTRACT_PRE:
 *   If specified, invoke this function BEFORE termbox tries to extract any
 *   escape sequences from the input buffer.
 *
 * TB_FUNC_EXTRACT_POST:
 *   If specified, invoke this function AFTER termbox tries (and fails) to
 *   extract any escape sequences from the input buffer.
 */
tb_err tb_set_func(int fn_type, int (*fn)(struct tb_event*, size_t*));

static tb_err init_term_attrs(void);
static tb_err init_term_caps(void);
static tb_err init_cap_trie(void);
static tb_err cap_trie_add(const char* cap, uint16_t key, uint8_t mod);
static tb_err cap_trie_find(const char* buf, size_t nbuf, struct cap_trie_t** last,
                         size_t* depth);
static tb_err cap_trie_deinit(struct cap_trie_t* node);
static tb_err init_resize_handler(void);
static tb_err send_init_escape_codes(void);

tb_err tb_init_file(const char* path);
tb_err tb_init_fd(int ttyfd);

static tb_err load_terminfo(void);
static tb_err load_terminfo_from_path(const char* path, const char* term);
static tb_err read_terminfo_path(const char* path);
static tb_err parse_terminfo_caps(void);
static tb_err load_builtin_caps(void);
static const char* get_terminfo_string(int16_t str_offsets_pos,
                                       int16_t str_offsets_len, int16_t str_table_pos, int16_t str_table_len,
                                       int16_t str_index);
static tb_err update_term_size(void);
static tb_err update_term_size_via_esc(void);
static tb_err wait_event(struct tb_event* event, int timeout);
static tb_err extract_event(struct tb_event* event);
static tb_err extract_esc(struct tb_event* event);
static tb_err extract_esc_user(struct tb_event* event, int is_post);
static tb_err extract_esc_cap(struct tb_event* event);
static tb_err extract_esc_mouse(struct tb_event* event);
static void handle_resize(int sig);

/* Internal termbox FDs that can be used with poll() / select(). Must call
 * tb_poll_event() / tb_peek_event() if activity is detected. */
tb_err tb_get_fds(int* ttyfd, int* resizefd);

/* BEGIN codegen c */
/* Produced by ./codegen.sh on Thu, 13 Jul 2023 05:46:13 +0000 */

static const int16_t terminfo_cap_indexes[] = {
    66,  // kf1 (TB_CAP_F1)
    68,  // kf2 (TB_CAP_F2)
    69,  // kf3 (TB_CAP_F3)
    70,  // kf4 (TB_CAP_F4)
    71,  // kf5 (TB_CAP_F5)
    72,  // kf6 (TB_CAP_F6)
    73,  // kf7 (TB_CAP_F7)
    74,  // kf8 (TB_CAP_F8)
    75,  // kf9 (TB_CAP_F9)
    67,  // kf10 (TB_CAP_F10)
    216, // kf11 (TB_CAP_F11)
    217, // kf12 (TB_CAP_F12)
    77,  // kich1 (TB_CAP_INSERT)
    59,  // kdch1 (TB_CAP_DELETE)
    76,  // khome (TB_CAP_HOME)
    164, // kend (TB_CAP_END)
    82,  // kpp (TB_CAP_PGUP)
    81,  // knp (TB_CAP_PGDN)
    87,  // kcuu1 (TB_CAP_ARROW_UP)
    61,  // kcud1 (TB_CAP_ARROW_DOWN)
    79,  // kcub1 (TB_CAP_ARROW_LEFT)
    83,  // kcuf1 (TB_CAP_ARROW_RIGHT)
    148, // kcbt (TB_CAP_BACK_TAB)
    28,  // smcup (TB_CAP_ENTER_CA)
    40,  // rmcup (TB_CAP_EXIT_CA)
    16,  // cnorm (TB_CAP_SHOW_CURSOR)
    13,  // civis (TB_CAP_HIDE_CURSOR)
    5,   // clear (TB_CAP_CLEAR_SCREEN)
    39,  // sgr0 (TB_CAP_SGR0)
    36,  // smul (TB_CAP_UNDERLINE)
    27,  // bold (TB_CAP_BOLD)
    26,  // blink (TB_CAP_BLINK)
    311, // sitm (TB_CAP_ITALIC)
    34,  // rev (TB_CAP_REVERSE)
    89,  // smkx (TB_CAP_ENTER_KEYPAD)
    88,  // rmkx (TB_CAP_EXIT_KEYPAD)
    30,  // dim (TB_CAP_DIM)
    32,  // invis (TB_CAP_INVISIBLE)
};

// xterm
static const char* xterm_caps[] = {
    "\033OP",                  // kf1 (TB_CAP_F1)
    "\033OQ",                  // kf2 (TB_CAP_F2)
    "\033OR",                  // kf3 (TB_CAP_F3)
    "\033OS",                  // kf4 (TB_CAP_F4)
    "\033[15~",                // kf5 (TB_CAP_F5)
    "\033[17~",                // kf6 (TB_CAP_F6)
    "\033[18~",                // kf7 (TB_CAP_F7)
    "\033[19~",                // kf8 (TB_CAP_F8)
    "\033[20~",                // kf9 (TB_CAP_F9)
    "\033[21~",                // kf10 (TB_CAP_F10)
    "\033[23~",                // kf11 (TB_CAP_F11)
    "\033[24~",                // kf12 (TB_CAP_F12)
    "\033[2~",                 // kich1 (TB_CAP_INSERT)
    "\033[3~",                 // kdch1 (TB_CAP_DELETE)
    "\033OH",                  // khome (TB_CAP_HOME)
    "\033OF",                  // kend (TB_CAP_END)
    "\033[5~",                 // kpp (TB_CAP_PGUP)
    "\033[6~",                 // knp (TB_CAP_PGDN)
    "\033OA",                  // kcuu1 (TB_CAP_ARROW_UP)
    "\033OB",                  // kcud1 (TB_CAP_ARROW_DOWN)
    "\033OD",                  // kcub1 (TB_CAP_ARROW_LEFT)
    "\033OC",                  // kcuf1 (TB_CAP_ARROW_RIGHT)
    "\033[Z",                  // kcbt (TB_CAP_BACK_TAB)
    "\033[?1049h\033[22;0;0t", // smcup (TB_CAP_ENTER_CA)
    "\033[?1049l\033[23;0;0t", // rmcup (TB_CAP_EXIT_CA)
    "\033[?12l\033[?25h",      // cnorm (TB_CAP_SHOW_CURSOR)
    "\033[?25l",               // civis (TB_CAP_HIDE_CURSOR)
    "\033[H\033[2J",           // clear (TB_CAP_CLEAR_SCREEN)
    "\033(B\033[m",            // sgr0 (TB_CAP_SGR0)
    "\033[4m",                 // smul (TB_CAP_UNDERLINE)
    "\033[1m",                 // bold (TB_CAP_BOLD)
    "\033[5m",                 // blink (TB_CAP_BLINK)
    "\033[3m",                 // sitm (TB_CAP_ITALIC)
    "\033[7m",                 // rev (TB_CAP_REVERSE)
    "\033[?1h\033=",           // smkx (TB_CAP_ENTER_KEYPAD)
    "\033[?1l\033>",           // rmkx (TB_CAP_EXIT_KEYPAD)
    "\033[2m",                 // dim (TB_CAP_DIM)
    "\033[8m",                 // invis (TB_CAP_INVISIBLE)
};

// linux
static const char* linux_caps[] = {
    "\033[[A",           // kf1 (TB_CAP_F1)
    "\033[[B",           // kf2 (TB_CAP_F2)
    "\033[[C",           // kf3 (TB_CAP_F3)
    "\033[[D",           // kf4 (TB_CAP_F4)
    "\033[[E",           // kf5 (TB_CAP_F5)
    "\033[17~",          // kf6 (TB_CAP_F6)
    "\033[18~",          // kf7 (TB_CAP_F7)
    "\033[19~",          // kf8 (TB_CAP_F8)
    "\033[20~",          // kf9 (TB_CAP_F9)
    "\033[21~",          // kf10 (TB_CAP_F10)
    "\033[23~",          // kf11 (TB_CAP_F11)
    "\033[24~",          // kf12 (TB_CAP_F12)
    "\033[2~",           // kich1 (TB_CAP_INSERT)
    "\033[3~",           // kdch1 (TB_CAP_DELETE)
    "\033[1~",           // khome (TB_CAP_HOME)
    "\033[4~",           // kend (TB_CAP_END)
    "\033[5~",           // kpp (TB_CAP_PGUP)
    "\033[6~",           // knp (TB_CAP_PGDN)
    "\033[A",            // kcuu1 (TB_CAP_ARROW_UP)
    "\033[B",            // kcud1 (TB_CAP_ARROW_DOWN)
    "\033[D",            // kcub1 (TB_CAP_ARROW_LEFT)
    "\033[C",            // kcuf1 (TB_CAP_ARROW_RIGHT)
    "\033\011",          // kcbt (TB_CAP_BACK_TAB)
    "",                  // smcup (TB_CAP_ENTER_CA)
    "",                  // rmcup (TB_CAP_EXIT_CA)
    "\033[?25h\033[?0c", // cnorm (TB_CAP_SHOW_CURSOR)
    "\033[?25l\033[?1c", // civis (TB_CAP_HIDE_CURSOR)
    "\033[H\033[J",      // clear (TB_CAP_CLEAR_SCREEN)
    "\033[m\017",        // sgr0 (TB_CAP_SGR0)
    "\033[4m",           // smul (TB_CAP_UNDERLINE)
    "\033[1m",           // bold (TB_CAP_BOLD)
    "\033[5m",           // blink (TB_CAP_BLINK)
    "",                  // sitm (TB_CAP_ITALIC)
    "\033[7m",           // rev (TB_CAP_REVERSE)
    "",                  // smkx (TB_CAP_ENTER_KEYPAD)
    "",                  // rmkx (TB_CAP_EXIT_KEYPAD)
    "\033[2m",           // dim (TB_CAP_DIM)
    "",                  // invis (TB_CAP_INVISIBLE)
};

// screen
static const char* screen_caps[] = {
    "\033OP",            // kf1 (TB_CAP_F1)
    "\033OQ",            // kf2 (TB_CAP_F2)
    "\033OR",            // kf3 (TB_CAP_F3)
    "\033OS",            // kf4 (TB_CAP_F4)
    "\033[15~",          // kf5 (TB_CAP_F5)
    "\033[17~",          // kf6 (TB_CAP_F6)
    "\033[18~",          // kf7 (TB_CAP_F7)
    "\033[19~",          // kf8 (TB_CAP_F8)
    "\033[20~",          // kf9 (TB_CAP_F9)
    "\033[21~",          // kf10 (TB_CAP_F10)
    "\033[23~",          // kf11 (TB_CAP_F11)
    "\033[24~",          // kf12 (TB_CAP_F12)
    "\033[2~",           // kich1 (TB_CAP_INSERT)
    "\033[3~",           // kdch1 (TB_CAP_DELETE)
    "\033[1~",           // khome (TB_CAP_HOME)
    "\033[4~",           // kend (TB_CAP_END)
    "\033[5~",           // kpp (TB_CAP_PGUP)
    "\033[6~",           // knp (TB_CAP_PGDN)
    "\033OA",            // kcuu1 (TB_CAP_ARROW_UP)
    "\033OB",            // kcud1 (TB_CAP_ARROW_DOWN)
    "\033OD",            // kcub1 (TB_CAP_ARROW_LEFT)
    "\033OC",            // kcuf1 (TB_CAP_ARROW_RIGHT)
    "\033[Z",            // kcbt (TB_CAP_BACK_TAB)
    "\033[?1049h",       // smcup (TB_CAP_ENTER_CA)
    "\033[?1049l",       // rmcup (TB_CAP_EXIT_CA)
    "\033[34h\033[?25h", // cnorm (TB_CAP_SHOW_CURSOR)
    "\033[?25l",         // civis (TB_CAP_HIDE_CURSOR)
    "\033[H\033[J",      // clear (TB_CAP_CLEAR_SCREEN)
    "\033[m\017",        // sgr0 (TB_CAP_SGR0)
    "\033[4m",           // smul (TB_CAP_UNDERLINE)
    "\033[1m",           // bold (TB_CAP_BOLD)
    "\033[5m",           // blink (TB_CAP_BLINK)
    "",                  // sitm (TB_CAP_ITALIC)
    "\033[7m",           // rev (TB_CAP_REVERSE)
    "\033[?1h\033=",     // smkx (TB_CAP_ENTER_KEYPAD)
    "\033[?1l\033>",     // rmkx (TB_CAP_EXIT_KEYPAD)
    "\033[2m",           // dim (TB_CAP_DIM)
    "",                  // invis (TB_CAP_INVISIBLE)
};

// rxvt-256color
static const char* rxvt_256color_caps[] = {
    "\033[11~",              // kf1 (TB_CAP_F1)
    "\033[12~",              // kf2 (TB_CAP_F2)
    "\033[13~",              // kf3 (TB_CAP_F3)
    "\033[14~",              // kf4 (TB_CAP_F4)
    "\033[15~",              // kf5 (TB_CAP_F5)
    "\033[17~",              // kf6 (TB_CAP_F6)
    "\033[18~",              // kf7 (TB_CAP_F7)
    "\033[19~",              // kf8 (TB_CAP_F8)
    "\033[20~",              // kf9 (TB_CAP_F9)
    "\033[21~",              // kf10 (TB_CAP_F10)
    "\033[23~",              // kf11 (TB_CAP_F11)
    "\033[24~",              // kf12 (TB_CAP_F12)
    "\033[2~",               // kich1 (TB_CAP_INSERT)
    "\033[3~",               // kdch1 (TB_CAP_DELETE)
    "\033[7~",               // khome (TB_CAP_HOME)
    "\033[8~",               // kend (TB_CAP_END)
    "\033[5~",               // kpp (TB_CAP_PGUP)
    "\033[6~",               // knp (TB_CAP_PGDN)
    "\033[A",                // kcuu1 (TB_CAP_ARROW_UP)
    "\033[B",                // kcud1 (TB_CAP_ARROW_DOWN)
    "\033[D",                // kcub1 (TB_CAP_ARROW_LEFT)
    "\033[C",                // kcuf1 (TB_CAP_ARROW_RIGHT)
    "\033[Z",                // kcbt (TB_CAP_BACK_TAB)
    "\0337\033[?47h",        // smcup (TB_CAP_ENTER_CA)
    "\033[2J\033[?47l\0338", // rmcup (TB_CAP_EXIT_CA)
    "\033[?25h",             // cnorm (TB_CAP_SHOW_CURSOR)
    "\033[?25l",             // civis (TB_CAP_HIDE_CURSOR)
    "\033[H\033[2J",         // clear (TB_CAP_CLEAR_SCREEN)
    "\033[m\017",            // sgr0 (TB_CAP_SGR0)
    "\033[4m",               // smul (TB_CAP_UNDERLINE)
    "\033[1m",               // bold (TB_CAP_BOLD)
    "\033[5m",               // blink (TB_CAP_BLINK)
    "",                      // sitm (TB_CAP_ITALIC)
    "\033[7m",               // rev (TB_CAP_REVERSE)
    "\033=",                 // smkx (TB_CAP_ENTER_KEYPAD)
    "\033>",                 // rmkx (TB_CAP_EXIT_KEYPAD)
    "",                      // dim (TB_CAP_DIM)
    "",                      // invis (TB_CAP_INVISIBLE)
};

// rxvt-unicode
static const char* rxvt_unicode_caps[] = {
    "\033[11~",           // kf1 (TB_CAP_F1)
    "\033[12~",           // kf2 (TB_CAP_F2)
    "\033[13~",           // kf3 (TB_CAP_F3)
    "\033[14~",           // kf4 (TB_CAP_F4)
    "\033[15~",           // kf5 (TB_CAP_F5)
    "\033[17~",           // kf6 (TB_CAP_F6)
    "\033[18~",           // kf7 (TB_CAP_F7)
    "\033[19~",           // kf8 (TB_CAP_F8)
    "\033[20~",           // kf9 (TB_CAP_F9)
    "\033[21~",           // kf10 (TB_CAP_F10)
    "\033[23~",           // kf11 (TB_CAP_F11)
    "\033[24~",           // kf12 (TB_CAP_F12)
    "\033[2~",            // kich1 (TB_CAP_INSERT)
    "\033[3~",            // kdch1 (TB_CAP_DELETE)
    "\033[7~",            // khome (TB_CAP_HOME)
    "\033[8~",            // kend (TB_CAP_END)
    "\033[5~",            // kpp (TB_CAP_PGUP)
    "\033[6~",            // knp (TB_CAP_PGDN)
    "\033[A",             // kcuu1 (TB_CAP_ARROW_UP)
    "\033[B",             // kcud1 (TB_CAP_ARROW_DOWN)
    "\033[D",             // kcub1 (TB_CAP_ARROW_LEFT)
    "\033[C",             // kcuf1 (TB_CAP_ARROW_RIGHT)
    "\033[Z",             // kcbt (TB_CAP_BACK_TAB)
    "\033[?1049h",        // smcup (TB_CAP_ENTER_CA)
    "\033[r\033[?1049l",  // rmcup (TB_CAP_EXIT_CA)
    "\033[?12l\033[?25h", // cnorm (TB_CAP_SHOW_CURSOR)
    "\033[?25l",          // civis (TB_CAP_HIDE_CURSOR)
    "\033[H\033[2J",      // clear (TB_CAP_CLEAR_SCREEN)
    "\033[m\033(B",       // sgr0 (TB_CAP_SGR0)
    "\033[4m",            // smul (TB_CAP_UNDERLINE)
    "\033[1m",            // bold (TB_CAP_BOLD)
    "\033[5m",            // blink (TB_CAP_BLINK)
    "\033[3m",            // sitm (TB_CAP_ITALIC)
    "\033[7m",            // rev (TB_CAP_REVERSE)
    "\033=",              // smkx (TB_CAP_ENTER_KEYPAD)
    "\033>",              // rmkx (TB_CAP_EXIT_KEYPAD)
    "",                   // dim (TB_CAP_DIM)
    "",                   // invis (TB_CAP_INVISIBLE)
};

// Eterm
static const char* eterm_caps[] = {
    "\033[11~",              // kf1 (TB_CAP_F1)
    "\033[12~",              // kf2 (TB_CAP_F2)
    "\033[13~",              // kf3 (TB_CAP_F3)
    "\033[14~",              // kf4 (TB_CAP_F4)
    "\033[15~",              // kf5 (TB_CAP_F5)
    "\033[17~",              // kf6 (TB_CAP_F6)
    "\033[18~",              // kf7 (TB_CAP_F7)
    "\033[19~",              // kf8 (TB_CAP_F8)
    "\033[20~",              // kf9 (TB_CAP_F9)
    "\033[21~",              // kf10 (TB_CAP_F10)
    "\033[23~",              // kf11 (TB_CAP_F11)
    "\033[24~",              // kf12 (TB_CAP_F12)
    "\033[2~",               // kich1 (TB_CAP_INSERT)
    "\033[3~",               // kdch1 (TB_CAP_DELETE)
    "\033[7~",               // khome (TB_CAP_HOME)
    "\033[8~",               // kend (TB_CAP_END)
    "\033[5~",               // kpp (TB_CAP_PGUP)
    "\033[6~",               // knp (TB_CAP_PGDN)
    "\033[A",                // kcuu1 (TB_CAP_ARROW_UP)
    "\033[B",                // kcud1 (TB_CAP_ARROW_DOWN)
    "\033[D",                // kcub1 (TB_CAP_ARROW_LEFT)
    "\033[C",                // kcuf1 (TB_CAP_ARROW_RIGHT)
    "",                      // kcbt (TB_CAP_BACK_TAB)
    "\0337\033[?47h",        // smcup (TB_CAP_ENTER_CA)
    "\033[2J\033[?47l\0338", // rmcup (TB_CAP_EXIT_CA)
    "\033[?25h",             // cnorm (TB_CAP_SHOW_CURSOR)
    "\033[?25l",             // civis (TB_CAP_HIDE_CURSOR)
    "\033[H\033[2J",         // clear (TB_CAP_CLEAR_SCREEN)
    "\033[m\017",            // sgr0 (TB_CAP_SGR0)
    "\033[4m",               // smul (TB_CAP_UNDERLINE)
    "\033[1m",               // bold (TB_CAP_BOLD)
    "\033[5m",               // blink (TB_CAP_BLINK)
    "",                      // sitm (TB_CAP_ITALIC)
    "\033[7m",               // rev (TB_CAP_REVERSE)
    "",                      // smkx (TB_CAP_ENTER_KEYPAD)
    "",                      // rmkx (TB_CAP_EXIT_KEYPAD)
    "",                      // dim (TB_CAP_DIM)
    "",                      // invis (TB_CAP_INVISIBLE)
};

static struct {
  const char* name;
  const char** caps;
  const char* alias;
} builtin_terms[] = {
    {"xterm",         xterm_caps,         ""    },
    {"linux",         linux_caps,         ""    },
    {"screen",        screen_caps,        "tmux"},
    {"rxvt-256color", rxvt_256color_caps, ""    },
    {"rxvt-unicode",  rxvt_unicode_caps,  "rxvt"},
    {"Eterm",         eterm_caps,         ""    },
    {NULL,            NULL,               NULL  },
};

/* END codegen c */

static struct {
  const char* cap;
  const uint16_t key;
  const uint8_t mod;
} builtin_mod_caps[] = {
  // xterm arrows
    {"\x1b[1;2A",    TB_KEY_ARROW_UP,    TB_MOD_SHIFT                           },
    {"\x1b[1;3A",    TB_KEY_ARROW_UP,    TB_MOD_ALT                             },
    {"\x1b[1;4A",    TB_KEY_ARROW_UP,    TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5A",    TB_KEY_ARROW_UP,    TB_MOD_CTRL                            },
    {"\x1b[1;6A",    TB_KEY_ARROW_UP,    TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7A",    TB_KEY_ARROW_UP,    TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8A",    TB_KEY_ARROW_UP,    TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2B",    TB_KEY_ARROW_DOWN,  TB_MOD_SHIFT                           },
    {"\x1b[1;3B",    TB_KEY_ARROW_DOWN,  TB_MOD_ALT                             },
    {"\x1b[1;4B",    TB_KEY_ARROW_DOWN,  TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5B",    TB_KEY_ARROW_DOWN,  TB_MOD_CTRL                            },
    {"\x1b[1;6B",    TB_KEY_ARROW_DOWN,  TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7B",    TB_KEY_ARROW_DOWN,  TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8B",    TB_KEY_ARROW_DOWN,  TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2C",    TB_KEY_ARROW_RIGHT, TB_MOD_SHIFT                           },
    {"\x1b[1;3C",    TB_KEY_ARROW_RIGHT, TB_MOD_ALT                             },
    {"\x1b[1;4C",    TB_KEY_ARROW_RIGHT, TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5C",    TB_KEY_ARROW_RIGHT, TB_MOD_CTRL                            },
    {"\x1b[1;6C",    TB_KEY_ARROW_RIGHT, TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7C",    TB_KEY_ARROW_RIGHT, TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8C",    TB_KEY_ARROW_RIGHT, TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2D",    TB_KEY_ARROW_LEFT,  TB_MOD_SHIFT                           },
    {"\x1b[1;3D",    TB_KEY_ARROW_LEFT,  TB_MOD_ALT                             },
    {"\x1b[1;4D",    TB_KEY_ARROW_LEFT,  TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5D",    TB_KEY_ARROW_LEFT,  TB_MOD_CTRL                            },
    {"\x1b[1;6D",    TB_KEY_ARROW_LEFT,  TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7D",    TB_KEY_ARROW_LEFT,  TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8D",    TB_KEY_ARROW_LEFT,  TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    // xterm keys
    {"\x1b[1;2H",    TB_KEY_HOME,        TB_MOD_SHIFT                           },
    {"\x1b[1;3H",    TB_KEY_HOME,        TB_MOD_ALT                             },
    {"\x1b[1;4H",    TB_KEY_HOME,        TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5H",    TB_KEY_HOME,        TB_MOD_CTRL                            },
    {"\x1b[1;6H",    TB_KEY_HOME,        TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7H",    TB_KEY_HOME,        TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8H",    TB_KEY_HOME,        TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2F",    TB_KEY_END,         TB_MOD_SHIFT                           },
    {"\x1b[1;3F",    TB_KEY_END,         TB_MOD_ALT                             },
    {"\x1b[1;4F",    TB_KEY_END,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5F",    TB_KEY_END,         TB_MOD_CTRL                            },
    {"\x1b[1;6F",    TB_KEY_END,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7F",    TB_KEY_END,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8F",    TB_KEY_END,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[2;2~",    TB_KEY_INSERT,      TB_MOD_SHIFT                           },
    {"\x1b[2;3~",    TB_KEY_INSERT,      TB_MOD_ALT                             },
    {"\x1b[2;4~",    TB_KEY_INSERT,      TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[2;5~",    TB_KEY_INSERT,      TB_MOD_CTRL                            },
    {"\x1b[2;6~",    TB_KEY_INSERT,      TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[2;7~",    TB_KEY_INSERT,      TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[2;8~",    TB_KEY_INSERT,      TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[3;2~",    TB_KEY_DELETE,      TB_MOD_SHIFT                           },
    {"\x1b[3;3~",    TB_KEY_DELETE,      TB_MOD_ALT                             },
    {"\x1b[3;4~",    TB_KEY_DELETE,      TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[3;5~",    TB_KEY_DELETE,      TB_MOD_CTRL                            },
    {"\x1b[3;6~",    TB_KEY_DELETE,      TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[3;7~",    TB_KEY_DELETE,      TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[3;8~",    TB_KEY_DELETE,      TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[5;2~",    TB_KEY_PGUP,        TB_MOD_SHIFT                           },
    {"\x1b[5;3~",    TB_KEY_PGUP,        TB_MOD_ALT                             },
    {"\x1b[5;4~",    TB_KEY_PGUP,        TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[5;5~",    TB_KEY_PGUP,        TB_MOD_CTRL                            },
    {"\x1b[5;6~",    TB_KEY_PGUP,        TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[5;7~",    TB_KEY_PGUP,        TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[5;8~",    TB_KEY_PGUP,        TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[6;2~",    TB_KEY_PGDN,        TB_MOD_SHIFT                           },
    {"\x1b[6;3~",    TB_KEY_PGDN,        TB_MOD_ALT                             },
    {"\x1b[6;4~",    TB_KEY_PGDN,        TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[6;5~",    TB_KEY_PGDN,        TB_MOD_CTRL                            },
    {"\x1b[6;6~",    TB_KEY_PGDN,        TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[6;7~",    TB_KEY_PGDN,        TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[6;8~",    TB_KEY_PGDN,        TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2P",    TB_KEY_F1,          TB_MOD_SHIFT                           },
    {"\x1b[1;3P",    TB_KEY_F1,          TB_MOD_ALT                             },
    {"\x1b[1;4P",    TB_KEY_F1,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5P",    TB_KEY_F1,          TB_MOD_CTRL                            },
    {"\x1b[1;6P",    TB_KEY_F1,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7P",    TB_KEY_F1,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8P",    TB_KEY_F1,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2Q",    TB_KEY_F2,          TB_MOD_SHIFT                           },
    {"\x1b[1;3Q",    TB_KEY_F2,          TB_MOD_ALT                             },
    {"\x1b[1;4Q",    TB_KEY_F2,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5Q",    TB_KEY_F2,          TB_MOD_CTRL                            },
    {"\x1b[1;6Q",    TB_KEY_F2,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7Q",    TB_KEY_F2,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8Q",    TB_KEY_F2,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2R",    TB_KEY_F3,          TB_MOD_SHIFT                           },
    {"\x1b[1;3R",    TB_KEY_F3,          TB_MOD_ALT                             },
    {"\x1b[1;4R",    TB_KEY_F3,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5R",    TB_KEY_F3,          TB_MOD_CTRL                            },
    {"\x1b[1;6R",    TB_KEY_F3,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7R",    TB_KEY_F3,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8R",    TB_KEY_F3,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[1;2S",    TB_KEY_F4,          TB_MOD_SHIFT                           },
    {"\x1b[1;3S",    TB_KEY_F4,          TB_MOD_ALT                             },
    {"\x1b[1;4S",    TB_KEY_F4,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[1;5S",    TB_KEY_F4,          TB_MOD_CTRL                            },
    {"\x1b[1;6S",    TB_KEY_F4,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[1;7S",    TB_KEY_F4,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[1;8S",    TB_KEY_F4,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[15;2~",   TB_KEY_F5,          TB_MOD_SHIFT                           },
    {"\x1b[15;3~",   TB_KEY_F5,          TB_MOD_ALT                             },
    {"\x1b[15;4~",   TB_KEY_F5,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[15;5~",   TB_KEY_F5,          TB_MOD_CTRL                            },
    {"\x1b[15;6~",   TB_KEY_F5,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[15;7~",   TB_KEY_F5,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[15;8~",   TB_KEY_F5,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[17;2~",   TB_KEY_F6,          TB_MOD_SHIFT                           },
    {"\x1b[17;3~",   TB_KEY_F6,          TB_MOD_ALT                             },
    {"\x1b[17;4~",   TB_KEY_F6,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[17;5~",   TB_KEY_F6,          TB_MOD_CTRL                            },
    {"\x1b[17;6~",   TB_KEY_F6,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[17;7~",   TB_KEY_F6,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[17;8~",   TB_KEY_F6,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[18;2~",   TB_KEY_F7,          TB_MOD_SHIFT                           },
    {"\x1b[18;3~",   TB_KEY_F7,          TB_MOD_ALT                             },
    {"\x1b[18;4~",   TB_KEY_F7,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[18;5~",   TB_KEY_F7,          TB_MOD_CTRL                            },
    {"\x1b[18;6~",   TB_KEY_F7,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[18;7~",   TB_KEY_F7,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[18;8~",   TB_KEY_F7,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[19;2~",   TB_KEY_F8,          TB_MOD_SHIFT                           },
    {"\x1b[19;3~",   TB_KEY_F8,          TB_MOD_ALT                             },
    {"\x1b[19;4~",   TB_KEY_F8,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[19;5~",   TB_KEY_F8,          TB_MOD_CTRL                            },
    {"\x1b[19;6~",   TB_KEY_F8,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[19;7~",   TB_KEY_F8,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[19;8~",   TB_KEY_F8,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[20;2~",   TB_KEY_F9,          TB_MOD_SHIFT                           },
    {"\x1b[20;3~",   TB_KEY_F9,          TB_MOD_ALT                             },
    {"\x1b[20;4~",   TB_KEY_F9,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[20;5~",   TB_KEY_F9,          TB_MOD_CTRL                            },
    {"\x1b[20;6~",   TB_KEY_F9,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[20;7~",   TB_KEY_F9,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[20;8~",   TB_KEY_F9,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[21;2~",   TB_KEY_F10,         TB_MOD_SHIFT                           },
    {"\x1b[21;3~",   TB_KEY_F10,         TB_MOD_ALT                             },
    {"\x1b[21;4~",   TB_KEY_F10,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[21;5~",   TB_KEY_F10,         TB_MOD_CTRL                            },
    {"\x1b[21;6~",   TB_KEY_F10,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[21;7~",   TB_KEY_F10,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[21;8~",   TB_KEY_F10,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[23;2~",   TB_KEY_F11,         TB_MOD_SHIFT                           },
    {"\x1b[23;3~",   TB_KEY_F11,         TB_MOD_ALT                             },
    {"\x1b[23;4~",   TB_KEY_F11,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[23;5~",   TB_KEY_F11,         TB_MOD_CTRL                            },
    {"\x1b[23;6~",   TB_KEY_F11,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[23;7~",   TB_KEY_F11,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[23;8~",   TB_KEY_F11,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b[24;2~",   TB_KEY_F12,         TB_MOD_SHIFT                           },
    {"\x1b[24;3~",   TB_KEY_F12,         TB_MOD_ALT                             },
    {"\x1b[24;4~",   TB_KEY_F12,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[24;5~",   TB_KEY_F12,         TB_MOD_CTRL                            },
    {"\x1b[24;6~",   TB_KEY_F12,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[24;7~",   TB_KEY_F12,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b[24;8~",   TB_KEY_F12,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    // rxvt arrows
    {"\x1b[a",       TB_KEY_ARROW_UP,    TB_MOD_SHIFT                           },
    {"\x1b\x1b[A",   TB_KEY_ARROW_UP,    TB_MOD_ALT                             },
    {"\x1b\x1b[a",   TB_KEY_ARROW_UP,    TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1bOa",       TB_KEY_ARROW_UP,    TB_MOD_CTRL                            },
    {"\x1b\x1bOa",   TB_KEY_ARROW_UP,    TB_MOD_CTRL | TB_MOD_ALT               },

    {"\x1b[b",       TB_KEY_ARROW_DOWN,  TB_MOD_SHIFT                           },
    {"\x1b\x1b[B",   TB_KEY_ARROW_DOWN,  TB_MOD_ALT                             },
    {"\x1b\x1b[b",   TB_KEY_ARROW_DOWN,  TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1bOb",       TB_KEY_ARROW_DOWN,  TB_MOD_CTRL                            },
    {"\x1b\x1bOb",   TB_KEY_ARROW_DOWN,  TB_MOD_CTRL | TB_MOD_ALT               },

    {"\x1b[c",       TB_KEY_ARROW_RIGHT, TB_MOD_SHIFT                           },
    {"\x1b\x1b[C",   TB_KEY_ARROW_RIGHT, TB_MOD_ALT                             },
    {"\x1b\x1b[c",   TB_KEY_ARROW_RIGHT, TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1bOc",       TB_KEY_ARROW_RIGHT, TB_MOD_CTRL                            },
    {"\x1b\x1bOc",   TB_KEY_ARROW_RIGHT, TB_MOD_CTRL | TB_MOD_ALT               },

    {"\x1b[d",       TB_KEY_ARROW_LEFT,  TB_MOD_SHIFT                           },
    {"\x1b\x1b[D",   TB_KEY_ARROW_LEFT,  TB_MOD_ALT                             },
    {"\x1b\x1b[d",   TB_KEY_ARROW_LEFT,  TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1bOd",       TB_KEY_ARROW_LEFT,  TB_MOD_CTRL                            },
    {"\x1b\x1bOd",   TB_KEY_ARROW_LEFT,  TB_MOD_CTRL | TB_MOD_ALT               },

    // rxvt keys
    {"\x1b[7$",      TB_KEY_HOME,        TB_MOD_SHIFT                           },
    {"\x1b\x1b[7~",  TB_KEY_HOME,        TB_MOD_ALT                             },
    {"\x1b\x1b[7$",  TB_KEY_HOME,        TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[7^",      TB_KEY_HOME,        TB_MOD_CTRL                            },
    {"\x1b[7@",      TB_KEY_HOME,        TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b\x1b[7^",  TB_KEY_HOME,        TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[7@",  TB_KEY_HOME,        TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},

    {"\x1b\x1b[8~",  TB_KEY_END,         TB_MOD_ALT                             },
    {"\x1b\x1b[8$",  TB_KEY_END,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[8^",      TB_KEY_END,         TB_MOD_CTRL                            },
    {"\x1b\x1b[8^",  TB_KEY_END,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[8@",  TB_KEY_END,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[8@",      TB_KEY_END,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[8$",      TB_KEY_END,         TB_MOD_SHIFT                           },

    {"\x1b\x1b[2~",  TB_KEY_INSERT,      TB_MOD_ALT                             },
    {"\x1b\x1b[2$",  TB_KEY_INSERT,      TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[2^",      TB_KEY_INSERT,      TB_MOD_CTRL                            },
    {"\x1b\x1b[2^",  TB_KEY_INSERT,      TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[2@",  TB_KEY_INSERT,      TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[2@",      TB_KEY_INSERT,      TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[2$",      TB_KEY_INSERT,      TB_MOD_SHIFT                           },

    {"\x1b\x1b[3~",  TB_KEY_DELETE,      TB_MOD_ALT                             },
    {"\x1b\x1b[3$",  TB_KEY_DELETE,      TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[3^",      TB_KEY_DELETE,      TB_MOD_CTRL                            },
    {"\x1b\x1b[3^",  TB_KEY_DELETE,      TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[3@",  TB_KEY_DELETE,      TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[3@",      TB_KEY_DELETE,      TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[3$",      TB_KEY_DELETE,      TB_MOD_SHIFT                           },

    {"\x1b\x1b[5~",  TB_KEY_PGUP,        TB_MOD_ALT                             },
    {"\x1b\x1b[5$",  TB_KEY_PGUP,        TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[5^",      TB_KEY_PGUP,        TB_MOD_CTRL                            },
    {"\x1b\x1b[5^",  TB_KEY_PGUP,        TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[5@",  TB_KEY_PGUP,        TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[5@",      TB_KEY_PGUP,        TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[5$",      TB_KEY_PGUP,        TB_MOD_SHIFT                           },

    {"\x1b\x1b[6~",  TB_KEY_PGDN,        TB_MOD_ALT                             },
    {"\x1b\x1b[6$",  TB_KEY_PGDN,        TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[6^",      TB_KEY_PGDN,        TB_MOD_CTRL                            },
    {"\x1b\x1b[6^",  TB_KEY_PGDN,        TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[6@",  TB_KEY_PGDN,        TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[6@",      TB_KEY_PGDN,        TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[6$",      TB_KEY_PGDN,        TB_MOD_SHIFT                           },

    {"\x1b\x1b[11~", TB_KEY_F1,          TB_MOD_ALT                             },
    {"\x1b\x1b[23~", TB_KEY_F1,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[11^",     TB_KEY_F1,          TB_MOD_CTRL                            },
    {"\x1b\x1b[11^", TB_KEY_F1,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[23^", TB_KEY_F1,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[23^",     TB_KEY_F1,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[23~",     TB_KEY_F1,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[12~", TB_KEY_F2,          TB_MOD_ALT                             },
    {"\x1b\x1b[24~", TB_KEY_F2,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[12^",     TB_KEY_F2,          TB_MOD_CTRL                            },
    {"\x1b\x1b[12^", TB_KEY_F2,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[24^", TB_KEY_F2,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[24^",     TB_KEY_F2,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[24~",     TB_KEY_F2,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[13~", TB_KEY_F3,          TB_MOD_ALT                             },
    {"\x1b\x1b[25~", TB_KEY_F3,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[13^",     TB_KEY_F3,          TB_MOD_CTRL                            },
    {"\x1b\x1b[13^", TB_KEY_F3,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[25^", TB_KEY_F3,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[25^",     TB_KEY_F3,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[25~",     TB_KEY_F3,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[14~", TB_KEY_F4,          TB_MOD_ALT                             },
    {"\x1b\x1b[26~", TB_KEY_F4,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[14^",     TB_KEY_F4,          TB_MOD_CTRL                            },
    {"\x1b\x1b[14^", TB_KEY_F4,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[26^", TB_KEY_F4,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[26^",     TB_KEY_F4,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[26~",     TB_KEY_F4,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[15~", TB_KEY_F5,          TB_MOD_ALT                             },
    {"\x1b\x1b[28~", TB_KEY_F5,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[15^",     TB_KEY_F5,          TB_MOD_CTRL                            },
    {"\x1b\x1b[15^", TB_KEY_F5,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[28^", TB_KEY_F5,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[28^",     TB_KEY_F5,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[28~",     TB_KEY_F5,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[17~", TB_KEY_F6,          TB_MOD_ALT                             },
    {"\x1b\x1b[29~", TB_KEY_F6,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[17^",     TB_KEY_F6,          TB_MOD_CTRL                            },
    {"\x1b\x1b[17^", TB_KEY_F6,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[29^", TB_KEY_F6,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[29^",     TB_KEY_F6,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[29~",     TB_KEY_F6,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[18~", TB_KEY_F7,          TB_MOD_ALT                             },
    {"\x1b\x1b[31~", TB_KEY_F7,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[18^",     TB_KEY_F7,          TB_MOD_CTRL                            },
    {"\x1b\x1b[18^", TB_KEY_F7,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[31^", TB_KEY_F7,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[31^",     TB_KEY_F7,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[31~",     TB_KEY_F7,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[19~", TB_KEY_F8,          TB_MOD_ALT                             },
    {"\x1b\x1b[32~", TB_KEY_F8,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[19^",     TB_KEY_F8,          TB_MOD_CTRL                            },
    {"\x1b\x1b[19^", TB_KEY_F8,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[32^", TB_KEY_F8,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[32^",     TB_KEY_F8,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[32~",     TB_KEY_F8,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[20~", TB_KEY_F9,          TB_MOD_ALT                             },
    {"\x1b\x1b[33~", TB_KEY_F9,          TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[20^",     TB_KEY_F9,          TB_MOD_CTRL                            },
    {"\x1b\x1b[20^", TB_KEY_F9,          TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[33^", TB_KEY_F9,          TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[33^",     TB_KEY_F9,          TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[33~",     TB_KEY_F9,          TB_MOD_SHIFT                           },

    {"\x1b\x1b[21~", TB_KEY_F10,         TB_MOD_ALT                             },
    {"\x1b\x1b[34~", TB_KEY_F10,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[21^",     TB_KEY_F10,         TB_MOD_CTRL                            },
    {"\x1b\x1b[21^", TB_KEY_F10,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[34^", TB_KEY_F10,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[34^",     TB_KEY_F10,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[34~",     TB_KEY_F10,         TB_MOD_SHIFT                           },

    {"\x1b\x1b[23~", TB_KEY_F11,         TB_MOD_ALT                             },
    {"\x1b\x1b[23$", TB_KEY_F11,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[23^",     TB_KEY_F11,         TB_MOD_CTRL                            },
    {"\x1b\x1b[23^", TB_KEY_F11,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[23@", TB_KEY_F11,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[23@",     TB_KEY_F11,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[23$",     TB_KEY_F11,         TB_MOD_SHIFT                           },

    {"\x1b\x1b[24~", TB_KEY_F12,         TB_MOD_ALT                             },
    {"\x1b\x1b[24$", TB_KEY_F12,         TB_MOD_ALT | TB_MOD_SHIFT              },
    {"\x1b[24^",     TB_KEY_F12,         TB_MOD_CTRL                            },
    {"\x1b\x1b[24^", TB_KEY_F12,         TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1b\x1b[24@", TB_KEY_F12,         TB_MOD_CTRL | TB_MOD_ALT | TB_MOD_SHIFT},
    {"\x1b[24@",     TB_KEY_F12,         TB_MOD_CTRL | TB_MOD_SHIFT             },
    {"\x1b[24$",     TB_KEY_F12,         TB_MOD_SHIFT                           },

    // linux console/putty arrows
    {"\x1b[A",       TB_KEY_ARROW_UP,    TB_MOD_SHIFT                           },
    {"\x1b[B",       TB_KEY_ARROW_DOWN,  TB_MOD_SHIFT                           },
    {"\x1b[C",       TB_KEY_ARROW_RIGHT, TB_MOD_SHIFT                           },
    {"\x1b[D",       TB_KEY_ARROW_LEFT,  TB_MOD_SHIFT                           },

    // more putty arrows
    {"\x1bOA",       TB_KEY_ARROW_UP,    TB_MOD_CTRL                            },
    {"\x1b\x1bOA",   TB_KEY_ARROW_UP,    TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1bOB",       TB_KEY_ARROW_DOWN,  TB_MOD_CTRL                            },
    {"\x1b\x1bOB",   TB_KEY_ARROW_DOWN,  TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1bOC",       TB_KEY_ARROW_RIGHT, TB_MOD_CTRL                            },
    {"\x1b\x1bOC",   TB_KEY_ARROW_RIGHT, TB_MOD_CTRL | TB_MOD_ALT               },
    {"\x1bOD",       TB_KEY_ARROW_LEFT,  TB_MOD_CTRL                            },
    {"\x1b\x1bOD",   TB_KEY_ARROW_LEFT,  TB_MOD_CTRL | TB_MOD_ALT               },

    {NULL,           0,                  0                                      },
};

tb_err tb_init_file(const char* path) {
  if (global.initialized) {
    return TB_ERR_INIT_ALREADY;
  }
  int ttyfd = open(path, O_RDWR);
  if (ttyfd < 0) {
    global.last_errno = errno;
    return TB_ERR_INIT_OPEN;
  }
  global.ttyfd_open = 1;
  return tb_init_fd(ttyfd);
}

tb_err tb_init_fd(int ttyfd) {
  return tb_init_rwfd(ttyfd, ttyfd);
}

tb_err tb_set_func(int fn_type, int (*fn)(struct tb_event*, size_t*)) {
  switch (fn_type) {
    case TB_FUNC_EXTRACT_PRE:
    global.fn_extract_esc_pre = fn;
    return TB_OK;
    case TB_FUNC_EXTRACT_POST:
    global.fn_extract_esc_post = fn;
    return TB_OK;
  }
  return TB_ERR;
}

static tb_err init_term_attrs(void) {
  if (global.ttyfd < 0) {
    return TB_OK;
  }

  if (tcgetattr(global.ttyfd, &global.orig_tios) != 0) {
    global.last_errno = errno;
    return TB_ERR_TCGETATTR;
  }

  struct termios tios;
  memcpy(&tios, &global.orig_tios, sizeof(tios));
  global.has_orig_tios = 1;

  cfmakeraw(&tios);
  tios.c_cc[VMIN] = 1;
  tios.c_cc[VTIME] = 0;

  if (tcsetattr(global.ttyfd, TCSAFLUSH, &tios) != 0) {
    global.last_errno = errno;
    return TB_ERR_TCSETATTR;
  }

  return TB_OK;
}

static tb_err init_term_caps(void) {
  if (load_terminfo() == TB_OK) {
    return parse_terminfo_caps();
  }
  return load_builtin_caps();
}

static tb_err init_cap_trie(void) {
  int rv, i;

  // Add caps from terminfo or built-in
  //
  // Collisions are expected as some terminfo entries have dupes. (For
  // example, att605-pc collides on TB_CAP_F4 and TB_CAP_DELETE.) First cap
  // in TB_CAP_* index order will win.
  //
  // TODO Reorder TB_CAP_* so more critical caps come first.
  for (i = 0; i < TB_CAP__COUNT_KEYS; i++) {
    rv = cap_trie_add(global.caps[i], tb_key_i(i), 0);
    if (rv != TB_OK && rv != TB_ERR_CAP_COLLISION) return rv;
  }

  // Add built-in mod caps
  //
  // Collisions are OK here as well. This can happen if global.caps collides
  // with builtin_mod_caps. It is desirable to give precedence to global.caps
  // here.
  for (i = 0; builtin_mod_caps[i].cap != NULL; i++) {
    rv = cap_trie_add(builtin_mod_caps[i].cap, builtin_mod_caps[i].key,
                      builtin_mod_caps[i].mod);
    if (rv != TB_OK && rv != TB_ERR_CAP_COLLISION) return rv;
  }

  return TB_OK;
}

static tb_err cap_trie_add(const char* cap, uint16_t key, uint8_t mod) {
  struct cap_trie_t* next, * node = &global.cap_trie;
  size_t i, j;

  if (!cap || strlen(cap) <= 0) return TB_OK; // Nothing to do for empty caps

  for (i = 0; cap[i] != '\0'; i++) {
    char c = cap[i];
    next = NULL;

    // Check if c is already a child of node
    for (j = 0; j < node->nchildren; j++) {
      if (node->children[j].c == c) {
        next = &node->children[j];
        break;
      }
    }
    if (!next) {
      // We need to add a new child to node
      node->nchildren += 1;
      node->children =
        tb_realloc(node->children, sizeof(*node) * node->nchildren);
      if (!node->children) {
        return TB_ERR_MEM;
      }
      next = &node->children[node->nchildren - 1];
      memset(next, 0, sizeof(*next));
      next->c = c;
    }

    // Continue
    node = next;
  }

  if (node->is_leaf) {
    // Already a leaf here
    return TB_ERR_CAP_COLLISION;
  }

  node->is_leaf = 1;
  node->key = key;
  node->mod = mod;
  return TB_OK;
}

static tb_err cap_trie_find(const char* buf, size_t nbuf, struct cap_trie_t** last,
                         size_t* depth) {
  struct cap_trie_t* next, * node = &global.cap_trie;
  size_t i, j;
  *last = node;
  *depth = 0;
  for (i = 0; i < nbuf; i++) {
    char c = buf[i];
    next = NULL;

    // Find c in node.children
    for (j = 0; j < node->nchildren; j++) {
      if (node->children[j].c == c) {
        next = &node->children[j];
        break;
      }
    }
    if (!next) {
      // Not found
      return TB_OK;
    }
    node = next;
    *last = node;
    *depth += 1;
    if (node->is_leaf && node->nchildren < 1) {
      break;
    }
  }
  return TB_OK;
}

static tb_err cap_trie_deinit(struct cap_trie_t* node) {
  size_t j;
  for (j = 0; j < node->nchildren; j++) {
    cap_trie_deinit(&node->children[j]);
  }
  if (node->children) {
    tb_free(node->children);
  }
  memset(node, 0, sizeof(*node));
  return TB_OK;
}

static tb_err init_resize_handler(void) {
  if (pipe(global.resize_pipefd) != 0) {
    global.last_errno = errno;
    return TB_ERR_RESIZE_PIPE;
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_resize;
  if (sigaction(SIGWINCH, &sa, NULL) != 0) {
    global.last_errno = errno;
    return TB_ERR_RESIZE_SIGACTION;
  }

  return TB_OK;
}

static tb_err send_init_escape_codes(void) {
  int rv;
  if_err_return(rv, bytebuf_puts(&global.out, global.caps[TB_CAP_ENTER_CA]));
  if_err_return(rv,
                bytebuf_puts(&global.out, global.caps[TB_CAP_ENTER_KEYPAD]));
  if_err_return(rv,
                bytebuf_puts(&global.out, global.caps[TB_CAP_HIDE_CURSOR]));
  return TB_OK;
}

static tb_err update_term_size(void) {
  int rv, ioctl_errno;

  if (global.ttyfd < 0) {
    return TB_OK;
  }

  struct winsize sz;
  memset(&sz, 0, sizeof(sz));

  // Try ioctl TIOCGWINSZ
  if (ioctl(global.ttyfd, TIOCGWINSZ, &sz) == 0) {
    global.width = sz.ws_col;
    global.height = sz.ws_row;
    return TB_OK;
  }
  ioctl_errno = errno;

  // Try >cursor(9999,9999), >u7, <u6
  if_ok_return(rv, update_term_size_via_esc());

  global.last_errno = ioctl_errno;
  return TB_ERR_RESIZE_IOCTL;
}

static tb_err update_term_size_via_esc(void) {
#ifndef TB_RESIZE_FALLBACK_MS
#define TB_RESIZE_FALLBACK_MS 1000
#endif

  char* move_and_report = "\x1b[9999;9999H\x1b[6n";
  ssize_t write_rv =
    write(global.wfd, move_and_report, strlen(move_and_report));
  if (write_rv != (ssize_t)strlen(move_and_report)) {
    return TB_ERR_RESIZE_WRITE;
  }

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(global.rfd, &fds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = TB_RESIZE_FALLBACK_MS * 1000;

  int select_rv = select(global.rfd + 1, &fds, NULL, NULL, &timeout);

  if (select_rv != 1) {
    global.last_errno = errno;
    return TB_ERR_RESIZE_POLL;
  }

  char buf[TB_OPT_READ_BUF];
  ssize_t read_rv = read(global.rfd, buf, sizeof(buf) - 1);
  if (read_rv < 1) {
    global.last_errno = errno;
    return TB_ERR_RESIZE_READ;
  }
  buf[read_rv] = '\0';

  int rw, rh;
  if (sscanf(buf, "\x1b[%d;%dR", &rh, &rw) != 2) {
    return TB_ERR_RESIZE_SSCANF;
  }

  global.width = rw;
  global.height = rh;
  return TB_OK;
}

static tb_err load_terminfo(void) {
  int rv;
  char tmp[TB_PATH_MAX];

  // See terminfo(5) "Fetching Compiled Descriptions" for a description of
  // this behavior. Some of these paths are compile-time ncurses options, so
  // best guesses are used here.
  const char* term = getenv("TERM");
  if (!term) {
    return TB_ERR;
  }

  // If TERMINFO is set, try that directory and stop
  const char* terminfo = getenv("TERMINFO");
  if (terminfo) {
    return load_terminfo_from_path(terminfo, term);
  }

  // Next try ~/.terminfo
  const char* home = getenv("HOME");
  if (home) {
    snprintf_or_return(rv, tmp, sizeof(tmp), "%s/.terminfo", home);
    if_ok_return(rv, load_terminfo_from_path(tmp, term));
  }

  // Next try TERMINFO_DIRS
  //
  // Note, empty entries are supposed to be interpretted as the "compiled-in
  // default", which is of course system-dependent. Previously /etc/terminfo
  // was used here. Let's skip empty entries altogether rather than give
  // precedence to a guess, and check common paths after this loop.
  const char* dirs = getenv("TERMINFO_DIRS");
  if (dirs) {
    snprintf_or_return(rv, tmp, sizeof(tmp), "%s", dirs);
    char* dir = strtok(tmp, ":");
    while (dir) {
      const char* cdir = dir;
      if (*cdir != '\0') {
        if_ok_return(rv, load_terminfo_from_path(cdir, term));
      }
      dir = strtok(NULL, ":");
    }
  }

#ifdef TB_TERMINFO_DIR
  if_ok_return(rv, load_terminfo_from_path(TB_TERMINFO_DIR, term));
#endif
  if_ok_return(rv, load_terminfo_from_path("/usr/local/etc/terminfo", term));
  if_ok_return(rv,
               load_terminfo_from_path("/usr/local/share/terminfo", term));
  if_ok_return(rv, load_terminfo_from_path("/usr/local/lib/terminfo", term));
  if_ok_return(rv, load_terminfo_from_path("/etc/terminfo", term));
  if_ok_return(rv, load_terminfo_from_path("/usr/share/terminfo", term));
  if_ok_return(rv, load_terminfo_from_path("/usr/lib/terminfo", term));
  if_ok_return(rv, load_terminfo_from_path("/usr/share/lib/terminfo", term));
  if_ok_return(rv, load_terminfo_from_path("/lib/terminfo", term));

  return TB_ERR;
}

static tb_err load_terminfo_from_path(const char* path, const char* term) {
  int rv;
  char tmp[TB_PATH_MAX];

  // Look for term at this terminfo location, e.g., <terminfo>/x/xterm
  snprintf_or_return(rv, tmp, sizeof(tmp), "%s/%c/%s", path, term[0], term);
  if_ok_return(rv, read_terminfo_path(tmp));

#ifdef __APPLE__
  // Try the Darwin equivalent path, e.g., <terminfo>/78/xterm
  snprintf_or_return(rv, tmp, sizeof(tmp), "%s/%x/%s", path, term[0], term);
  return read_terminfo_path(tmp);
#endif

  return TB_ERR;
}

static tb_err read_terminfo_path(const char* path) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    return TB_ERR;
  }

  struct stat st;
  if (fstat(fileno(fp), &st) != 0) {
    fclose(fp);
    return TB_ERR;
  }

  size_t fsize = st.st_size;
  char* data = tb_malloc(fsize);
  if (!data) {
    fclose(fp);
    return TB_ERR;
  }

  if (fread(data, 1, fsize, fp) != fsize) {
    fclose(fp);
    tb_free(data);
    return TB_ERR;
  }

  global.terminfo = data;
  global.nterminfo = fsize;

  fclose(fp);
  return TB_OK;
}

static tb_err parse_terminfo_caps(void) {
  // See term(5) "LEGACY STORAGE FORMAT" and "EXTENDED STORAGE FORMAT" for a
  // description of this behavior.

  // Ensure there's at least a header's worth of data
  if (global.nterminfo < 6) {
    return TB_ERR;
  }

  int16_t* header = (int16_t*)global.terminfo;
  // header[0] the magic number (octal 0432 or 01036)
  // header[1] the size, in bytes, of the names section
  // header[2] the number of bytes in the boolean section
  // header[3] the number of short integers in the numbers section
  // header[4] the number of offsets (short integers) in the strings section
  // header[5] the size, in bytes, of the string table

  // Legacy ints are 16-bit, extended ints are 32-bit
  const int bytes_per_int = header[0] == 01036 ? 4  // 32-bit
    : 2; // 16-bit

  // > Between the boolean section and the number section, a null byte will be
  // > inserted, if necessary, to ensure that the number section begins on an
  // > even byte
  const int align_offset = (header[1] + header[2]) % 2 != 0 ? 1 : 0;

  const int pos_str_offsets =
    (6 * sizeof(int16_t)) // header (12 bytes)
    + header[1]           // length of names section
    + header[2]           // length of boolean section
    + align_offset +
    (header[3] * bytes_per_int); // length of numbers section

  const int pos_str_table =
    pos_str_offsets +
    (header[4] * sizeof(int16_t)); // length of string offsets table

  // Load caps
  int i;
  for (i = 0; i < TB_CAP__COUNT; i++) {
    const char* cap = get_terminfo_string(pos_str_offsets, header[4],
                                          pos_str_table, header[5], terminfo_cap_indexes[i]);
    if (!cap) {
      // Something is not right
      return TB_ERR;
    }
    global.caps[i] = cap;
  }

  return TB_OK;
}

static tb_err load_builtin_caps(void) {
  int i, j;
  const char* term = getenv("TERM");

  if (!term) {
    return TB_ERR_NO_TERM;
  }

  // Check for exact TERM match
  for (i = 0; builtin_terms[i].name != NULL; i++) {
    if (strcmp(term, builtin_terms[i].name) == 0) {
      for (j = 0; j < TB_CAP__COUNT; j++) {
        global.caps[j] = builtin_terms[i].caps[j];
      }
      return TB_OK;
    }
  }

  // Check for partial TERM or alias match
  for (i = 0; builtin_terms[i].name != NULL; i++) {
    if (strstr(term, builtin_terms[i].name) != NULL ||
        (*(builtin_terms[i].alias) != '\0' &&
         strstr(term, builtin_terms[i].alias) != NULL))
    {
      for (j = 0; j < TB_CAP__COUNT; j++) {
        global.caps[j] = builtin_terms[i].caps[j];
      }
      return TB_OK;
    }
  }

  return TB_ERR_UNSUPPORTED_TERM;
}

static const char* get_terminfo_string(int16_t str_offsets_pos,
                                       int16_t str_offsets_len, int16_t str_table_pos, int16_t str_table_len,
                                       int16_t str_index) {
  const int str_byte_index = (int)str_index * (int)sizeof(int16_t);
  if (str_byte_index >= (int)str_offsets_len * (int)sizeof(int16_t)) {
    // An offset beyond the table indicates absent
    // See `convert_strings` in tinfo `read_entry.c`
    return "";
  }
  const int16_t* str_offset =
    (int16_t*)(global.terminfo + (int)str_offsets_pos + str_byte_index);
  if ((char*)str_offset >= global.terminfo + global.nterminfo) {
    // str_offset points beyond end of entry
    // Truncated/corrupt terminfo entry?
    return NULL;
  }
  if (*str_offset < 0 || *str_offset >= str_table_len) {
    // A negative offset indicates absent
    // An offset beyond the table indicates absent
    // See `convert_strings` in tinfo `read_entry.c`
    return "";
  }
  if (((size_t)((int)str_table_pos + (int)*str_offset)) >= global.nterminfo) {
    // string points beyond end of entry
    // Truncated/corrupt terminfo entry?
    return NULL;
  }
  return (
    const char*)(global.terminfo + (int)str_table_pos + (int)*str_offset);
}

static tb_err wait_event(struct tb_event* event, int timeout) {
  int rv;
  char buf[TB_OPT_READ_BUF];

  memset(event, 0, sizeof(*event));
  if_ok_return(rv, extract_event(event));

  fd_set fds;
  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout - (tv.tv_sec * 1000)) * 1000;

  do {
    FD_ZERO(&fds);
    FD_SET(global.rfd, &fds);
    FD_SET(global.resize_pipefd[0], &fds);

    int maxfd = global.resize_pipefd[0] > global.rfd
      ? global.resize_pipefd[0]
      : global.rfd;

    int select_rv =
      select(maxfd + 1, &fds, NULL, NULL, (timeout < 0) ? NULL : &tv);

    if (select_rv < 0) {
      // Let EINTR/EAGAIN bubble up
      global.last_errno = errno;
      return TB_ERR_POLL;
    } else if (select_rv == 0) {
      return TB_ERR_NO_EVENT;
    }

    int tty_has_events = (FD_ISSET(global.rfd, &fds));
    int resize_has_events = (FD_ISSET(global.resize_pipefd[0], &fds));

    if (tty_has_events) {
      ssize_t read_rv = read(global.rfd, buf, sizeof(buf));
      if (read_rv < 0) {
        global.last_errno = errno;
        return TB_ERR_READ;
      } else if (read_rv > 0) {
        bytebuf_nputs(&global.in, buf, read_rv);
      }
    }

    if (resize_has_events) {
      int ignore = 0;
      read(global.resize_pipefd[0], &ignore, sizeof(ignore));
      // TODO Harden against errors encountered mid-resize
      if_err_return(rv, update_term_size());
      if_err_return(rv, resize_cellbufs());
      event->type = TB_EVENT_RESIZE;
      event->w = global.width;
      event->h = global.height;
      return TB_OK;
    }

    memset(event, 0, sizeof(*event));
    if_ok_return(rv, extract_event(event));
  } while (timeout == -1);

  return rv;
}

static tb_err extract_event(struct tb_event* event) {
  int rv;
  struct bytebuf_t* in = &global.in;

  if (in->len == 0) {
    return TB_ERR;
  }

  if (in->buf[0] == '\x1b') {
    // Escape sequence?
    // In TB_INPUT_ESC, skip if the buffer is a single escape char
    if (!((global.input_mode & TB_INPUT_ESC) && in->len == 1)) {
      if_ok_or_need_more_return(rv, extract_esc(event));
    }

    // Escape key?
    if (global.input_mode & TB_INPUT_ESC) {
      event->type = TB_EVENT_KEY;
      event->ch = 0;
      event->key = TB_KEY_ESC;
      event->mod = 0;
      bytebuf_shift(in, 1);
      return TB_OK;
    }

    // Recurse for alt key
    event->mod |= TB_MOD_ALT;
    bytebuf_shift(in, 1);
    return extract_event(event);
  }

  // ASCII control key?
  if ((uint16_t)in->buf[0] < TB_KEY_SPACE || in->buf[0] == TB_KEY_BACKSPACE2)
  {
    event->type = TB_EVENT_KEY;
    event->ch = 0;
    event->key = (uint16_t)in->buf[0];
    event->mod |= TB_MOD_CTRL;
    bytebuf_shift(in, 1);
    return TB_OK;
  }

  // UTF-8?
  if (in->len >= (size_t)tb_utf8_char_length(in->buf[0])) {
    event->type = TB_EVENT_KEY;
    tb_utf8_char_to_unicode(&event->ch, in->buf);
    event->key = 0;
    bytebuf_shift(in, tb_utf8_char_length(in->buf[0]));
    return TB_OK;
  }

  // Need more input
  return TB_ERR;
}

static tb_err extract_esc(struct tb_event* event) {
  int rv;
  if_ok_or_need_more_return(rv, extract_esc_user(event, 0));
  if_ok_or_need_more_return(rv, extract_esc_cap(event));
  if_ok_or_need_more_return(rv, extract_esc_mouse(event));
  if_ok_or_need_more_return(rv, extract_esc_user(event, 1));
  return TB_ERR;
}

static tb_err extract_esc_user(struct tb_event* event, int is_post) {
  int rv;
  size_t consumed = 0;
  struct bytebuf_t* in = &global.in;
  int (*fn)(struct tb_event*, size_t*);

  fn = is_post ? global.fn_extract_esc_post : global.fn_extract_esc_pre;

  if (!fn) {
    return TB_ERR;
  }

  rv = fn(event, &consumed);
  if (rv == TB_OK) {
    bytebuf_shift(in, consumed);
  }

  if_ok_or_need_more_return(rv, rv);
  return TB_ERR;
}

static tb_err extract_esc_cap(struct tb_event* event) {
  int rv;
  struct bytebuf_t* in = &global.in;
  struct cap_trie_t* node;
  size_t depth;

  if_err_return(rv, cap_trie_find(in->buf, in->len, &node, &depth));
  if (node->is_leaf) {
    // Found a leaf node
    event->type = TB_EVENT_KEY;
    event->ch = 0;
    event->key = node->key;
    event->mod = node->mod;
    bytebuf_shift(in, depth);
    return TB_OK;
  } else if (node->nchildren > 0 && in->len <= depth) {
    // Found a branch node (not enough input)
    return TB_ERR_NEED_MORE;
  }

  return TB_ERR;
}

static tb_err extract_esc_mouse(struct tb_event* event) {
  struct bytebuf_t* in = &global.in;

  enum type { TYPE_VT200 = 0, TYPE_1006, TYPE_1015, TYPE_MAX };

  char* cmp[TYPE_MAX] = {//
    // X10 mouse encoding, the simplest one
    // \x1b [ M Cb Cx Cy
    [TYPE_VT200] = "\x1b[M",
    // xterm 1006 extended mode or urxvt 1015 extended mode
    // xterm: \x1b [ < Cb ; Cx ; Cy (M or m)
    [TYPE_1006] = "\x1b[<",
    // urxvt: \x1b [ Cb ; Cx ; Cy M
    [TYPE_1015] = "\x1b[" };

  enum type type = 0;
  int ret = TB_ERR;

  // Unrolled at compile-time (probably)
  for (; type < TYPE_MAX; type++) {
    size_t size = strlen(cmp[type]);

    if (in->len >= size && (strncmp(cmp[type], in->buf, size)) == 0) {
      break;
    }
  }

  if (type == TYPE_MAX) {
    ret = TB_ERR; // No match
    return ret;
  }

  size_t buf_shift = 0;

  switch (type) {
    case TYPE_VT200:
    if (in->len >= 6) {
      int b = in->buf[3] - 0x20;
      int fail = 0;

      switch (b & 3) {
        case 0:
        event->key = ((b & 64) != 0) ? TB_KEY_MOUSE_WHEEL_UP
          : TB_KEY_MOUSE_LEFT;
        break;
        case 1:
        event->key = ((b & 64) != 0) ? TB_KEY_MOUSE_WHEEL_DOWN
          : TB_KEY_MOUSE_MIDDLE;
        break;
        case 2:
        event->key = TB_KEY_MOUSE_RIGHT;
        break;
        case 3:
        event->key = TB_KEY_MOUSE_RELEASE;
        break;
        default:
        ret = TB_ERR;
        fail = 1;
        break;
      }

      if (!fail) {
        if ((b & 32) != 0) {
          event->mod |= TB_MOD_MOTION;
        }

        // the coord is 1,1 for upper left
        event->x = ((uint8_t)in->buf[4]) - 0x21;
        event->y = ((uint8_t)in->buf[5]) - 0x21;

        ret = TB_OK;
      }

      buf_shift = 6;
    }
    break;
    case TYPE_1006:
    // fallthrough
    case TYPE_1015:
    {
      size_t index_fail = (size_t)-1;

      enum {
        FIRST_M = 0,
        FIRST_SEMICOLON,
        LAST_SEMICOLON,
        FIRST_LAST_MAX
      };

      size_t indices[FIRST_LAST_MAX] = { index_fail, index_fail,
          index_fail };
      int m_is_capital = 0;

      for (size_t i = 0; i < in->len; i++) {
        if (in->buf[i] == ';') {
          if (indices[FIRST_SEMICOLON] == index_fail) {
            indices[FIRST_SEMICOLON] = i;
          } else {
            indices[LAST_SEMICOLON] = i;
          }
        } else if (indices[FIRST_M] == index_fail) {
          if (in->buf[i] == 'm' || in->buf[i] == 'M') {
            m_is_capital = (in->buf[i] == 'M');
            indices[FIRST_M] = i;
          }
        }
      }

      if (indices[FIRST_M] == index_fail ||
          indices[FIRST_SEMICOLON] == index_fail ||
          indices[LAST_SEMICOLON] == index_fail)
      {
        ret = TB_ERR;
      } else {
        int start = (type == TYPE_1015 ? 2 : 3);

        unsigned n1 = strtoul(&in->buf[start], NULL, 10);
        unsigned n2 =
          strtoul(&in->buf[indices[FIRST_SEMICOLON] + 1], NULL, 10);
        unsigned n3 =
          strtoul(&in->buf[indices[LAST_SEMICOLON] + 1], NULL, 10);

        if (type == TYPE_1015) {
          n1 -= 0x20;
        }

        int fail = 0;

        switch (n1 & 3) {
          case 0:
          event->key = ((n1 & 64) != 0) ? TB_KEY_MOUSE_WHEEL_UP
            : TB_KEY_MOUSE_LEFT;
          break;
          case 1:
          event->key = ((n1 & 64) != 0) ? TB_KEY_MOUSE_WHEEL_DOWN
            : TB_KEY_MOUSE_MIDDLE;
          break;
          case 2:
          event->key = TB_KEY_MOUSE_RIGHT;
          break;
          case 3:
          event->key = TB_KEY_MOUSE_RELEASE;
          break;
          default:
          ret = TB_ERR;
          fail = 1;
          break;
        }

        buf_shift = in->len;

        if (!fail) {
          if (!m_is_capital) {
            // on xterm mouse release is signaled by lowercase m
            event->key = TB_KEY_MOUSE_RELEASE;
          }

          if ((n1 & 32) != 0) {
            event->mod |= TB_MOD_MOTION;
          }

          event->x = ((uint8_t)n2) - 1;
          event->y = ((uint8_t)n3) - 1;

          ret = TB_OK;
        }
      }
    } break;
    case TYPE_MAX:
    ret = TB_ERR;
  }

  if (buf_shift > 0) {
    bytebuf_shift(in, buf_shift);
  }

  if (ret == TB_OK) {
    event->type = TB_EVENT_MOUSE;
  }

  return ret;
}

static void handle_resize(int sig) {
  int errno_copy = errno;
  write(global.resize_pipefd[1], &sig, sizeof(sig));
  errno = errno_copy;
}

tb_err tb_get_fds(int* ttyfd, int* resizefd) {
  if_not_init_return();

  *ttyfd = global.rfd;
  *resizefd = global.resize_pipefd[0];

  return TB_OK;
}


#endif // POSIX

// ----------------------------------------------------------------------------
// SECTION WINDOWS INTERNAL HEADER
// ----------------------------------------------------------------------------

#if defined(_WIN32)

// Here we'll just use xterm caps hardcoded unlike the posix version which will
// initialize the caps dynamically based on the terminal it's running on.
static tb_err init_term_caps();

static bool to_tb_key(WORD vk, tb_key* kc, const uint8_t mod);
static bool to_tb_event(INPUT_RECORD* ir, struct tb_event* ev);
// An int is here just because posix need and not used in windows.
static tb_err wait_event(struct tb_event* ev, int);

static tb_err init_term_caps() {
  global.caps[TB_CAP_F1] = "\033OP";
  global.caps[TB_CAP_F2] = "\033OQ";
  global.caps[TB_CAP_F3] = "\033OR";
  global.caps[TB_CAP_F4] = "\033OS";
  global.caps[TB_CAP_F5] = "\033[15~";
  global.caps[TB_CAP_F6] = "\033[17~";
  global.caps[TB_CAP_F7] = "\033[18~";
  global.caps[TB_CAP_F8] = "\033[19~";
  global.caps[TB_CAP_F9] = "\033[20~";
  global.caps[TB_CAP_F10] = "\033[21~";
  global.caps[TB_CAP_F11] = "\033[23~";
  global.caps[TB_CAP_F12] = "\033[24~";
  global.caps[TB_CAP_INSERT] = "\033[2~";
  global.caps[TB_CAP_DELETE] = "\033[3~";
  global.caps[TB_CAP_HOME] = "\033OH";
  global.caps[TB_CAP_END] = "\033OF";
  global.caps[TB_CAP_PGUP] = "\033[5~";
  global.caps[TB_CAP_PGDN] = "\033[6~";
  global.caps[TB_CAP_ARROW_UP] = "\033OA";
  global.caps[TB_CAP_ARROW_DOWN] = "\033OB";
  global.caps[TB_CAP_ARROW_LEFT] = "\033OD";
  global.caps[TB_CAP_ARROW_RIGHT] = "\033OC";
  global.caps[TB_CAP_BACK_TAB] = "\033[Z";
  global.caps[TB_CAP_ENTER_CA] = "\033[?1049h\033[22;0;0t";
  global.caps[TB_CAP_EXIT_CA] = "\033[?1049l\033[23;0;0t";
  global.caps[TB_CAP_SHOW_CURSOR] = "\033[?12l\033[?25h";
  global.caps[TB_CAP_HIDE_CURSOR] = "\033[?25l";
  global.caps[TB_CAP_CLEAR_SCREEN] = "\033[H\033[2J";
  global.caps[TB_CAP_SGR0] = "\033(B\033[m";
  global.caps[TB_CAP_UNDERLINE] = "\033[4m";
  global.caps[TB_CAP_BOLD] = "\033[1m";
  global.caps[TB_CAP_BLINK] = "\033[5m";
  global.caps[TB_CAP_ITALIC] = "\033[3m";
  global.caps[TB_CAP_REVERSE] = "\033[7m";
  global.caps[TB_CAP_ENTER_KEYPAD] = "\033[?1h\033=";
  global.caps[TB_CAP_EXIT_KEYPAD] = "\033[?1l\033>";
  global.caps[TB_CAP_DIM] = "\033[2m";
  global.caps[TB_CAP_INVISIBLE] = "\033[8m";

  return TB_OK;
}

// Convert windows virtual keys to tb_key. Returns false if the key
// should be ignored as an event (ex: shift, ctrl, ...).
static bool to_tb_key(WORD vk, tb_key* kc, const uint8_t mod) {

  if (0x70 <= vk && vk <= 0x7b) { *kc = (tb_key)(TB_KEY_F1 + (vk - 0x70)); return true; }

  // Note that shift, ctrl, alt keys are returned as TERM_KEY_UNKNOWN
  // since *nix systems don't support them.
  switch (vk) {
    case VK_BACK:   *kc = TB_KEY_BACKSPACE; return true;
    case VK_TAB:    *kc = TB_KEY_TAB; return true;
    case VK_RETURN: *kc = TB_KEY_ENTER; return true;
    case VK_ESCAPE: *kc = TB_KEY_ESC; return true;
    case VK_SPACE:  *kc = TB_KEY_SPACE; return true;
    case VK_PRIOR:  *kc = TB_KEY_PGUP; return true;
    case VK_NEXT:   *kc = TB_KEY_PGDN; return true;
    case VK_END:    *kc = TB_KEY_END; return true;
    case VK_HOME:   *kc = TB_KEY_HOME; return true;
    case VK_UP:     *kc = TB_KEY_ARROW_UP; return true;
    case VK_DOWN:   *kc = TB_KEY_ARROW_DOWN; return true;
    case VK_LEFT:   *kc = TB_KEY_ARROW_LEFT; return true;
    case VK_RIGHT:  *kc = TB_KEY_ARROW_RIGHT; return true;
    case VK_INSERT: *kc = TB_KEY_INSERT; return true;
    case VK_DELETE: *kc = TB_KEY_DELETE; return true;

    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_PAUSE:
    case VK_CAPITAL: // Capslock.
    return false;
  }

  // ctrl + (0 to 9)
  if (mod & TB_MOD_CTRL && (0x30 <= vk && vk <= 0x39)) {
    switch (vk) {
      //case 0x30: *kc = TB_KEY_CTRL_0; return true;
      //case 0x31: *kc = TB_KEY_CTRL_1; return true;
      case 0x32: *kc = TB_KEY_CTRL_2; return true;
      case 0x33: *kc = TB_KEY_CTRL_3; return true;
      case 0x34: *kc = TB_KEY_CTRL_4; return true;
      case 0x35: *kc = TB_KEY_CTRL_5; return true;
      case 0x36: *kc = TB_KEY_CTRL_6; return true;
      case 0x37: *kc = TB_KEY_CTRL_7; return true;
      case 0x38: *kc = TB_KEY_CTRL_8; return true;
      //case 0x39: *kc = TB_KEY_CTRL_9; return true;
    }

    return false;
  }

  // ctrl + a-z, A-Z
  if (mod & TB_MOD_CTRL){
    switch (vk) {
      case 'a': case 'A': *kc = TB_KEY_CTRL_A; return true;
      case 'b': case 'B': *kc = TB_KEY_CTRL_B; return true;
      case 'c': case 'C': *kc = TB_KEY_CTRL_C; return true;
      case 'd': case 'D': *kc = TB_KEY_CTRL_D; return true;
      case 'e': case 'E': *kc = TB_KEY_CTRL_E; return true;
      case 'f': case 'F': *kc = TB_KEY_CTRL_F; return true;
      case 'g': case 'G': *kc = TB_KEY_CTRL_G; return true;
      case 'h': case 'H': *kc = TB_KEY_CTRL_H; return true;
      case 'i': case 'I': *kc = TB_KEY_CTRL_I; return true;
      case 'j': case 'J': *kc = TB_KEY_CTRL_J; return true;
      case 'k': case 'K': *kc = TB_KEY_CTRL_K; return true;
      case 'l': case 'L': *kc = TB_KEY_CTRL_L; return true;
      case 'm': case 'M': *kc = TB_KEY_CTRL_M; return true;
      case 'n': case 'N': *kc = TB_KEY_CTRL_N; return true;
      case 'o': case 'O': *kc = TB_KEY_CTRL_O; return true;
      case 'p': case 'P': *kc = TB_KEY_CTRL_P; return true;
      case 'q': case 'Q': *kc = TB_KEY_CTRL_Q; return true;
      case 'r': case 'R': *kc = TB_KEY_CTRL_R; return true;
      case 's': case 'S': *kc = TB_KEY_CTRL_S; return true;
      case 't': case 'T': *kc = TB_KEY_CTRL_T; return true;
      case 'u': case 'U': *kc = TB_KEY_CTRL_U; return true;
      case 'v': case 'V': *kc = TB_KEY_CTRL_V; return true;
      case 'w': case 'W': *kc = TB_KEY_CTRL_W; return true;
      case 'x': case 'X': *kc = TB_KEY_CTRL_X; return true;
      case 'y': case 'Y': *kc = TB_KEY_CTRL_Y; return true;
      case 'z': case 'Z': *kc = TB_KEY_CTRL_Z; return true;
    }
  }

  return false;
}

// Returns true if the convertion is success otherwise the event should be ignored
// for example key up event is ignored by us since it's also ignored in *nix systems.
static bool to_tb_event(INPUT_RECORD* ir, struct tb_event* ev) {

  switch (ir->EventType) {

    case WINDOW_BUFFER_SIZE_EVENT:
    {
      const WINDOW_BUFFER_SIZE_RECORD* wbs = &ir->Event.WindowBufferSizeEvent;
      ev->type = TB_EVENT_RESIZE;
      ev->w = wbs->dwSize.X;
      ev->h = wbs->dwSize.Y;
      global.width = ev->w;
      global.height = ev->h;
      return true;
    } break;

    case KEY_EVENT:
    {
      const KEY_EVENT_RECORD* ker = &ir->Event.KeyEvent;

      // Note: Key up event not available in *nix systems. So we're ignoring
      // here as well.
      if (!ker->bKeyDown) return false;

      ev->mod = 0x0;

      const DWORD state = ker->dwControlKeyState;
      if ((state & LEFT_ALT_PRESSED) || (state & RIGHT_ALT_PRESSED)) {
        ev->mod = (tb_mod)((uint8_t)ev->mod | TB_MOD_ALT);
      }

      if ((state & LEFT_CTRL_PRESSED) || (state & RIGHT_CTRL_PRESSED)) {
        ev->mod = (tb_mod)((uint8_t)ev->mod | TB_MOD_CTRL);
      }

      if (state & SHIFT_PRESSED) {
        ev->mod = (tb_mod)((uint8_t)ev->mod | TB_MOD_SHIFT);
      }

      ev->type = TB_EVENT_KEY;
      ev->ch = ker->uChar.UnicodeChar;

      // Check if it's an actual character entered.
      switch (ev->ch) {
        case 0x00:
        case '\x1b': // Escape key pressed.
        case '\b':   // backspace.
        case '\t':   // Tab.
        case '\r':   // Enter.
          ev->ch = 0x0;
          ev->key = 0x0;
          break;

        // If a key is pressed with ctrl or alt is not a character enter event. 
        default:
          if ((ev->mod & TB_MOD_CTRL) ||
              (ev->mod & TB_MOD_ALT)) {
            ev->ch = 0x0;
          } else {
            ev->key = 0x0;
          }
          break;
      }

      if (ev->ch != 0x0) return true;
      return to_tb_key(ker->wVirtualKeyCode, &ev->key, ev->mod);

    } break;


    case MOUSE_EVENT:
    {

      const MOUSE_EVENT_RECORD* mer = &ir->Event.MouseEvent;

      ev->type = TB_EVENT_MOUSE;
      ev->x = mer->dwMousePosition.X;
      ev->y = mer->dwMousePosition.Y;

      const DWORD state = mer->dwControlKeyState;
      if ((state & LEFT_ALT_PRESSED) || (state & RIGHT_ALT_PRESSED)) {
        ev->mod = (tb_mod)((uint8_t)ev->mod | TB_MOD_ALT);
      }

      if ((state & LEFT_CTRL_PRESSED) || (state & RIGHT_CTRL_PRESSED)) {
        ev->mod = (tb_mod)((uint8_t)ev->mod | TB_MOD_CTRL);
      }

      if (state & SHIFT_PRESSED) {
        ev->mod = (tb_mod)((uint8_t)ev->mod | TB_MOD_SHIFT);
      }

      // https://learn.microsoft.com/en-us/windows/console/mouse-event-record-str
      // 
      // MOUSE_WHEELED 0x0004	The vertical mouse wheel was moved.
      //
      // If the high word of the dwButtonState member contains a positive
      // value, the wheel was rotated forward, away from the user. Otherwise,
      // the wheel was rotated backward, toward the user.
      if (mer->dwEventFlags & MOUSE_WHEELED) {
        if (HIWORD(mer->dwButtonState) & 0x8000) {
          ev->key = TB_KEY_MOUSE_WHEEL_UP;
        } else {
          ev->key = TB_KEY_MOUSE_WHEEL_DOWN;
        }
        return true;
      }

      // if dwEventFlags == 0, it's either a mouse button pressed or released.
      // We consider click, release and drag here (just the mouse move will be ignored).
      if (mer->dwEventFlags == 0 || (mer->dwEventFlags & MOUSE_MOVED)) {

        // Current mouse state as a bit field where each bits correspond to a
        // mouse button, if set the button is pressed, if 0 the button is released.
        const DWORD button_state = mer->dwButtonState;

        // btn_chagne will bit set which buttons are changed (from 0 to 1 or vise versa).
        const DWORD btn_change = global.button_state ^ button_state;

        // Note: Even though windows can give which button was released, we just
        // set mouse released event (without specifing the button) since we want
        // to mimic termbox's behavior.
        //
        //   released = (btn_change & (~new_state))
        //
        // xor will set bits which has changed (from 0 to 1 or vise versa) ~new will
        // set bits which are not pressed (released) the total operation will result bits
        // which have changed and released.
        const bool released = ((btn_change & (~button_state)) != 0);

        global.button_state = button_state;

        // Note: if no button changed and no buttons are pressed the event is
        // just mouse move (not even drag) and we don't consider that as an event
        // since we want to mimic termbox's behavior.
        if (btn_change == 0 && button_state == 0) return false;

        if (released) {
          ev->key = TB_KEY_MOUSE_RELEASE;
          return true;

        } else {
          if (button_state & FROM_LEFT_1ST_BUTTON_PRESSED) {
            ev->key = TB_KEY_MOUSE_LEFT;
          } else if (button_state & RIGHTMOST_BUTTON_PRESSED) {
            ev->key = TB_KEY_MOUSE_RIGHT;
          } else if (button_state & FROM_LEFT_2ND_BUTTON_PRESSED) {
            ev->key = TB_KEY_MOUSE_MIDDLE;
          }
        }

        return true;
      }

      // TODO: What to do with the bellow ??
      //if (mer->dwEventFlags & DOUBLE_CLICK);

      return false;
    } break;

    // Not handling as it's not available in *nix.
    case MENU_EVENT:
    case FOCUS_EVENT:
    break; // Will return false at the end.
  }

  // Valid events will return true above and if we reached here, the event is not parsed.
  return false;
}

static tb_err wait_event(struct tb_event* ev, int timeout) {
  tb_err rv;

  INPUT_RECORD record;

  // Since some win32 events are not considered as valid events in termbox
  // (like just the mouse move or key press of ctrl key), we set it not done
  // and read another event.
  bool done = false;
  do {

    // Since it support reading multiple events at a time this will be set however
    // we're only reading one event so we can ignore the value of it.
    DWORD count;
    if (!ReadConsoleInput(global.h_stdin, &record, 1, &count)) {
      return TB_ERR_WIN32;
    }

    memset(ev, 0, sizeof(*ev));
    if (to_tb_event(&record, ev)) {
      if (ev->type == TB_EVENT_RESIZE) {
        global.width = ev->w;
        global.height = ev->h;
        if_err_return(rv, resize_cellbufs());
      }
      done = true;
    }
  } while (!done);

  return TB_OK;
}

#endif

// ----------------------------------------------------------------------------
// SECTION TB IMPLEMENTATION
// ----------------------------------------------------------------------------

static const unsigned char utf8_length[256] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1 };

static const unsigned char utf8_mask[6] = { 0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };


tb_err tb_init(void) {
#if defined(__unix__) || defined(__APPLE__)
  return tb_init_file("/dev/tty");
#elif defined(_WIN32)
  return tb_init_rwfd(fileno(stdin), fileno(stdout));
#endif
}

tb_err tb_init_rwfd(int rfd, int wfd) {
  int rv;

  tb_reset();

  global.rfd = rfd;
  global.wfd = wfd;

#if defined(__unix__) || defined(__APPLE__)
  global.ttyfd = rfd == wfd && isatty(rfd) ? rfd : -1;
#endif

  // Initialize windows terminal.
#ifdef _WIN32
  // Backup the initial modes.
  global.h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  global.h_stdin = GetStdHandle(STD_INPUT_HANDLE);

  if (!GetConsoleMode(global.h_stdout, &global.mode_out)) return TB_ERR_WIN32;
  if (!GetConsoleMode(global.h_stdin, &global.mode_in)) return TB_ERR_WIN32;

  // Set new modes for the stdin, stdout.
  if (!SetConsoleMode(global.h_stdout, global.mode_out | ENABLE_VIRTUALINAL_PROCESSING)) return TB_ERR_WIN32;
  if (!SetConsoleMode(global.h_stdin, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)) return TB_ERR_WIN32;

  // Set utf8 mode.
  if (!SetConsoleOutputCP(CP_UTF8)) return TB_ERR_WIN32;

  // We need to set width and height here to allocate the buffer bellow, otherwise it'll
  // fail to allocate.
  CONSOLE_SCREEN_BUFFER_INFO binfo;
  if (!GetConsoleScreenBufferInfo(global.h_stdout, &binfo)) return TB_ERR_WIN32;
  global.width = binfo.srWindow.Right - binfo.srWindow.Left + 1;
  global.height = binfo.srWindow.Bottom - binfo.srWindow.Top + 1;
#endif


  do {
#ifdef _WIN32
    if_err_break(rv, init_term_caps());
#endif

#if defined(__unix__) || defined(__APPLE__)
    if_err_break(rv, init_term_attrs());
    if_err_break(rv, init_term_caps());
    if_err_break(rv, init_cap_trie());
    if_err_break(rv, init_resize_handler());
    if_err_break(rv, send_init_escape_codes());
#endif
    if_err_break(rv, send_clear());
#if defined(__unix__) || defined(__APPLE__)
    if_err_break(rv, update_term_size());
#endif

    if_err_break(rv, init_cellbuf());
    global.initialized = 1;
  } while (0);

  if (rv != TB_OK) {
    tb_deinit();
  }

  // This will be handled in posix with the call send_init_escape_codes() call.
  // Maybe I should call this somewhere else.
#ifdef _WIN32
  if_err_return(rv, bytebuf_puts(&global.out, global.caps[TB_CAP_HIDE_CURSOR]));
#endif

  return rv;
}

tb_err tb_shutdown(void) {
  if_not_init_return();
  tb_deinit();
  return TB_OK;
}

int tb_width(void) {
  if_not_init_return();
  return global.width;
}

int tb_height(void) {
  if_not_init_return();
  return global.height;
}

tb_err tb_clear(void) {
  if_not_init_return();
  return cellbuf_clear(&global.back);
}

tb_err tb_set_clear_attrs(uintattr_t fg, uintattr_t bg) {
  if_not_init_return();
  global.fg = fg;
  global.bg = bg;
  return TB_OK;
}

tb_err tb_present(void) {
  if_not_init_return();

  int rv;

  // TODO Assert global.back.(width,height) == global.front.(width,height)

  global.last_x = -1;
  global.last_y = -1;

  int x, y;
  for (y = 0; y < global.front.height; y++) {
    for (x = 0; x < global.front.width; x++) {
      struct tb_cell* back, * front;
      if_err_return(rv, cellbuf_get(&global.back, x, y, &back));
      if_err_return(rv, cellbuf_get(&global.front, x, y, &front));

      if (cell_cmp(back, front) != 0) {
        cell_copy(front, back);

        send_attr(back->fg, back->bg);
        send_char(x, y, back->ch);
      }
    }
  }

  if_err_return(rv, send_cursor_if(global.cursor_x, global.cursor_y));
  if_err_return(rv, bytebuf_flush(&global.out, global.wfd));

  return TB_OK;
}

tb_err tb_invalidate(void) {
  int rv;
  if_not_init_return();
  if_err_return(rv, resize_cellbufs());
  return TB_OK;
}

tb_err tb_set_cursor(int cx, int cy) {
  if_not_init_return();
  int rv;
  if (cx < 0) cx = 0;
  if (cy < 0) cy = 0;
  if (global.cursor_x == -1) {
    if_err_return(rv,
                  bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]));
  }
  if_err_return(rv, send_cursor_if(cx, cy));
  global.cursor_x = cx;
  global.cursor_y = cy;
  return TB_OK;
}

tb_err tb_hide_cursor(void) {
  if_not_init_return();
  int rv;
  if (global.cursor_x >= 0) {
    if_err_return(rv,
                  bytebuf_puts(&global.out, global.caps[TB_CAP_HIDE_CURSOR]));
  }
  global.cursor_x = -1;
  global.cursor_y = -1;
  return TB_OK;
}

tb_err tb_set_cell(int x, int y, uint32_t ch, uintattr_t fg, uintattr_t bg) {
  if_not_init_return();
  int rv;
  struct tb_cell* cell;
  if_err_return(rv, cellbuf_get(&global.back, x, y, &cell));
  if_err_return(rv, cell_set(cell, &ch, fg, bg));
  return TB_OK;
}

int tb_set_input_mode(int mode) {
  if_not_init_return();
#if defined(__unix__) || defined(__APPLE__)
  if (mode == TB_INPUT_CURRENT) {
    return global.input_mode;
  }

  if ((mode & (TB_INPUT_ESC | TB_INPUT_ALT)) == 0) {
    mode |= TB_INPUT_ESC;
  }

  if ((mode & (TB_INPUT_ESC | TB_INPUT_ALT)) == (TB_INPUT_ESC | TB_INPUT_ALT))
  {
    mode &= ~TB_INPUT_ALT;
  }

  if (mode & TB_INPUT_MOUSE) {
    bytebuf_puts(&global.out, TB_HARDCAP_ENTER_MOUSE);
    bytebuf_flush(&global.out, global.wfd);
  } else {
    bytebuf_puts(&global.out, TB_HARDCAP_EXIT_MOUSE);
    bytebuf_flush(&global.out, global.wfd);
  }

  global.input_mode = mode;

  return global.input_mode;
#endif
  return 0; // FIXME(grep). support set input/output modes for windows.
}

tb_err tb_set_output_mode(int mode) {
  if_not_init_return();
  switch (mode) {
    case TB_OUTPUT_CURRENT:
    return global.output_mode;
    case TB_OUTPUT_NORMAL:
    case TB_OUTPUT_256:
    case TB_OUTPUT_216:
    case TB_OUTPUT_GRAYSCALE:
#if TB_OPT_ATTR_W >= 32
    case TB_OUTPUT_TRUECOLOR:
#endif
    global.last_fg = ~global.fg;
    global.last_bg = ~global.bg;
    global.output_mode = mode;
    return TB_OK;
  }
  return TB_ERR;
}

tb_err tb_peek_event(struct tb_event* event, int timeout_ms) {
  if_not_init_return();
#if defined(__unix__) || defined(__APPLE__)
  return wait_event(event, timeout_ms);

#elif defined(_WIN32)
  memset(event, 0, sizeof(*event));
  DWORD count;
  if (!GetNumberOfConsoleInputEvents(global.h_stdin, &count)) {
    return TB_ERR_WIN32;
  }
  if (count == 0) return TB_ERR_NO_EVENT;
  return wait_event(event, -1);
#endif
}

tb_err tb_poll_event(struct tb_event* event) {
  if_not_init_return();
  return wait_event(event, -1);
}

tb_err tb_print(int x, int y, uintattr_t fg, uintattr_t bg, const char* str) {
  return tb_print_ex(x, y, fg, bg, NULL, str);
}

tb_err tb_print_ex(int x, int y, uintattr_t fg, uintattr_t bg, size_t* out_w,
                const char* str) {
  int rv;
  uint32_t uni;
  int w, ix = x;
  if (out_w) {
    *out_w = 0;
  }
  while (*str) {
    rv = tb_utf8_char_to_unicode(&uni, str);
    if (rv < 0) {
      uni = 0xfffd; // replace invalid UTF-8 char with U+FFFD
      str += rv * -1;
    } else if (rv > 0) {
      str += rv;
    } else {
      break; // shouldn't get here
    }

    w = 1; // wcwidth((wchar_t)uni);
    if_err_return(rv, tb_set_cell(x, y, uni, fg, bg));
    x += w;

    if (out_w) {
      *out_w += w;
    }
  }
  return TB_OK;
}

tb_err tb_printf(int x, int y, uintattr_t fg, uintattr_t bg, const char* fmt,
              ...) {
  int rv;
  va_list vl;
  va_start(vl, fmt);
  rv = tb_printf_inner(x, y, fg, bg, NULL, fmt, vl);
  va_end(vl);
  return rv;
}

tb_err tb_printf_ex(int x, int y, uintattr_t fg, uintattr_t bg, size_t* out_w,
                 const char* fmt, ...) {
  int rv;
  va_list vl;
  va_start(vl, fmt);
  rv = tb_printf_inner(x, y, fg, bg, out_w, fmt, vl);
  va_end(vl);
  return rv;
}

tb_err tb_send(const char* buf, size_t nbuf) {
  return bytebuf_nputs(&global.out, buf, nbuf);
}

tb_err tb_sendf(const char* fmt, ...) {
  int rv;
  char buf[TB_OPT_PRINTF_BUF];
  va_list vl;
  va_start(vl, fmt);
  rv = vsnprintf(buf, sizeof(buf), fmt, vl);
  va_end(vl);
  if (rv < 0 || rv >= (int)sizeof(buf)) {
    return TB_ERR;
  }
  return tb_send(buf, (size_t)rv);
}

struct tb_cell* tb_cell_buffer(void) {
  if (!global.initialized) return NULL;
  return global.back.cells;
}

int tb_utf8_char_length(char c) {
  return utf8_length[(unsigned char)c];
}

int tb_utf8_char_to_unicode(uint32_t* out, const char* c) {
  if (*c == '\0') return 0;

  int i;
  unsigned char len = tb_utf8_char_length(*c);
  unsigned char mask = utf8_mask[len - 1];
  uint32_t result = c[0] & mask;
  for (i = 1; i < len && c[i] != '\0'; ++i) {
    result <<= 6;
    result |= c[i] & 0x3f;
  }

  if (i != len) return i * -1;

  *out = result;
  return (int)len;
}

int tb_utf8_unicode_to_char(char* out, uint32_t c) {
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
  out[len] = '\0';

  return len;
}

int tb_last_errno(void) {
  return global.last_errno;
}

const char* tb_strerror(int err) {
  switch (err) {
    case TB_OK:
    return "Success";
    case TB_ERR_NEED_MORE:
    return "Not enough input";
    case TB_ERR_INIT_ALREADY:
    return "Termbox initialized already";
    case TB_ERR_MEM:
    return "Out of memory";
    case TB_ERR_NO_EVENT:
    return "No event";
    case TB_ERR_NO_TERM:
    return "No TERM in environment";
    case TB_ERR_NOT_INIT:
    return "Termbox not initialized";
    case TB_ERR_OUT_OF_BOUNDS:
    return "Out of bounds";
    case TB_ERR_UNSUPPORTED_TERM:
    return "Unsupported terminal";
    case TB_ERR_CAP_COLLISION:
    return "Termcaps collision";
    case TB_ERR_RESIZE_SSCANF:
    return "Terminal width/height not received by sscanf() after "
      "resize";
    case TB_ERR:
    case TB_ERR_INIT_OPEN:
    case TB_ERR_READ:
    case TB_ERR_RESIZE_IOCTL:
    case TB_ERR_RESIZE_PIPE:
    case TB_ERR_RESIZE_SIGACTION:
    case TB_ERR_POLL:
    case TB_ERR_TCGETATTR:
    case TB_ERR_TCSETATTR:
    case TB_ERR_RESIZE_WRITE:
    case TB_ERR_RESIZE_POLL:
    case TB_ERR_RESIZE_READ:
    default:
#if defined(__unix__) || defined(__APPLE__)
    strerror_r(global.last_errno, global.errbuf, sizeof(global.errbuf));
#endif
    return (const char*)global.errbuf;
  }
}

int tb_has_truecolor(void) {
#if TB_OPT_ATTR_W >= 32
  return 1;
#else
  return 0;
#endif
}

int tb_attr_width(void) {
  return TB_OPT_ATTR_W;
}

const char* tb_version(void) {
  return TB_VERSION_STR;
}

static tb_err tb_reset(void) {

#if defined(__unix__) || defined(__APPLE__)
  int ttyfd_open = global.ttyfd_open;
#endif

  memset(&global, 0, sizeof(global));
  global.rfd = -1;
  global.wfd = -1;

#if defined(__unix__) || defined(__APPLE__)
  global.ttyfd_open = ttyfd_open;
  global.ttyfd = -1;
  global.resize_pipefd[0] = -1;
  global.resize_pipefd[1] = -1;
#endif
  global.width = -1;
  global.height = -1;
  global.cursor_x = -1;
  global.cursor_y = -1;
  global.last_x = -1;
  global.last_y = -1;
  global.fg = TB_DEFAULT;
  global.bg = TB_DEFAULT;
  global.last_fg = ~global.fg;
  global.last_bg = ~global.bg;
#if defined(__unix__) || defined(__APPLE__)
  global.input_mode = TB_INPUT_ESC;
#endif
  global.output_mode = TB_OUTPUT_NORMAL;
  return TB_OK;
}


tb_err tb_printf_inner(int x, int y, uintattr_t fg, uintattr_t bg, size_t* out_w,
                    const char* fmt, va_list vl) {
  int rv;
  char buf[TB_OPT_PRINTF_BUF];
  rv = vsnprintf(buf, sizeof(buf), fmt, vl);
  if (rv < 0 || rv >= (int)sizeof(buf)) {
    return TB_ERR;
  }
  return tb_print_ex(x, y, fg, bg, out_w, buf);
}


static tb_err send_clear(void) {
  int rv;

  if_err_return(rv, send_attr(global.fg, global.bg));
  if_err_return(rv,
                bytebuf_puts(&global.out, global.caps[TB_CAP_CLEAR_SCREEN]));

  if_err_return(rv, send_cursor_if(global.cursor_x, global.cursor_y));
  if_err_return(rv, bytebuf_flush(&global.out, global.wfd));

  global.last_x = -1;
  global.last_y = -1;

  return TB_OK;
}


static tb_err init_cellbuf(void) {
  int rv;
  if_err_return(rv, cellbuf_init(&global.back, global.width, global.height));
  if_err_return(rv, cellbuf_init(&global.front, global.width, global.height));
  if_err_return(rv, cellbuf_clear(&global.back));
  if_err_return(rv, cellbuf_clear(&global.front));
  return TB_OK;
}

static tb_err tb_deinit(void) {

  if (global.caps[0] != NULL && global.wfd >= 0) {
    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
    bytebuf_puts(&global.out, global.caps[TB_CAP_SGR0]);
    bytebuf_puts(&global.out, global.caps[TB_CAP_CLEAR_SCREEN]);
#if defined(__unix__) || defined(__APPLE__)
    bytebuf_puts(&global.out, global.caps[TB_CAP_EXIT_CA]);
    bytebuf_puts(&global.out, global.caps[TB_CAP_EXIT_KEYPAD]);
    bytebuf_puts(&global.out, TB_HARDCAP_EXIT_MOUSE);
#endif
    bytebuf_flush(&global.out, global.wfd);
  }

#if defined(__unix__) || defined(__APPLE__)
  if (global.ttyfd >= 0) {
    if (global.has_orig_tios) {
      tcsetattr(global.ttyfd, TCSAFLUSH, &global.orig_tios);
    }
    if (global.ttyfd_open) {
      close(global.ttyfd);
      global.ttyfd_open = 0;
    }
  }

  sigaction(SIGWINCH, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
  if (global.resize_pipefd[0] >= 0) close(global.resize_pipefd[0]);
  if (global.resize_pipefd[1] >= 0) close(global.resize_pipefd[1]);

  if (global.terminfo) tb_free(global.terminfo);
  cap_trie_deinit(&global.cap_trie);

#elif defined(_WIN32)
  SetConsoleMode(global.h_stdout, global.mode_out);
  SetConsoleMode(global.h_stdin, global.mode_in);
#endif

  cellbuf_free(&global.back);
  cellbuf_free(&global.front);
#if defined(__unix__) || defined(__APPLE__)
  bytebuf_free(&global.in);
#endif
  bytebuf_free(&global.out);

  tb_reset();
  return TB_OK;
}


static tb_err resize_cellbufs(void) {
  int rv;
  if_err_return(rv,
                cellbuf_resize(&global.back, global.width, global.height));
  if_err_return(rv,
                cellbuf_resize(&global.front, global.width, global.height));
  if_err_return(rv, cellbuf_clear(&global.front));
  if_err_return(rv, send_clear());
  return TB_OK;
}


static tb_err send_attr(uintattr_t fg, uintattr_t bg) {
  int rv;

  if (fg == global.last_fg && bg == global.last_bg) {
    return TB_OK;
  }

  if_err_return(rv, bytebuf_puts(&global.out, global.caps[TB_CAP_SGR0]));

  uint32_t cfg, cbg;
  switch (global.output_mode) {
    default:
    case TB_OUTPUT_NORMAL:
    // The minus 1 below is because our colors are 1-indexed starting
    // from black. Black is represented by a 30, 40, 90, or 100 for fg,
    // bg, bright fg, or bright bg respectively. Red is 31, 41, 91,
    // 101, etc.
    cfg = (fg & TB_BRIGHT ? 90 : 30) + (fg & 0x0f) - 1;
    cbg = (bg & TB_BRIGHT ? 100 : 40) + (bg & 0x0f) - 1;
    break;

    case TB_OUTPUT_256:
    cfg = fg & 0xff;
    cbg = bg & 0xff;
    if (fg & TB_HI_BLACK) cfg = 0;
    if (bg & TB_HI_BLACK) cbg = 0;
    break;

    case TB_OUTPUT_216:
    cfg = fg & 0xff;
    cbg = bg & 0xff;
    if (cfg > 216) cfg = 216;
    if (cbg > 216) cbg = 216;
    cfg += 0x0f;
    cbg += 0x0f;
    break;

    case TB_OUTPUT_GRAYSCALE:
    cfg = fg & 0xff;
    cbg = bg & 0xff;
    if (cfg > 24) cfg = 24;
    if (cbg > 24) cbg = 24;
    cfg += 0xe7;
    cbg += 0xe7;
    break;

#if TB_OPT_ATTR_W >= 32
    case TB_OUTPUT_TRUECOLOR:
    cfg = fg & 0xffffff;
    cbg = bg & 0xffffff;
    if (fg & TB_HI_BLACK) cfg = 0;
    if (bg & TB_HI_BLACK) cbg = 0;
    break;
#endif
  }

  if (fg & TB_BOLD)
    if_err_return(rv, bytebuf_puts(&global.out, global.caps[TB_CAP_BOLD]));

  if (fg & TB_BLINK)
    if_err_return(rv, bytebuf_puts(&global.out, global.caps[TB_CAP_BLINK]));

  if (fg & TB_UNDERLINE)
    if_err_return(rv,
                  bytebuf_puts(&global.out, global.caps[TB_CAP_UNDERLINE]));

  if (fg & TB_ITALIC)
    if_err_return(rv,
                  bytebuf_puts(&global.out, global.caps[TB_CAP_ITALIC]));

  if (fg & TB_DIM)
    if_err_return(rv, bytebuf_puts(&global.out, global.caps[TB_CAP_DIM]));

#if TB_OPT_ATTR_W == 64
  if (fg & TB_STRIKEOUT)
    if_err_return(rv, bytebuf_puts(&global.out, TB_HARDCAP_STRIKEOUT));

  if (fg & TB_UNDERLINE_2)
    if_err_return(rv, bytebuf_puts(&global.out, TB_HARDCAP_UNDERLINE_2));

  if (fg & TB_OVERLINE)
    if_err_return(rv, bytebuf_puts(&global.out, TB_HARDCAP_OVERLINE));

  if (fg & TB_INVISIBLE)
    if_err_return(rv,
                  bytebuf_puts(&global.out, global.caps[TB_CAP_INVISIBLE]));
#endif

  if ((fg & TB_REVERSE) || (bg & TB_REVERSE))
    if_err_return(rv,
                  bytebuf_puts(&global.out, global.caps[TB_CAP_REVERSE]));

  int fg_is_default = (fg & 0xff) == 0;
  int bg_is_default = (bg & 0xff) == 0;
  if (global.output_mode == TB_OUTPUT_256) {
    if (fg & TB_HI_BLACK) fg_is_default = 0;
    if (bg & TB_HI_BLACK) bg_is_default = 0;
  }
#if TB_OPT_ATTR_W >= 32
  if (global.output_mode == TB_OUTPUT_TRUECOLOR) {
    fg_is_default = ((fg & 0xffffff) == 0) && ((fg & TB_HI_BLACK) == 0);
    bg_is_default = ((bg & 0xffffff) == 0) && ((bg & TB_HI_BLACK) == 0);
  }
#endif

  if_err_return(rv, send_sgr(cfg, cbg, fg_is_default, bg_is_default));

  global.last_fg = fg;
  global.last_bg = bg;

  return TB_OK;
}

static tb_err send_sgr(uint32_t cfg, uint32_t cbg, int fg_is_default,
                    int bg_is_default) {
  int rv;
  char nbuf[32];

  if (fg_is_default && bg_is_default) {
    return TB_OK;
  }

  switch (global.output_mode) {
    default:
    case TB_OUTPUT_NORMAL:
    send_literal(rv, "\x1b[");
    if (!fg_is_default) {
      send_num(rv, nbuf, cfg);
      if (!bg_is_default) {
        send_literal(rv, ";");
      }
    }
    if (!bg_is_default) {
      send_num(rv, nbuf, cbg);
    }
    send_literal(rv, "m");
    break;

    case TB_OUTPUT_256:
    case TB_OUTPUT_216:
    case TB_OUTPUT_GRAYSCALE:
    send_literal(rv, "\x1b[");
    if (!fg_is_default) {
      send_literal(rv, "38;5;");
      send_num(rv, nbuf, cfg);
      if (!bg_is_default) {
        send_literal(rv, ";");
      }
    }
    if (!bg_is_default) {
      send_literal(rv, "48;5;");
      send_num(rv, nbuf, cbg);
    }
    send_literal(rv, "m");
    break;

#if TB_OPT_ATTR_W >= 32
    case TB_OUTPUT_TRUECOLOR:
    send_literal(rv, "\x1b[");
    if (!fg_is_default) {
      send_literal(rv, "38;2;");
      send_num(rv, nbuf, (cfg >> 16) & 0xff);
      send_literal(rv, ";");
      send_num(rv, nbuf, (cfg >> 8) & 0xff);
      send_literal(rv, ";");
      send_num(rv, nbuf, cfg & 0xff);
      if (!bg_is_default) {
        send_literal(rv, ";");
      }
    }
    if (!bg_is_default) {
      send_literal(rv, "48;2;");
      send_num(rv, nbuf, (cbg >> 16) & 0xff);
      send_literal(rv, ";");
      send_num(rv, nbuf, (cbg >> 8) & 0xff);
      send_literal(rv, ";");
      send_num(rv, nbuf, cbg & 0xff);
    }
    send_literal(rv, "m");
    break;
#endif
  }
  return TB_OK;
}

static tb_err send_cursor_if(int x, int y) {
  int rv;
  char nbuf[32];
  if (x < 0 || y < 0) {
    return TB_OK;
  }
  send_literal(rv, "\x1b[");
  send_num(rv, nbuf, y + 1);
  send_literal(rv, ";");
  send_num(rv, nbuf, x + 1);
  send_literal(rv, "H");
  return TB_OK;
}

static tb_err send_char(int x, int y, uint32_t ch) {
  return send_cluster(x, y, &ch, 1);
}

static tb_err send_cluster(int x, int y, uint32_t* ch, size_t nch) {
  int rv;
  char chu8[8];

  if (global.last_x != x - 1 || global.last_y != y) {
    if_err_return(rv, send_cursor_if(x, y));
  }
  global.last_x = x;
  global.last_y = y;

  int i;
  for (i = 0; i < (int)nch; i++) {
    uint32_t ch32 = *(ch + i);
    int chu8_len;
    if (ch32 == 0) { // replace null with space (from termbox 19dbee5)
      chu8_len = 1;
      chu8[0] = ' ';
    } else {
      chu8_len = tb_utf8_unicode_to_char(chu8, ch32);
    }
    if_err_return(rv, bytebuf_nputs(&global.out, chu8, (size_t)chu8_len));
  }

  return TB_OK;
}

static tb_err convert_num(uint32_t num, char* buf) {
  int i, l = 0;
  char ch;
  do {
    /* '0' = 48; 48 + num%10 < 58 < MAX_8bitCHAR */
    buf[l++] = (char)('0' + (num % 10));
    num /= 10;
  } while (num);
  for (i = 0; i < l / 2; i++) {
    ch = buf[i];
    buf[i] = buf[l - 1 - i];
    buf[l - 1 - i] = ch;
  }
  return l;
}

static tb_err cell_cmp(struct tb_cell* a, struct tb_cell* b) {
  if (a->ch != b->ch || a->fg != b->fg || a->bg != b->bg) {
    return 1;
  }
  return 0;
}

static tb_err cell_copy(struct tb_cell* dst, struct tb_cell* src) {
  return cell_set(dst, &src->ch, src->fg, src->bg);
}

static tb_err cell_set(struct tb_cell* cell, uint32_t* ch,
                    uintattr_t fg, uintattr_t bg) {
  cell->ch = ch ? *ch : 0;
  cell->fg = fg;
  cell->bg = bg;
  return TB_OK;
}

static tb_err cell_free(struct tb_cell* cell) {
  memset(cell, 0, sizeof(*cell));
  return TB_OK;
}

static tb_err cellbuf_init(struct cellbuf_t* c, int w, int h) {
  c->cells = tb_malloc(sizeof(struct tb_cell) * w * h);
  if (!c->cells) {
    return TB_ERR_MEM;
  }
  memset(c->cells, 0, sizeof(struct tb_cell) * w * h);
  c->width = w;
  c->height = h;
  return TB_OK;
}

static tb_err cellbuf_free(struct cellbuf_t* c) {
  if (c->cells) {
    int i;
    for (i = 0; i < c->width * c->height; i++) {
      cell_free(&c->cells[i]);
    }
    tb_free(c->cells);
  }
  memset(c, 0, sizeof(*c));
  return TB_OK;
}

static tb_err cellbuf_clear(struct cellbuf_t* c) {
  int rv, i;
  uint32_t space = (uint32_t)' ';
  for (i = 0; i < c->width * c->height; i++) {
    if_err_return(rv,
                  cell_set(&c->cells[i], &space, global.fg, global.bg));
  }
  return TB_OK;
}

static tb_err cellbuf_get(struct cellbuf_t* c, int x, int y,
                       struct tb_cell** out) {
  if (x < 0 || x >= c->width || y < 0 || y >= c->height) {
    *out = NULL;
    return TB_ERR_OUT_OF_BOUNDS;
  }
  *out = &c->cells[(y * c->width) + x];
  return TB_OK;
}

static tb_err cellbuf_resize(struct cellbuf_t* c, int w, int h) {
  int rv;

  int ow = c->width;
  int oh = c->height;

  if (ow == w && oh == h) {
    return TB_OK;
  }

  w = w < 1 ? 1 : w;
  h = h < 1 ? 1 : h;

  int minw = (w < ow) ? w : ow;
  int minh = (h < oh) ? h : oh;

  struct tb_cell* prev = c->cells;

  if_err_return(rv, cellbuf_init(c, w, h));
  if_err_return(rv, cellbuf_clear(c));

  int x, y;
  for (x = 0; x < minw; x++) {
    for (y = 0; y < minh; y++) {
      struct tb_cell* src, * dst;
      src = &prev[(y * ow) + x];
      if_err_return(rv, cellbuf_get(c, x, y, &dst));
      if_err_return(rv, cell_copy(dst, src));
    }
  }

  tb_free(prev);

  return TB_OK;
}

static tb_err bytebuf_puts(struct bytebuf_t* b, const char* str) {
  if (!str || strlen(str) <= 0) return TB_OK; // Nothing to do for empty caps
  return bytebuf_nputs(b, str, (size_t)strlen(str));
}

static tb_err bytebuf_nputs(struct bytebuf_t* b, const char* str, size_t nstr) {
  int rv;
  if_err_return(rv, bytebuf_reserve(b, b->len + nstr + 1));
  memcpy(b->buf + b->len, str, nstr);
  b->len += nstr;
  b->buf[b->len] = '\0';
  return TB_OK;
}

static tb_err bytebuf_shift(struct bytebuf_t* b, size_t n) {
  if (n > b->len) {
    n = b->len;
  }
  size_t nmove = b->len - n;
  memmove(b->buf, b->buf + n, nmove);
  b->len -= n;
  return TB_OK;
}

static tb_err bytebuf_flush(struct bytebuf_t* b, int fd) {
  if (b->len <= 0) {
    return TB_OK;
  }
  ssize_t write_rv = write(fd, b->buf, b->len);
  if (write_rv < 0 || (size_t)write_rv != b->len) {
    // Note, errno will be 0 on partial write
    global.last_errno = errno;
    return TB_ERR;
  }
  b->len = 0;
  return TB_OK;
}

static tb_err bytebuf_reserve(struct bytebuf_t* b, size_t sz) {
  if (b->cap >= sz) {
    return TB_OK;
  }
  size_t newcap = b->cap > 0 ? b->cap : 1;
  while (newcap < sz) {
    newcap *= 2;
  }
  char* newbuf;
  if (b->buf) {
    newbuf = tb_realloc(b->buf, newcap);
  } else {
    newbuf = tb_malloc(newcap);
  }
  if (!newbuf) {
    return TB_ERR_MEM;
  }
  b->buf = newbuf;
  b->cap = newcap;
  return TB_OK;
}

static tb_err bytebuf_free(struct bytebuf_t* b) {
  if (b->buf) {
    tb_free(b->buf);
  }
  memset(b, 0, sizeof(*b));
  return TB_OK;
}

