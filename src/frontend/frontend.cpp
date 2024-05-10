//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "frontend.hpp"


bool Termbox2::Initialize() {
	int code = tb_init();
	if (code < 0) {
		fprintf(stderr, "termbox init failed, code: %d\n", code);
    return false;
	}

  // DEFINE TB_OPT_ATTR_W=32, to termbox for true color.
	tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
  tb_set_output_mode(TB_OUTPUT_TRUECOLOR);
  return true;
}


bool Termbox2::Cleanup() {
  return tb_shutdown() == TB_OK;
}


FrameBuffer& Termbox2::GetDrawBuffer() {

  int width = tb_width();
  int height = tb_height();

  if (buff.cells.size() != width * height) {
    buff.cells.resize(width*height);
  }

  buff.width  = width;
  buff.height = height;

  return buff;
}


void Termbox2::Display() {

  int width = buff.width;
  int height = buff.height;

  tb_clear();

  // This should be optimized with memmove of the buffer with the termbox
  // buffer and they provide an api to do so.
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      Cell& cell = buff.cells[y*width + x];

      // FIXME(mess): Since termbox treat 0 as the default terminal color, we
      // cannot use black with 0, so I'm using 0x000001 as black (truecolor).
      // Find a better way.
      if (cell.fg == 0) cell.fg = 0x000001;

      int attrib = 0;
      attrib |= (cell.attrib & VMACS_CELL_BOLD)      ? TB_BOLD : 0;
      attrib |= (cell.attrib & VMACS_CELL_UNDERLINE) ? TB_UNDERLINE : 0;
      attrib |= (cell.attrib & VMACS_CELL_ITALIC)    ? TB_ITALIC : 0;
      attrib |= (cell.attrib & VMACS_CELL_REVERSE)   ? TB_REVERSE : 0;
      tb_set_cell(x, y, cell.ch, cell.fg | attrib, cell.bg);
    }
  }

  tb_present();
}


std::vector<Event> Termbox2::GetEvents() {

  std::vector<Event> events;

  do {
    struct tb_event ev;
    if (tb_poll_event(&ev) != TB_OK) return {};

    switch (ev.type) {
    case TB_EVENT_KEY: {

      Event e(Event::Type::KEY);
      e.key.unicode = ev.ch;

      // TODO: Note that we're setting key.ctrl to false bellow when it's key
      // enter or backspace etc, since the terminal "sees" ctrl+m / Enter as the
      // same like wise the other combinations. figure out a different way to handle this.

      e.key.alt     = (ev.mod & TB_MOD_ALT) != 0;
      e.key.ctrl    = (ev.mod & TB_MOD_CTRL) != 0;
      e.key.shift   = (ev.mod & TB_MOD_SHIFT) != 0;

      switch (ev.key) {
        case 0                       : break;
        case TB_KEY_F1               : e.key.code = Event::KEY_F1; break;
        case TB_KEY_F2               : e.key.code = Event::KEY_F2; break;
        case TB_KEY_F3               : e.key.code = Event::KEY_F3; break;
        case TB_KEY_F4               : e.key.code = Event::KEY_F4; break;
        case TB_KEY_F5               : e.key.code = Event::KEY_F5; break;
        case TB_KEY_F6               : e.key.code = Event::KEY_F6; break;
        case TB_KEY_F7               : e.key.code = Event::KEY_F7; break;
        case TB_KEY_F8               : e.key.code = Event::KEY_F8; break;
        case TB_KEY_F9               : e.key.code = Event::KEY_F9; break;
        case TB_KEY_F10              : e.key.code = Event::KEY_F10; break;
        case TB_KEY_F11              : e.key.code = Event::KEY_F11; break;
        case TB_KEY_F12              : e.key.code = Event::KEY_F12; break;
        case TB_KEY_INSERT           : e.key.code = Event::KEY_INSERT; break;
        case TB_KEY_DELETE           : e.key.code = Event::KEY_DELETE; break;
        case TB_KEY_HOME             : e.key.code = Event::KEY_HOME; break;
        case TB_KEY_END              : e.key.code = Event::KEY_END; break;
        case TB_KEY_PGUP             : e.key.code = Event::KEY_PAGE_UP; break;
        case TB_KEY_PGDN             : e.key.code = Event::KEY_PAGE_DOWN; break;
        case TB_KEY_ARROW_UP         : e.key.code = Event::KEY_UP; break;
        case TB_KEY_ARROW_DOWN       : e.key.code = Event::KEY_DOWN; break;
        case TB_KEY_ARROW_LEFT       : e.key.code = Event::KEY_LEFT; break;
        case TB_KEY_ARROW_RIGHT      : e.key.code = Event::KEY_RIGHT; break;

        // The value for both the bello are 0, cannot bind.
        // case TB_KEY_CTRL_TILDE    :
        // case TB_KEY_CTRL_2           : break;

        case TB_KEY_CTRL_A           : e.key.code = Event::KEY_A; break;
        case TB_KEY_CTRL_B           : e.key.code = Event::KEY_B; break;
        case TB_KEY_CTRL_C           : e.key.code = Event::KEY_C; break;
        case TB_KEY_CTRL_D           : e.key.code = Event::KEY_D; break;
        case TB_KEY_CTRL_E           : e.key.code = Event::KEY_E; break;
        case TB_KEY_CTRL_F           : e.key.code = Event::KEY_F; break;
        case TB_KEY_CTRL_G           : e.key.code = Event::KEY_G; break;

        // case TB_KEY_BACKSPACE     :
        case TB_KEY_CTRL_H           : e.key.code = Event::KEY_H; break;

        // case TB_KEY_TAB           :
        case TB_KEY_CTRL_I           : e.key.ctrl = false; e.key.code = Event::KEY_TAB; break;

        case TB_KEY_CTRL_J           : e.key.code = Event::KEY_J; break;
        case TB_KEY_CTRL_K           : e.key.code = Event::KEY_K; break;
        case TB_KEY_CTRL_L           : e.key.code = Event::KEY_L; break;

        // case TB_KEY_ENTER         :
        case TB_KEY_CTRL_M           : e.key.ctrl = false; e.key.code = Event::KEY_ENTER; break;

        case TB_KEY_CTRL_N           : e.key.code = Event::KEY_N; break;
        case TB_KEY_CTRL_O           : e.key.code = Event::KEY_O; break;
        case TB_KEY_CTRL_P           : e.key.code = Event::KEY_P; break;
        case TB_KEY_CTRL_Q           : e.key.code = Event::KEY_Q; break;
        case TB_KEY_CTRL_R           : e.key.code = Event::KEY_R; break;
        case TB_KEY_CTRL_S           : e.key.code = Event::KEY_S; break;
        case TB_KEY_CTRL_T           : e.key.code = Event::KEY_T; break;
        case TB_KEY_CTRL_U           : e.key.code = Event::KEY_U; break;
        case TB_KEY_CTRL_V           : e.key.code = Event::KEY_V; break;
        case TB_KEY_CTRL_W           : e.key.code = Event::KEY_W; break;
        case TB_KEY_CTRL_X           : e.key.code = Event::KEY_X; break;
        case TB_KEY_CTRL_Y           : e.key.code = Event::KEY_Y; break;
        case TB_KEY_CTRL_Z           : e.key.code = Event::KEY_Z; break;

        // case TB_KEY_ESC           :
        // case TB_KEY_CTRL_LSQ_BRACKET :
        case TB_KEY_CTRL_3           : e.key.ctrl = false; e.key.code = Event::KEY_ESCAPE; break;

        // case TB_KEY_CTRL_4        :
        case TB_KEY_CTRL_BACKSLASH   : e.key.code = Event::KEY_BACKSLASH; break;

        // case TB_KEY_CTRL_5        :
        case TB_KEY_CTRL_RSQ_BRACKET : e.key.code = Event::KEY_RIGHT_BRACKET; break;

        case TB_KEY_CTRL_6           : e.key.code = Event::KEY_KP_6; break;

        // case TB_KEY_CTRL_7        :
        // case TB_KEY_CTRL_SLASH    :
        case TB_KEY_CTRL_UNDERSCORE  : e.key.code = Event::KEY_SLASH; break;

        case TB_KEY_SPACE            : e.key.code = Event::KEY_SPACE; break;

        // case TB_KEY_BACKSPACE2    :
        case TB_KEY_CTRL_8           : e.key.ctrl = false; e.key.code = Event::KEY_BACKSPACE; break;

      }

      // FIXME(mess): Temproary close button.
      if (e.key.code == Event::KEY_Q && e.key.ctrl) {
        events.push_back(Event(Event::Type::CLOSE));
      } else {
        events.push_back(e);
      }


    } break;

    case TB_EVENT_MOUSE: {
      Event e(Event::Type::MOUSE);
      e.mouse.x = ev.x;
      e.mouse.y = ev.y;
      switch (ev.key) {
        case TB_KEY_MOUSE_LEFT       : e.mouse.button = Event::MouseButton::MOUSE_BUTTON_LEFT; break;
        case TB_KEY_MOUSE_RIGHT      : e.mouse.button = Event::MouseButton::MOUSE_BUTTON_RIGHT; break;
        case TB_KEY_MOUSE_MIDDLE     : e.mouse.button = Event::MouseButton::MOUSE_BUTTON_MIDDLE; break;
        case TB_KEY_MOUSE_RELEASE    : e.mouse.button = Event::MouseButton::MOUSE_RELEASED; break;
        case TB_KEY_MOUSE_WHEEL_UP   : e.mouse.button = Event::MouseButton::MOUSE_WHEEL_UP; break;
        case TB_KEY_MOUSE_WHEEL_DOWN : e.mouse.button = Event::MouseButton::MOUSE_WHEEL_DOWN; break;
      }
      events.push_back(e);
    } break;

    case TB_EVENT_RESIZE: {
      Event e(Event::Type::RESIZE);
      e.resize.width  = tb_width();
      e.resize.height = tb_height();
      events.push_back(e);
    } break;

    }

  } while (events.size() == 0);

  return events;
}

