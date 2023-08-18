// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <raylib.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

#include "common.h"
#include "buffer.h"


class Theme;
typedef const Theme* const* ThemePtr;


class ThemeListener {
public:
	virtual void ThemeChanged(const Theme* theme) = 0;
};



struct ThemeEntry {
	Color fg;
	Color bg;
	uint8_t modifier;
};


struct UiThemeCache {
#define _ BLANK
	ThemeEntry background     = { _,        WHITE,   0 };
	ThemeEntry text           = { BLACK,    _,       0 };
	ThemeEntry cursor_primary = { GRAY,     _,       0 };
	ThemeEntry selection      = { _,        BLUE,    0 };
	ThemeEntry linenr         = { GRAY,     _,       0 };
	ThemeEntry statusline     = { WHITE,    BLUE,    0 };
#undef _
};


class Theme {

	// Only create this object as a unique pointer.
public: static std::unique_ptr<Theme> New(const std::string& name) { return std::unique_ptr<Theme>(new Theme(name)); }
private: Theme(const std::string& name) : name(name) {}

public:
	enum Modifier {
		NONE      = 0,
		BOLD      = 1 << 1,
		ITALIC    = 1 << 2,
		UNDERLINE = 1 << 3,
		CROSSED   = 1 << 4,
		DIM       = 1 << 5,
		REVERSED  = 1 << 6,
	};

	const ThemeEntry* GetEntry(std::string name) const;
	const std::string& GetName() const { return name; }

	const UiThemeCache& GetUiEntries() const { return ui; }

	void LoadFromMemory(const char* toml_source);
	void LoadFromFile(const char* toml_file_path) { TODO; }

	// Convert a string like "#58a6ff" to a color, returns true
	// if success and update the color value.
	static bool StringToColor(const char* str, Color* color);

private:
	void _CacheUiEntries();
	bool _GetColorFromString(const std::string& name, Color* color) const;

	std::string name;
	std::unordered_map<std::string, Color> palette_table;
	std::unordered_map<std::string, ThemeEntry> entries;

	UiThemeCache ui; // "cached" entries.
};


class BufferThemeCache : public BufferListener {

	// Only create this object as a unique pointer.
public: static std::unique_ptr<BufferThemeCache> New(ThemePtr ptr) {
	return std::unique_ptr<BufferThemeCache>(new BufferThemeCache(ptr));
}
private: BufferThemeCache(ThemePtr ptr) : theme_ptr(ptr) {}

public:
	// The of a single character, we'll cache this value in a buffer and re-use this
	// instead of calculating the value over and over 60 frames per second. Note that
	// the size of this struct is smaller than a 64 bit integer.
	struct Themelet {
		Color color;
		uint8_t modifiers;
	};

	void OnBufferChanged(Buffer* buffer) override;
	void CacheThemelets(Buffer* buffer);

	const std::vector<Themelet>& GetThemeles() const { return themelets; }

private:

	// We use a pointer to theme pointer because if the theme ever changed, 
	ThemePtr theme_ptr = nullptr;

	std::vector<Themelet> themelets;

};


class ThemeManager {
public:

	// We have this function because if the theme ever updated, those who uses
	// this pointer to pointer to theme will automatically get free update of
	// the new theme, No need callback bullshit.
	ThemePtr GetThemePtr() const { return &theme; }
	const Theme* GetCurrentTheme() const { return theme; }

	const Theme* GetTheme(const std::string& name) const;

	// If update is true, it'll set it as the current theme.
	void AddTheme(std::unique_ptr<Theme> theme, bool update = false);
	void SetTheme(const std::string& name);

	void RegisterListener(ThemeListener* listener);

private:
	void _OnThemeChanged();

	const Theme* theme = nullptr;
	std::unordered_map<std::string, std::unique_ptr<Theme>> themes;
	std::vector<ThemeListener*> listeners;
};

