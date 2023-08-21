// Copyright (c) 2023 Thakee Nathees

#pragma once

#include "core/common.h"
#include "core/widget.h"
#include "widgets/textbox/textbox.h"


// This is not actually a buffer but a widget at the bottom of the screen
// printing the logs and prompting commands, In vim they're calling it
// mini buffer so do we.
class MiniBuffer : public Widget {
public:
	MiniBuffer(Window* window);

	void Clean();

private:
	void _Draw(Size area) override;

	TextBox* textbox;
};
