// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <raylib.h>


class Event {

public:
  struct Size {
    unsigned int width;
    unsigned int height;
  };

  struct Key {
    KeyboardKey key;
    int unicode;
    bool alt;
    bool control;
    bool shift;
  };

#if 0 // TODO: Mouse events are not considered at the moment.

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
#endif

  enum class Type {
    _NONE,
    CLOSE,
    RESIZE,
    KEY,
    WHEEL,
    MOUSE,
    _COUNT,
  };

  Type type;
  bool handled = false;

  union {
    Size resize;
    Key  key;
#if 0
    Mouse      mouse;
    MouseWheel mouse_wheel;
#endif
  };

  Event(Event::Type type) : type(type), key(Key()) {}

};
