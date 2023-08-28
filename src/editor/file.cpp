//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include "file.hpp"


File::File() {
	buffer->RegisterListener(highlighter.get());
	buffer->GetHistory().RegisterListener(this);
	ThemeManager::RegisterListener(this);
}


void File::OnThemeChanged(const Theme* theme) {
	highlighter->GetCache()->CacheThemelets(buffer.get(), highlighter.get());
}


void File::OnHistoryChanged(History* history) {

}


void File::SetLanguage(const Language* lang) {
	highlighter->SetLanguage(lang);
}


std::shared_ptr<Buffer> File::GetBuffer() {
	return buffer;
}


std::shared_ptr<Highlighter> File::GetHighlighter() {
	return highlighter;
}
