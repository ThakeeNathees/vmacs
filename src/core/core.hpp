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

#include <thread>
#include <atomic>
#include <future>

// Commonly used macros and utility C things.
#include "common.h"


// The key event is serialized into a single integer to be used in map keys and
// compact form where the ctrl/alt/shift are set as bits in the integer.
typedef uint32_t event_t;

// Not sure this is a good idea but, if this set to true, we signal all the
// threads to stop what they're doing and join.
extern std::atomic<bool> global_thread_stop;

/*
 * TODO:
 * This is incomplete as there are resize events and mouse events. Also I
 * need to re-consider that in key events, only unicode or keycode exists at a
 * time so we don't need more space for storing both at the same time.
 *
 * Largest key code we use is 336 whish will fit into 9 bits so we're using 10
 * bits for the keycode here.
 *
 *   0000 0000 0000 0000   0000 0000 0000 0000
 *             ^       ^      ^ ^^^-- keycode  mask = 0x3ff, shift = 0
 *             '-------'      | |`---- ctrl    mask = 0x400
 *                 |          | '----- alt     mask = 0x800
 *                 |          '------- shift   mask = 0x1000
 *                 '------------------ ascii   mask = 0xff0000, shift = 16
 */
#define KEY_CODE(code, ascii, ctrl, alt, shift) \
  Event::Keycode::code | \
  (ctrl?0x400:0) | (alt?0x800:0) | (shift?0x1000:0) | ((ascii & 0xff) << 13)


/*
 * The string type used by the buffer type defined in case if we want to change
 * it in the future and implement our own string system which support first
 * class utf8 handling.
 */
typedef std::string String;
typedef std::string_view StringView;


typedef struct {
  int row;
  int col;
} Coord;


typedef struct {
  int start;
  int end;
} Slice;


typedef struct {
  int width;
  int height;
} Size;


// The tree acts like a Trie for Events to handle sequence events. contains a
// cursor to keep track of where we're at the key combination.
class KeyTree {

  // A Node in the KeyTree (trie). Contains a map of child nodes and a map of
  // actions mapped from mode. The action for a specific key combination can
  // change depend on the mode it's in.
  struct KeyTreeNode {
    std::map<event_t, std::unique_ptr<KeyTreeNode>> children;
    std::map<std::string, FuncAction> actions;
  };


  // KeyTreeCursor will keep track of where we're in while processing a key
  // combination.
  struct KeyTreeCursor {
    KeyTreeNode* node; // Node we're currenly in initially the root.
    std::vector<event_t> recorded_events; // Previous events which leads here.
  };

public:
  KeyTree();

  // It'll change the mode and clear the cursor back to root.
  void SetMode(const std::string& mode);

  // Clear all the tracks of cursor and set back to the root node.
  void ResetCursor();

  // Returns true if the cursor is at the root node and not listening to any key
  // sequences at the moment.
  bool IsCursorRoot() const;

  void RegisterBinding(std::string, const std::vector<event_t>& events, FuncAction action);

  // When an event is encountered we feed that event to the keytree here to
  // update it's internal cursor, this will return the action for this event
  // if exists otherwise nullptr.
  //
  // It'll set the boolean more true if there are more sequence of keys down the
  // tree starting with the current sequence. The calling function can decide
  // what to do about the returned action and what to do with the reset of the
  // sequence (e.g. use a timeout).
  FuncAction ConsumeEvent(event_t event, bool* more);

private:
  std::string mode; // Current mode we're in.
  std::unique_ptr<KeyTreeNode> root;
  KeyTreeCursor cursor;
};


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


// ----------------------------------------------------------------------------
// Util function definitions.
// ----------------------------------------------------------------------------


// TODO: Define a Config class.
#define TABSIZE 4


unsigned constexpr ConstHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) + 33 * ConstHash(input + 1) : 5381;
}


std::size_t constexpr operator "" _hash(const char* s, size_t) {
  return ConstHash(s);
}


int GetElapsedTime();
bool EndsWith(StringView str, StringView suffix);
bool StartsWith(StringView str, StringView suffix);

uint8_t RgbToXterm(uint32_t rgb);
uint32_t XtermToRgb(uint8_t xterm);

event_t EncodeKeyEvent(Event::Key key);
Event::Key DecodeKeyEvent(event_t key);

/*
 * On a successfull parse, it'll return true and push all the keys are parsed
 * from that string.
 *
 * - Note that it'll not clear the events vector, but just push along with
 *   whatever is already in the vector, so make sure it's empty before calling.
 *
 * - Note that an empty string is valid here and result empty events.
 *
 */
bool ParseKeyBindingString(std::vector<event_t>& events, const char* binding);

