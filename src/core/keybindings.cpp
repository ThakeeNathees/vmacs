// Copyright (c) 2023 Thakee Nathees

#include "keybindings.h"


CommandFn KeyBindings::GetCommand(const std::string& key_combination) const {
  auto iter = bindings.find(key_combination);
  if (iter == bindings.end()) return nullptr;
  return iter->second;
}


const char* KeyBindings::GetKeyName(KeyboardKey key) {
  auto iter = key_names.find(key);
  if (iter == key_names.end()) return nullptr;
  return iter->second;
}


const std::unordered_map<KeyboardKey, const char*> KeyBindings::key_names = {
  { KEY_APOSTROPHE      , "'" },
  { KEY_COMMA           , "," },
  { KEY_MINUS           , "-" },
  { KEY_PERIOD          , "." },
  { KEY_SLASH           , "/" },
  { KEY_ZERO            , "0" },
  { KEY_ONE             , "1" },
  { KEY_TWO             , "2" },
  { KEY_THREE           , "3" },
  { KEY_FOUR            , "4" },
  { KEY_FIVE            , "5" },
  { KEY_SIX             , "6" },
  { KEY_SEVEN           , "7" },
  { KEY_EIGHT           , "8" },
  { KEY_NINE            , "9" },
  { KEY_SEMICOLON       , ";" },
  { KEY_EQUAL           , "=" },
  { KEY_A               , "a" },
  { KEY_B               , "b" },
  { KEY_C               , "c" },
  { KEY_D               , "d" },
  { KEY_E               , "e" },
  { KEY_F               , "f" },
  { KEY_G               , "g" },
  { KEY_H               , "h" },
  { KEY_I               , "i" },
  { KEY_J               , "j" },
  { KEY_K               , "k" },
  { KEY_L               , "l" },
  { KEY_M               , "m" },
  { KEY_N               , "n" },
  { KEY_O               , "o" },
  { KEY_P               , "p" },
  { KEY_Q               , "q" },
  { KEY_R               , "r" },
  { KEY_S               , "s" },
  { KEY_T               , "t" },
  { KEY_U               , "u" },
  { KEY_V               , "v" },
  { KEY_W               , "w" },
  { KEY_X               , "x" },
  { KEY_Y               , "y" },
  { KEY_Z               , "z" },
  { KEY_LEFT_BRACKET    , "[" },
  { KEY_BACKSLASH       , "\\" },
  { KEY_RIGHT_BRACKET   , "]" },
  { KEY_GRAVE           , "`" },
  { KEY_SPACE           , "space" },
  { KEY_ESCAPE          , "esc" },
  { KEY_ENTER           , "enter" },
  { KEY_TAB             , "tab" },
  { KEY_BACKSPACE       , "backspace" },
  { KEY_INSERT          , "ins" },
  { KEY_DELETE          , "delete" },
  { KEY_RIGHT           , "right" },
  { KEY_LEFT            , "left" },
  { KEY_DOWN            , "down" },
  { KEY_UP              , "up" },
  { KEY_PAGE_UP         , "pageup" },
  { KEY_PAGE_DOWN       , "pagedown" },
  { KEY_HOME            , "home" },
  { KEY_END             , "end" },
  { KEY_F1              , "f1" },
  { KEY_F2              , "f2" },
  { KEY_F3              , "f3" },
  { KEY_F4              , "f4" },
  { KEY_F5              , "f5" },
  { KEY_F6              , "f6" },
  { KEY_F7              , "f7" },
  { KEY_F8              , "f8" },
  { KEY_F9              , "f9" },
  { KEY_F10             , "f10" },
  { KEY_F11             , "f11" },
  { KEY_F12             , "f12" },
};
