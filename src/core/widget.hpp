//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "common.hpp"
#include "event.hpp"
#include "mode.hpp"


class Widget {
public:
	virtual void Init() {}

	virtual bool HandleEvent(const Event& event);
	virtual void Update();
	virtual void Draw(const Widget* parent, Size area) final;
	virtual void Cleanup() {}

	// Create a copy of this widget and return (to split view).
	// By default it'll return nullptr.
	virtual std::unique_ptr<Widget> Copy() const;

	// To traverse between splits, this might not be the propper way to do it
	// but who cares.
	virtual bool IsSplit() const;
	Widget* GetFocusedWidget(); // Returns the non-split widget that has focus.

	// Signals.
	virtual void OnFocusChanged() {}

	void SetMode(const std::string_view name);
	RenderTexture2D GetCanvas() const;

	// Returns the index of the child. Note that if we ever remove a child from
	// the widget this index will be invalid.
	Widget* AddChild(std::unique_ptr<Widget> child, int index = -1);
	std::unique_ptr<Widget> RemoveChild(Widget* widget, int* index = nullptr);
	Widget* GetParent() const;

	int GetChildCount() const;
	Widget* GetChild(int index) const;
	int GetChildIndex(const Widget* child) const;
	Widget* GetNextSibling() const;
	Widget* GetPrevSibling() const;

	bool IsFocused() const;
	void SetFocused();

	Widget* GetFocusedChild() const;

protected:
	// Implement this in all child classes and call the above Draw method when
	// needs to draw the widget.
	virtual void _Draw(Size area) {}

	// Construct and initialize the list of modes.
	void _SetModes(std::shared_ptr<ModeList> mode_list);

	RenderTexture2D canvas = { 0 };

private:
	void _RemoveFocusTree();

	Mode* mode = nullptr; // Current mode in the modes list.
	std::shared_ptr<ModeList> modes;

	Widget* parent = nullptr;
	std::vector<std::unique_ptr<Widget>> children;

	// Pointer to the focused child. When this widget gain focus it'll pass the
	// focus to it's focused_childs along the tree.
	Widget* focused_child = nullptr;

	bool is_focused = false;

};
