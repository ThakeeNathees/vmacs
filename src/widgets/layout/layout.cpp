// Copyright (c) 2023 Thakee Nathees

#include "layout.h"


HSplit::HSplit(
	Window* window,
	std::unique_ptr<Widget> first,
	std::unique_ptr<Widget> second)
	: Widget(window) {

	ASSERT(first != nullptr && second != nullptr, OOPS);
	AddChild(std::move(first));
	AddChild(std::move(second));
	SetFocusedChild(0);

	AddMode(
		std::make_shared<Mode>(
			"default",
			nullptr,
			KeyBindings::New(
				{
					{ "ctrl+w", HSplit::_Test }
				}
			)),
		true);
}


void HSplit::_Draw(Size area) {
	Vector2i char_size = window->GetFontCharSize();

	GetChild(0)->Draw(this, { area.width, area.height - char_size.y });
	DrawRenderTexture(GetChild(0)->GetCanvas(), { 0, 0 });

	GetChild(1)->Draw(this, {area.width, char_size.y});
	DrawRenderTexture(GetChild(1)->GetCanvas(), { 0, (float) area.height - char_size.y });
}
