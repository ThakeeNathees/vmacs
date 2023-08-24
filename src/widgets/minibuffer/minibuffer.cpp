// Copyright (c) 2023 Thakee Nathees

#include "minibuffer.h"
#include "core/window.h"
#include "core/theme.h"


MiniBuffer::MiniBuffer(Window* window) : Widget(window) {

	// We store the textbox content as a temp file that doesn't has a path.
	std::unique_ptr<TextBox> tb = std::make_unique<TextBox>(window, false, Buffer::New(), nullptr);
	
	int idx = AddChild(std::move(tb));
	textbox = static_cast<TextBox*>(GetChild(idx));
	SetFocusedChild(idx);
}


void MiniBuffer::Clean() {
	textbox->Clean();
}


void MiniBuffer::_Draw(Size area) {
	ASSERT(theme_ptr != nullptr && *theme_ptr != nullptr, OOPS);

	const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
	Vector2i char_size = window->GetFontCharSize();

	DrawRectangle(0, 0, area.width, char_size.y, ui.background.bg);
	textbox->Draw(this, { area.width, char_size.y });
	DrawRenderTexture(textbox->GetCanvas(), { 0, 0 });
	
}
