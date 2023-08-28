// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT


#include "minibuffer.hpp"
#include "core/theme.hpp"
#include "core/font.hpp"


MiniBuffer::MiniBuffer() {

	// We store the textbox content as a temp file that doesn't has a path.
	std::unique_ptr<TextBox> tb = std::make_unique<TextBox>(false, Buffer::New());
	textbox = static_cast<TextBox*>(AddChild(std::move(tb)));
}


void MiniBuffer::Clean() {
	textbox->Clean();
}


void MiniBuffer::_Draw(Size area) {

	Size char_size = FontManager::GetFont()->GetCharSize();
	const UiThemeCache& ui = ThemeManager::GetCurrentTheme()->GetUiEntries();

	DrawRectangle(0, 0, area.width, char_size.height, ui.background.bg);
	textbox->Draw(this, { area.width, char_size.height });
	DrawRenderTexture(textbox->GetCanvas(), { 0, 0 });
}
