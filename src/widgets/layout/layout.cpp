// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "layout.hpp"
#include "core/theme.hpp"
#include "core/font.hpp"


std::shared_ptr<ModeList> RootView::_GetModes() {
	static std::shared_ptr<ModeList> modes = std::make_shared<ModeList>(
		std::initializer_list<std::shared_ptr<Mode>>({

			Mode::New("normal", {
				{ "ctrl+w" , RootView::_ModeListenWindow },
				{ "ctrl+shift+;" , RootView::_ModeCommand },
			}),

			Mode::New("listen_window", false, { // Listen key after ctrl+w.
				{ "<other>" , RootView::_ModeNormal },
				{ "w" , RootView::_SwitchWindow },
			}),

			Mode::New("command", {
				{ "esc" , RootView::_ModeNormal },
				// { "enter" , RootView::CommandExecute },
			}),

    }));

	return modes;
}


bool Split::IsSplit() const {
	return true;
}


RootView::RootView(std::unique_ptr<Widget> main_widget) {

	_SetModes(_GetModes());

	this->main_widget = AddChild(std::move(main_widget));

	std::unique_ptr<MiniBuffer> minibuffer = std::make_unique<MiniBuffer>();
	this->minibuffer = static_cast<MiniBuffer*>(AddChild(std::move(minibuffer)));
}


void RootView::_Draw(Size area) {
	
	Size char_size = FontManager::GetFont()->GetCharSize();

	main_widget->Draw(this, { area.width, area.height - char_size.height });
	DrawRenderTexture(main_widget->GetCanvas(), { 0, 0 });

	minibuffer->Draw(this, { area.width, char_size.height });
	DrawRenderTexture(minibuffer->GetCanvas(), { 0, (float)area.height - char_size.height });
}


static void FocusNextWidget(Widget* widget, bool& focus_next, bool& done) {
	if (done) return;

	if (!widget->IsSplit()) {
		if (focus_next) {
			widget->SetFocused();
			done = true;
		} else {
			focus_next = widget->IsFocused();
		}
		return;
	}

	for (int i = 0; i < widget->GetChildCount(); i++) {
		FocusNextWidget(widget->GetChild(i), focus_next, done);
		if (done) return;
	}

}


void RootView::_ModeListenWindow(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("listen_window");
}


void RootView::_SwitchWindow(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);

	bool focus_next = false, done = false;
	FocusNextWidget(rv->main_widget , focus_next, done);
	if (!done) {
		Widget* first = rv->main_widget;
		while (first->IsSplit() && first->GetChildCount() > 0) {
			Widget* child = first->GetChild(0);
			if (!child->IsSplit()) {
				child->SetFocused();
				break;
			} else {
				first = child;
			}
		}
	}

	rv->SetMode("normal");
}


void RootView::_ModeNormal(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("normal");
	rv->minibuffer->Clean();
	rv->main_widget->SetFocused();
}


void RootView::_ModeCommand(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("command");
	rv->minibuffer->SetFocused();
}


void VSplit::_Draw(Size area) {

	int count = GetChildCount();

	float width = area.width / (float) count;
	for (int i = 0; i < count; i++) {
		if (i != 0) {
			DrawLine((int)(i * width), 0, (int)(i * width), area.height, WHITE);
		}
		Widget* w = GetChild(i);
		w->Draw(this, { (int) width, area.height });
		DrawRenderTexture(w->GetCanvas(), { i * width, 0 });
	}
}


void HSplit::_Draw(Size area) {

	int count = GetChildCount();
	float height = area.height / (float) count;
	for (int i = 0; i < count; i++) {
		if (i != 0) {
			DrawLine(0, (int)(i * height), area.width, (int)(i * height), WHITE);
		}
		Widget* w = GetChild(i);
		w->Draw(this, { area.width, (int) height });
		DrawRenderTexture(w->GetCanvas(), { 0, i * height });
	}
}
