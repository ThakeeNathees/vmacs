// Copyright (c) 2023 Thakee Nathees

#include "file.h"
#include "core/window.h"


File::File(Window* window) {
	ThemePtr ptr = window->GetThemeManager().GetThemePtr();
	ASSERT(ptr != nullptr, OOPS);
	themecache = BufferThemeCache::New(ptr);

	// The register order is super important since they'll be notified
	// in the same order they've registered.
	//
	// Here we cannot cache theme without the highlighter constructed.
	buffer->RegisterListener(highlighter.get());
	buffer->RegisterListener(this);

	history->RegisterListener(this);
	history->SetBuffer(buffer.get());

	window->GetThemeManager().RegisterListener(this);
}


void File::OnHistoryChanged(History* history) {

}


void File::OnThemeChanged(const Theme* theme) {
	themecache->CacheThemelets(buffer.get(), highlighter.get());
}


void File::OnBufferChanged(Buffer* buffer) {
	themecache->CacheThemelets(buffer, highlighter.get());
}


const BufferThemeCache* File::GetThemeCache() const {
	ASSERT(themecache != nullptr, OOPS);
	return themecache.get();
}


void File::SetLanguage(const Language* lang) {
	highlighter->SetLanguage(lang);
}
