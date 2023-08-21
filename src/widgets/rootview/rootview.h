// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <memory>

#include "core/window.h"
#include "core/widget.h"

#include "widgets/editor/editor.h"
#include "widgets/minibuffer/minibuffer.h"


class RootView : public Widget {
public:
	RootView(Window* window);

private:
	void _Draw(Size area) override;

	static ModeList _GetModes();
	static void _ModeListenWindow(Widget* w, CommandArgs args);
	static void _ModeNormal(Widget* w, CommandArgs args);
	static void _ModeCommand(Widget* w, CommandArgs args);

	TextEditor* editor = nullptr;
	MiniBuffer* minibuffer = nullptr;

};
