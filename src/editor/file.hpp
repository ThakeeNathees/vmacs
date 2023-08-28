//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "core/theme.hpp"

#include "highlighter.hpp"
#include "buffer.hpp"
#include "history.hpp"
#include "language.hpp"


class File : ThemeListener, HistoryListener {

public: static std::unique_ptr<File> New() { return std::unique_ptr<File>(new File); }
private: File();

public:
	void OnHistoryChanged(History* history) override;
	void OnThemeChanged(const Theme* theme) override;

	std::shared_ptr<Buffer> GetBuffer();
	std::shared_ptr<Highlighter> GetHighlighter();

	void SetLanguage(const Language* lang);

private:
	std::shared_ptr<Buffer> buffer = Buffer::New();
	std::shared_ptr<Highlighter> highlighter = Highlighter::New();

	bool crlf = false;
};
