// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include <raylib.h>
#include <tree_sitter/parser.h>
#include <tree_sitter/api.h>

#include "common.h"

// Treesitter get language function.
typedef const TSLanguage* (*GetLanguageFn)(void);

// TODO: The highlighting alorithm is not quite optimal, but did this way to
// complete the development reapidly and need to be fixed in the future.


class Language {

  // Only create this object as a unique pointer.
public:  static std::unique_ptr<Language> New(const std::string& name) { return std::unique_ptr<Language>(new Language(name)); }
private: Language(const std::string& name) : name(name) {}

public:
	static void LoadLanguages();
	static const Language* GetLanguage(const std::string& name);
	static std::map<std::string, std::unique_ptr<Language>> languages;

	const TSLanguage* Get() const { return data; }
	const std::string& GetName() const { return name; }
	const TSQuery* GetQuery() const { return query; }

	~Language();

private:
	std::string name;
	const TSLanguage* data = nullptr;
	TSQuery* query = nullptr;
};


// Each slice is a substring (or a token) of the buffer and we capture that
// string with a capture string.
//
// Example: in C
//   string = "while"
//   capture = "keyword.control.repeat"
// 
//   string = "if"
//   capture = "keyword.control.conditional"
//
// We apply theme based on the capture.
struct HighlightSlice {
	int start;
	int end;

	const char* capture;
};


class Highlighter {

public:
	Highlighter();
	~Highlighter();

	void Highlight(const char* source, int size);
	void SetLanguage(const Language* lang);

	const std::vector<HighlightSlice>& GetHighlightSlices() const { return slices; }
	const HighlightSlice* GetHighlightSlice(int index) const;

private:

	//void _SortSlices();

	// TODO: Write a better datastructure and optimize.
	//
	// Consider '"a\nb"' here we have two slices:
	// 1. string (0, 6)
	// 2. escape_seequence (2, 4)
	//
	// We'll sort the slices in a way that smaller slice comes first.
	std::vector<HighlightSlice> slices;

	TSParser* parser = nullptr;
	TSTree* tree = nullptr;
	const Language* lang = nullptr;
};
