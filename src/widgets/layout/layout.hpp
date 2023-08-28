// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "core/widget.hpp"

#include "widgets/minibuffer/minibuffer.hpp"


class Split : public Widget {
public:
	bool IsSplit() const override;
};


class VSplit : public Split {

private:
	void _Draw(Size area) override;
};


class HSplit : public Split {
private:
	void _Draw(Size area) override;
};



class RootView : public Widget {
public:
	RootView(std::unique_ptr<Widget> main_widget);

private:
	void _Draw(Size area) override;

	static std::shared_ptr<ModeList> _GetModes();
	static void _ModeListenWindow(Widget* w, CommandArgs args);
	static void _SwitchWindow(Widget* w, CommandArgs args);
	static void _ModeNormal(Widget* w, CommandArgs args);
	static void _ModeCommand(Widget* w, CommandArgs args);

	Widget* main_widget = nullptr;
	MiniBuffer* minibuffer = nullptr;

};
