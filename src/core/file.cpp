// Copyright (c) 2023 Thakee Nathees

#include "file.h"
#include "core/window.h"


File::File(Window* window) {
	ThemePtr ptr = window->GetThemeManager().GetThemePtr();
	ASSERT(ptr != nullptr, OOPS);
	themecache = BufferThemeCache::New(ptr);
	themecache->SetHighlighter(highlighter);

	buffer->RegisterListener(highlighter.get());
	buffer->GetHistory().RegisterListener(this);
	buffer->RegisterListener(themecache.get());

	window->GetThemeManager().RegisterListener(this);
}


void File::OnHistoryChanged(History* history) {

}


void File::OnThemeChanged(const Theme* theme) {
	themecache->CacheThemelets(buffer.get());
}


void File::OnBufferChanged(Buffer* buffer) {
	
}


const BufferThemeCache* File::GetThemeCache() const {
	ASSERT(themecache != nullptr, OOPS);
	return themecache.get();
}


void File::SetLanguage(const Language* lang) {
	highlighter->SetLanguage(lang);
}
