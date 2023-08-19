// Copyright (c) 2023 Thakee Nathees

#include "rootview.h"


std::shared_ptr<Mode> RootView::mode_normal = std::make_shared<Mode>(
	"normal",
	nullptr,
	KeyBindings::New({
		{ "ctrl+w" , RootView::_ModeListenWindow },
		{ "ctrl+shift+;" , RootView::_ModeCommand },
  })
);


std::shared_ptr<Mode> RootView::mode_listen_window = std::make_shared<Mode>(
	"listen_window",
	nullptr,
	KeyBindings::New({
		{ "<other>" , RootView::_ModeNormal },
		{ "w" , RootView::_ModeCommand },
  })
);


std::shared_ptr<Mode> RootView::mode_command = std::make_shared<Mode>(
	"command",
	nullptr,
	KeyBindings::New({
		{ "esc" , RootView::_ModeNormal },
		// { "enter" , RootView::CommandExecute },
  })
);


RootView::RootView(Window* window) : Widget(window) {
	std::unique_ptr<TextEditor> editor = std::make_unique<TextEditor>(window);
	std::unique_ptr<MiniBuffer> minibuffer = std::make_unique<MiniBuffer>(window);

	AddMode(mode_normal);
	AddMode(mode_listen_window);
	AddMode(mode_command);

	AddChild(std::move(editor));
	this->editor = static_cast<TextEditor*>(GetChild(0));

	AddChild(std::move(minibuffer));
	this->minibuffer = static_cast<MiniBuffer*>(GetChild(1));

	SetFocusedChild(0);
}


void RootView::_Draw(Size area) {
	Vector2i char_size = window->GetFontCharSize();

	editor->Draw(this, { area.width, area.height - char_size.y });
	DrawRenderTexture(editor->GetCanvas(), { 0, 0 });

	minibuffer->Draw(this, { area.width, char_size.y });
	DrawRenderTexture(minibuffer->GetCanvas(), { 0, (float)area.height - char_size.y });
}


void RootView::_ModeListenWindow(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("listen_window");
}


void RootView::_ModeNormal(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("normal");
	rv->minibuffer->Clean();
	rv->SetFocusedChild(rv->editor);
}


void RootView::_ModeCommand(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("command");
	rv->SetFocusedChild(rv->minibuffer);
}
