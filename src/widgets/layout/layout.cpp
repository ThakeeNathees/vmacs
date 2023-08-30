// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "layout.hpp"
#include "core/theme.hpp"
#include "core/font.hpp"


Split::Split(Split::Type type) : type(type) {
}


std::unique_ptr<Split> Split::New(Split::Type type) {
	switch (type) {
		case HSPLIT:
		return std::make_unique<HSplit>();

		case VSPLIT:
		return std::make_unique<VSplit>();
	}

	UNREACHABLE();
}


bool Split::IsSplit() const {
	return true;
}


Split::Type Split::GetType() const {
	return type;
}


static void _SplitTraverse(std::vector<Widget*>& list, Widget* widget) {
	if (widget == nullptr) return;

	if (!widget->IsSplit()) {
		list.push_back(widget);
		return;
	}

	for (int i = 0; i < widget->GetChildCount(); i++) {
		_SplitTraverse(list, widget->GetChild(i));
	}
}


std::vector<Widget*> Split::Traverse() {
	std::vector<Widget*> ret;
	_SplitTraverse(ret, this);
	return ret;
}


Widget* Split::SplitChild(Widget* widget, Split::Type type) {
	Widget* parent = widget->GetParent();
	if (parent == nullptr) return nullptr;

	if (parent->IsSplit() && static_cast<Split*>(parent)->type == type) {
		std::unique_ptr<Widget> copy = widget->Copy();
		if (copy == nullptr) return nullptr;
		parent->AddChild(std::move(copy), parent->GetChildIndex(widget) + 1)->SetFocused();
		return parent;

	} else {
		int index;
		bool is_focused_child = parent->GetFocusedChild() == widget;
		std::unique_ptr<Widget> first = parent->RemoveChild(widget, &index);
		std::unique_ptr<Widget> second = widget->Copy();
		if (first == nullptr || second == nullptr) return nullptr;
		std::unique_ptr<Split> split = Split::New(type);
		split->AddChild(std::move(first));
		Widget* next_focus = split->AddChild(std::move(second));

		Widget* new_child = parent->AddChild(std::move(split), index);
		if (is_focused_child) next_focus->SetFocused();
		return new_child;
	}

	UNREACHABLE();
}


VSplit::VSplit() : Split(Split::VSPLIT) {
}


HSplit::HSplit() : Split(Split::HSPLIT) {
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
