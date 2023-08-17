// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <memory>

#include "core/common.h"
#include "core/theme.h"
#include "core/buffer.h"

#include "history.h"
#include "core/theme.h"


class File : public ThemeListener, HistoryListener {

	// Only create this object as a unique pointer.
public: static std::unique_ptr<File> New() { return std::unique_ptr<File>(new File); }
private: File();

public:

	void HistoryChanged(History* history) override;
	void ThemeChanged(const Theme* theme) override;

	// TODO: Get is pointer and reference make all reference.
	const Buffer* GetBuffer() const { return buffer.get(); }
	History& GetHistory() { return *history; }
	const History& GetHistory() const { return *history; }

	void SetLanguage(const Language* lang);

	// FIXME: this is for debugging.
public:
	std::unique_ptr<BufferThemeCache> themecache = BufferThemeCache::New();

private:
	std::unique_ptr<Buffer> buffer = Buffer::New();
	std::unique_ptr<History> history = History::New();

	bool crlf = false;
};
