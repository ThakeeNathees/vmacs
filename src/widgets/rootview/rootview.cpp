// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "rootview.hpp"

#include <vector>

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
				{ "w"       , RootView::_SwitchWindow },
				{ "ctrl+w"  , RootView::_SwitchWindow },
				{ "s"       , RootView::_SplitHorizontal },
				{ "ctrl+s"  , RootView::_SplitHorizontal },
				{ "v"       , RootView::_SplitVertical },
				{ "ctrl+v"  , RootView::_SplitVertical },
			}),

			Mode::New("command", {
				{ "esc" , RootView::_ModeNormal },
				// { "enter" , RootView::CommandExecute },
			}),

    }));

	return modes;
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


void RootView::_ModeListenWindow(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);
	rv->SetMode("listen_window");
}


static Widget* GetFocusedWidget(Widget* widget) {
	if (widget == nullptr) return nullptr;
	if (!widget->IsSplit()) return widget;
	return GetFocusedWidget(widget->GetFocusedChild());
}


void RootView::_SwitchWindow(Widget* w, CommandArgs args) {
	RootView* rv = static_cast<RootView*>(w);

	if (rv->main_widget->IsSplit()) {
		std::vector<Widget*> widgets = static_cast<Split*>(rv->main_widget)->Traverse();

		bool focus_next = false, done = false;
		for (Widget* widget : widgets) {
			if (focus_next) {
				widget->SetFocused();
				done = true;
				break;
			}
			if (widget->IsFocused()) focus_next = true;
		}

		// Nothing switched.
		if (!done && widgets.size() > 0) widgets[0]->SetFocused();
	}

	rv->SetMode("normal");
}


void RootView::_Split(RootView* rv, Split::Type type) {
#define return ?
#define return_defer goto l_defer

	if (!rv->main_widget->IsSplit()) {
		Widget* container = Split::SplitChild(rv->main_widget, type);
		if (container) rv->main_widget = container;
		return_defer;
	}

	Widget* widget = rv->main_widget->GetFocusedWidget();
	if (widget == nullptr) return_defer;
	Split::SplitChild(widget, type);

l_defer:
	rv->SetMode("normal");

#undef return
#undef return_defer
}


void RootView::_SplitHorizontal(Widget* w, CommandArgs args) {
	RootView::_Split(static_cast<RootView*>(w), Split::HSPLIT);
}


void RootView::_SplitVertical(Widget* w, CommandArgs args) {
	RootView::_Split(static_cast<RootView*>(w), Split::VSPLIT);
}
