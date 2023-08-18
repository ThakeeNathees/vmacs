

#include "minibuffer.h"
#include "core/window.h"
#include "core/theme.h"


MiniBuffer::MiniBuffer(Window* window) : Widget(window) {

	// We store the textbox content as a temp file that doesn't has a path.
	textbox = std::make_unique<TextBox>(window, File::New(window), false);
}


bool MiniBuffer::HandleEvent(const Event& event) {
	if (textbox->HandleEvent(event)) return true;
	return Widget::HandleEvent(event);
}


void MiniBuffer::Update() {
	textbox->Update();
}


void MiniBuffer::_Draw(Size area) {
	ASSERT(theme_ptr != nullptr && *theme_ptr != nullptr, OOPS);

	const UiThemeCache& ui = (*theme_ptr)->GetUiEntries();
	Vector2i char_size = window->GetFontCharSize();

	DrawRectangle(0, 0, area.width, char_size.y, ui.background.bg);
	textbox->Draw(this, { area.width, char_size.y });
	DrawRenderTexture(textbox->GetCanvas(), { 0, 0 });
	
}
