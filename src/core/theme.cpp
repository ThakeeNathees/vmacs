// Copyright (c) 2023 Thakee Nathees

#include "theme.h"
#include <tomlcpp.hpp>


void Theme::_CacheUiEntries() {

#define CACHE_VALUE(value, name)              \
	do {                                        \
		const ThemeEntry* entry = GetEntry(name); \
		if (entry != nullptr) value = *entry;     \
	} while (false);

	CACHE_VALUE(ui.background,     "ui.background");
	CACHE_VALUE(ui.text,           "ui.text");
	CACHE_VALUE(ui.cursor_primary, "ui.cursor.primary");
	CACHE_VALUE(ui.selection,      "ui.selection");
	CACHE_VALUE(ui.linenr,         "ui.linenr");
	CACHE_VALUE(ui.statusline,     "ui.statusline");

#undef CACHE_VALUE
}


const ThemeEntry* Theme::GetEntry(std::string name) const {

	const ThemeEntry* entry = nullptr;
	while (true) {
		auto iter = entries.find(name);
		if (iter != entries.end()) return &iter->second;
		size_t pos = name.find_last_of('.');
		if (pos == std::string::npos) return nullptr;
		name.erase(pos, name.size() - pos);
	}
}


bool Theme::StringToColor(const char* str, Color* color) {
	ASSERT(str != nullptr && color != nullptr, OOPS);
	if (*str != '#') return false;

	str++; // Skip the '#'.

	for (int i = 0; i < 6; i++) {
		char c = str[i];
		if (!(BETWEEN('0', c, '9') || BETWEEN('a', c, 'f') || BETWEEN('A', c, 'F'))) return false;
	}

#define HEX_TO_INT(c)         \
	BETWEEN('0', (c), '9')      \
	  ? ((c) - '0')             \
	  : (BETWEEN('a', (c), 'f') \
	      ? ((c) - 'a' + 10)    \
				: ((c) - 'A' + 10))

	uint8_t hex[6];
	for (int i = 0; i < 6; i++) hex[i] = HEX_TO_INT(str[i]);

	color->r = hex[0] << 4 | hex[1];
	color->g = hex[2] << 4 | hex[3];
	color->b = hex[4] << 4 | hex[5];
	color->a = 255;

#undef HEX_TO_INT

	return true;
}


bool Theme::_GetColorFromString(const std::string& name, Color* color) const {

	if (name.size() >= 1 && name.at(0) == '#') {
		return StringToColor(name.c_str(), color);
	}

	auto iter = palette_table.find(name);
	if (iter == palette_table.end()) return false;
	*color = iter->second;
	return true;
}


void Theme::LoadFromMemory(const char* toml_source) {
	toml::Result res = toml::parse(toml_source);
	if (!res.table) {
		// TODO: error("Error while parsing theme config: %s", res.errmsg);
		return;
	}

	auto palette = res.table->getTable("palette");
	if (!palette) {
		// TODO: error("Error while loading theme %s, doesn't have palette in the config.", name);
		return;
	}

	for (const std::string& key : palette->keys()) {
		auto [ok, value] = palette->getString(key);
		if (!ok) {
			// TODO: error("Invalid config value for palette key %s. Expected a string color", key);
			continue;
		}

		Color color;
		if (!StringToColor(value.c_str(), &color)) {
			// TODO: error("Invalid color value (%s) for palette key %s.", value, key);
			continue;
		}
		palette_table[key] = color;
	}

	for (const std::string& key : res.table->keys()) {

		// Value can be either color string or a table.

		{ // Check if string.
			auto [ok, value] = res.table->getString(key);
			if (ok) {

				Color color;
				if (!_GetColorFromString(value, &color)) {
					// TODO: error("Invalid palette key / or color value (%s) for theme %s.", value, key);
				} else {
					ThemeEntry& entry = entries[key];
					entry.fg = color;
				}

				continue;
			}
		}

		{ // Must be table then.
			auto table = res.table->getTable(key);
			if (!table) {
				// TODO: error("Invalid theme entry (%s) for theme %s.", value, key);
				continue;
			}

			ThemeEntry& entry = entries[key];

			// fg.
			{
				auto [ok, fg] = table->getString("fg");
				if (ok) {
					if (!_GetColorFromString(fg, &entry.fg)) {
						// TODO: error("Invalid palette key / or color value (%s) for theme %s.", value, key);
					}
				}
			}

			// bg.
			{
				auto [ok, bg] = table->getString("bg");
				if (ok) {
					if (!_GetColorFromString(bg, &entry.bg)) {
						// TODO: error("Invalid palette key / or color value (%s) for theme %s.", value, key);
					}
				}
			}

			// modifiers.
			{
				uint8_t modifier = 0;
				auto arr = table->getArray("modifiers");
				if (!!arr) {
					if (arr->kind() != 'v') {
						// TODO: error("Invalid modifier array for theme %s.", key);
					} else if (arr->type() != 's') {
						// TODO: error("Invalid modifier array for theme %s.", key);
					} else {
						auto vec = arr->getStringVector();
						for (const std::string& mod : *vec) {
							if (mod == "bold")             modifier |= Modifier::BOLD;
							else if (mod == "italic")      modifier |= Modifier::ITALIC;
							else if (mod == "underline")   modifier |= Modifier::UNDERLINE;
							else if (mod == "crossed_out") modifier |= Modifier::CROSSED;
							else if (mod == "dim")         modifier |= Modifier::DIM;
							else if (mod == "reversed")    modifier |= Modifier::REVERSED;
							else {
								// error("Invalid modifier value (%s) for theme %s.", mod, key);
							}
						}
					}
				}
				entry.modifier = modifier;
			}
		}
	}

	_CacheUiEntries();
}


void ThemeManager::RegisterListener(ThemeListener* listener) {
	listeners.push_back(listener);
}


void ThemeManager::_OnThemeChanged() {
	for (ThemeListener* listener : listeners) {
		listener->ThemeChanged(theme);
	}
}


const Theme* ThemeManager::GetTheme(const std::string& name) const {
	const auto& iter = themes.find(name);
	if (iter == themes.end()) return nullptr;
	return iter->second.get();
}


void ThemeManager::AddTheme(std::unique_ptr<Theme> theme, bool update) {
	const std::string& name = theme->GetName();
	themes[theme->GetName()] = std::move(theme);
	if (update) SetTheme(name);
}


void ThemeManager::SetTheme(const std::string& name) {
	const auto& iter = themes.find(name);
	if (iter == themes.end()) {
		// TODO: error("No theme named '%s' exists.", name);
		return;
	}
	theme = iter->second.get();
	_OnThemeChanged();
}


void BufferThemeCache::OnBufferChanged(Buffer* buffer) {
	CacheThemelets(buffer);
}


void BufferThemeCache::CacheThemelets(Buffer* buffer) {

	ASSERT(theme_ptr != nullptr, OOPS);
	const Theme* theme = *theme_ptr;

	Color text = (theme != nullptr)
		? (theme)->GetUiEntries().text.fg
		: WHITE;
	themelets.assign(buffer->GetSize(), {text, 0});

	if (theme == nullptr) return;
	
	const std::vector<HighlightSlice>& slices = buffer->GetHighlighter().GetHighlightSlices();

	for (auto it = slices.begin(); it != slices.end(); ++it) {

		// For a key like "keyword.control.conditional" if not exists, we again
		// search for "keyword.control" and then "keyword".
		std::string name = it->capture;
		const ThemeEntry* entry = theme->GetEntry(name);
		if (entry == nullptr) continue;

		for (int i = it->start; i < it->end; i++) {
			themelets[i].color = entry->fg;
			themelets[i].modifiers = entry->modifier;
		}
	}
}
