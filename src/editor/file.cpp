// Copyright (c) 2023 Thakee Nathees

#include "file.h"


File::File() {
	buffer->RegisterListener(themecache.get());
	history->SetBuffer(buffer.get());
	history->RegisterListener(this);
}


void File::HistoryChanged(History* history) {

}


void File::ThemeChanged(const Theme* theme) {
	themecache->CacheThemelets(buffer.get());
}


void File::SetLanguage(const Language* lang) {
	buffer->GetHighlighter().SetLanguage(lang);
}
