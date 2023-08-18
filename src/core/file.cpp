// Copyright (c) 2023 Thakee Nathees

#include "file.h"
#include "core/window.h"


File::File(Window* window) {
	ThemePtr ptr = window->GetThemeManager().GetThemePtr();
	ASSERT(ptr != nullptr, OOPS);
	themecache = BufferThemeCache::New(ptr);
	buffer->RegisterListener(themecache.get());
	history->SetBuffer(buffer.get());
	history->RegisterListener(this);

	window->GetThemeManager().RegisterListener(this);
}


void File::HistoryChanged(History* history) {

}


void File::ThemeChanged(const Theme* theme) {
	themecache->CacheThemelets(buffer.get());
}


const BufferThemeCache* File::GetThemeCache() const {
	ASSERT(themecache != nullptr, OOPS);
	return themecache.get();
}


void File::SetLanguage(const Language* lang) {
	buffer->GetHighlighter().SetLanguage(lang);
}
