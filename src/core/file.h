// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <memory>

#include "common.h"
#include "theme.h"
#include "buffer.h"
#include "history.h"
#include "highligther.h"

class Window;


class File :
	ThemeListener,
	HistoryListener,
  BufferListener {

	// Only create this object as a unique pointer.
public: static std::unique_ptr<File> New(Window* window) {
	return std::unique_ptr<File>(new File(window));
}
private: File(Window* window);

public:

	void OnHistoryChanged(History* history) override;
	void OnThemeChanged(const Theme* theme) override;
	void OnBufferChanged(Buffer* buffer) override;

	// TODO: Get is pointer and reference make all reference.
	const Buffer* GetBuffer() const { return buffer.get(); }
	Buffer* GetBuffer() { return buffer.get(); }

	History& GetHistory() { return *history; }
	const History& GetHistory() const { return *history; }

	Highlighter& GetHighlighter() { return *highlighter; }

	const BufferThemeCache* GetThemeCache() const;

	void SetLanguage(const Language* lang);

private:
	std::unique_ptr<Highlighter> highlighter = Highlighter::New();
	std::unique_ptr<Buffer> buffer = Buffer::New();
	std::unique_ptr<History> history = History::New();
	std::unique_ptr<BufferThemeCache> themecache;

	bool crlf = false;
};
