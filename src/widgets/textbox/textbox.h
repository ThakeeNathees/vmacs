// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <memory>

#include "core/common.h"
#include "core/widget.h"
#include "core/cursors.h"
#include "core/buffer.h"


class TextBox : public Widget {
public:
	TextBox(
		Window* window,
		bool multiline,
		std::shared_ptr<Buffer> buffer,
		std::shared_ptr<BufferThemeCache> themecache);

	void Update();
	void OnFocusChanged();

	// Returns a weak reference to buffer.
	Buffer* GetBuffer() const { return buffer.get(); }
	Coord GetViewStart() const { return view_start; }

	void ClearCursors();
	void Clean();

private:
	void _EnsureCursorsOnView();

	void _Draw(Size area) override;
	void _DrawSelections(Size area);
	void _DrawCursors(Size area);
	void _DrawText(Size area);

	static ModeList _GetModes();
	static void _InsertText(Widget* w, CommandArgs args);
	static void _InsertLine(Widget* w, CommandArgs args);
	static void _CursorsUp(Widget* w, CommandArgs args);
	static void _CursorsDown(Widget* w, CommandArgs args);
	static void _CursorsLeft(Widget* w, CommandArgs args);
	static void _CursorsRight(Widget* w, CommandArgs args);
	static void _CursorsHome(Widget* w, CommandArgs args);
	static void _CursorsEnd(Widget* w, CommandArgs args);
	static void _AddCursorDown(Widget* w, CommandArgs args);
	static void _AddCursorUp(Widget* w, CommandArgs args);
	static void _Backspace(Widget* w, CommandArgs args);
	static void _SelectRight(Widget* w, CommandArgs args);
	static void _SelectLeft(Widget* w, CommandArgs args);
	static void _Undo(Widget* w, CommandArgs args);
	static void _Redo(Widget* w, CommandArgs args);

	std::shared_ptr<Buffer> buffer;
	std::shared_ptr<BufferThemeCache> themecache;

	bool multiline = false;

	Cursors cursors;
	Coord view_start = { 0 };
	Size text_area = { 0 }; // We store the size of the text area here to adjust the view.

};
