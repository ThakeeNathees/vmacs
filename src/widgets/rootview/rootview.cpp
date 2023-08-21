// Copyright (c) 2023 Thakee Nathees

#include "rootview.h"


ModeList RootView::_GetModes() {
  static ModeList modes = std::make_shared<std::vector<std::shared_ptr<Mode>>>(
    std::initializer_list<std::shared_ptr<Mode>>({

      Mode::New("normal", {
				{ "ctrl+w" , RootView::_ModeListenWindow },
				{ "ctrl+shift+;" , RootView::_ModeCommand },
      }),

			Mode::New("listen_window", { // Listen key after ctrl+w.
		    { "<other>" , RootView::_ModeNormal },
		    { "w" , RootView::_ModeCommand },
	    }),

			Mode::New("command", {
				{ "esc" , RootView::_ModeNormal },
				// { "enter" , RootView::CommandExecute },
		  }),

    })
  );

  return modes;
}



RootView::RootView(Window* window) : Widget(window) {

	_UpdateModes(_GetModes());

	std::unique_ptr<TextEditor> editor = std::make_unique<TextEditor>(window);
	std::unique_ptr<MiniBuffer> minibuffer = std::make_unique<MiniBuffer>(window);

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
