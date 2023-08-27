//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <map>
#include <string>
#include <memory>

#include "common.hpp"
#include "event.hpp"

class Widget;


// Passed the widget and the parameters, if no parameters, it'll be  null.
typedef const std::vector<std::string>* CommandArgs;
typedef void (*CommandFn)(Widget*, CommandArgs);
typedef std::map<std::string, CommandFn> BindingData;

// TODO: Remove Keybindings from the editor after implementing this.
class KeyBindings {

public:
	static const char* GetKeyName(KeyboardKey key);

	// Only create this object as a unique pointer.
	// This will take an rvalue reference. You either have to pass a literal
	// data or do an std::move of the data.
public: static std::unique_ptr<KeyBindings> New(BindingData&& data) {
	return std::unique_ptr<KeyBindings>(new KeyBindings(std::move(data)));
}
private: KeyBindings(BindingData&& data) { bindings = std::move(data); }

public:
	CommandFn GetCommand(const std::string& key_combination) const;

private:
	// These are the only boundable keys along with ctrl, shift, alt.
	static const std::unordered_map<KeyboardKey, const char*> key_names;
	BindingData bindings;

};
