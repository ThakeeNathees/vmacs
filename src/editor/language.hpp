// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"

#include <tree_sitter/parser.h>
#include <tree_sitter/api.h>


// Treesitter get language function.
typedef const TSLanguage* (*GetLanguageFn)(void);


class Language {

	// Only create this object as a unique pointer.
public:  static std::unique_ptr<Language> New(const std::string& name) { return std::unique_ptr<Language>(new Language(name)); }
private: Language(const std::string& name) : name(name) {}

public:

	void SetData(const TSLanguage* data);
	void SetQuery(const char* source);

	const TSLanguage* GetData() const;
	const std::string& GetName() const;
	const TSQuery* GetQuery() const;

	~Language();

private:
	std::string name;
	const TSLanguage* data = nullptr;
	TSQuery* query = nullptr;
};


class LanguageManager {
public:
	static void Init();
	static void AddLanguage(std::unique_ptr<Language> lang);
	static const Language* GetLanguage(const std::string& name);

private:
	static std::map<std::string, std::unique_ptr<Language>> languages;
};
