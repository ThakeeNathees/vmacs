// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <stdio.h>
#include <stdlib.h>


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


// ----------------------------------------------------------------------------
// Basic type definition (inaddition to raylib)
// ----------------------------------------------------------------------------

#include <raylib.h>

#ifdef __cplusplus

class Vector2i {
public:
  int x;
  int y;

  Vector2i(int x, int y) : x(x), y(y) {}
  Vector2i(const Vector2 other) { x = (int)other.x; y = (int)other.y; }
  Vector2 Float() const { return { (float)x, (float)y }; }
};
#else

typedef struct Vector2i {
  int x;
  int y;
} Vector2i;

#endif


typedef struct Rectanglei {
  int x;                // Rectangle top-left corner position x
  int y;                // Rectangle top-left corner position y
  int width;            // Rectangle width
  int height;           // Rectangle height
} Rectanglei;

// Since all the basic types are defined here, I'm also defining this here,
// however I know that I'm not supposed to do this. Who cares.
typedef struct Coord {
  int row;
  int col;
} Coord;


typedef struct Slice {
  int start;
  int end;
} Slice;


typedef struct Size {
  int width;
  int height;
} Size;
