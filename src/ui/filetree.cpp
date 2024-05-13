//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


FileTree::FileTree(const Path& path) {
  ASSERT(path.IsDirectory(), OOPS);
  root = std::make_unique<Item>();
  root->path = path;
  root->expand = true;
  root->LoadItems();
}


void FileTree::Item::LoadItems() {

  // TODO: Reload the items even if it's not incomplete since the content can
  // be changed outside.
  if (!is_incomplete) return;

  std::vector<Path> paths;
  Platform::ListDirectory(&paths, path);

  for (Path& path_item : paths) {

    // Only directory and regular files are considered.
    if (!path_item.IsDirectory() && !path_item.IsRegularFile()) continue;

    // FIXME: Not considerting the hidden files (use a toggle).
    if (StartsWith(path_item.FileName(), ".")) continue;

    std::unique_ptr<Item> item = std::make_unique<Item>();
    item->path          = path_item;
    item->is_incomplete = true;
    item->expand        = false;
    item->parent        = this;
    items.push_back(std::move(item));
  }
  std::sort(items.begin(), items.end(),
      [](std::unique_ptr<Item>& l, std::unique_ptr<Item>& r){
        if (l->path.IsDirectory() && !r->path.IsDirectory()) return true;
        if (!l->path.IsDirectory() && r->path.IsDirectory()) return false;
        return l->path.FileName() < r->path.FileName();
      });

  is_incomplete = false;
}


int FileTree::Item::GetIndexInParent() const {
  for (int i = 0; i < parent->items.size(); i++) {
    if (parent->items[i].get() == this) return i;
  }
  return -1;
}


FileTree::Item* FileTree::Item::GetNextSibling() const {
  if (parent == nullptr) return nullptr;

  int index = GetIndexInParent();
  if (index < 0) return nullptr;
  index++; // Index of the next child.
  if (index < parent->items.size()) return parent->items[index].get();
  return nullptr;
}


FileTree::Item* FileTree::GetRoot() {
  return root.get();
}


Window::Type FileTreeWindow::GetType() const {
  return Window::Type::FILETREE;
}


bool FileTreeWindow::_HandleEvent(const Event& event) {
  return false;
}


FileTreeWindow::FileTreeWindow(std::shared_ptr<FileTree> tree) : tree(tree) {
  ASSERT(this->tree->GetRoot()->path.IsDirectory(), OOPS);
  auto& items = this->tree->GetRoot()->items;
  if (!items.empty()) cursor = items[0].get();
}


void FileTreeWindow::_Update() {

}


void FileTreeWindow::_Draw(FrameBuffer& buff, Position pos, Area area) {
  FileTree::Item* root = tree->GetRoot();
  ASSERT(root != nullptr, OOPS);

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  // FIXME: Move this mess. ----------------------------------------------------
  Style style_text      = theme.GetStyle("ui.text");
  Style style_bg        = theme.GetStyle("ui.background");
  Style style_comment   = theme.GetStyle("comment");
  Style style_root_path = style_bg.Apply(style_text).Apply(style_comment);
  style_root_path.attrib |= VMACS_CELL_BOLD;
  // FIXME: Move this mess. ----------------------------------------------------

  pos.y++; // Margin at the top 1.

  // Draw the Root path.
  DrawTextLine(
    buff,
    root->path.String().c_str(),
    pos,
    area.width,
    style_root_path,
    icons,
    false,
    true);
  pos.y += 2; // 1 Space between the path and the tree.
  DrawDirItems(root, buff, pos, area, 0);
}


// FIXME (mess): This method is a total mess.
int FileTreeWindow::DrawDirItems(FileTree::Item* dir, FrameBuffer& buff, Position pos, Area area, int indent) {

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  // FIXME: Move this mess. ----------------------------------------------------
  Style style_text   = theme.GetStyle("ui.text");
  Style style_bg     = theme.GetStyle("ui.background");
  Style style_keyword   = theme.GetStyle("keyword");
  Style style_whitespace = theme.GetStyle("ui.virtual.whitespace");
  Style style_selection  = theme.GetStyle("ui.menu");
  style_selection.fg = std::nullopt;
  // --------------------------------------------------------------------------

  ASSERT(dir != nullptr, OOPS);
  ASSERT(dir->expand, OOPS);

  // Maybe run this in a different thread.
  if (dir->is_incomplete) dir->LoadItems();

  // We store the initial y coordinate to know how many lines this directory
  // has drawn at the end of the function.
  const int y_start = pos.y;

  // FIXME: Move this somewhere.
  const int INDENTATION = 2;

  for (auto& item : dir->items) {

    // FIXME:
    // -------------------------------------------------------------------------
    Style style_file   = style_bg.Apply(style_text);
    Style style_dir    = style_file.Apply(style_keyword);
    Style style_expand = style_bg.Apply(style_whitespace);
    // -------------------------------------------------------------------------

    if (item.get() == cursor) {
      DrawRectangleFill(buff, pos, Area(area.width, 1), style_selection);
      style_file.ApplyInplace(style_selection);
      style_dir.ApplyInplace(style_selection);
      style_expand.ApplyInplace(style_selection);
    }

    if (item->path.IsDirectory()) {

      // Temproarly move forward to draw.
      pos.x += indent; area.width -= indent;
      if (item->expand) {
        DrawIcon(buff, icons.angle_down, pos, style_expand);
      } else {
        DrawIcon(buff, icons.angle_right, pos, style_expand);
      }
      pos.x += 2; area.width -= 2; // Space betwen icon and name.
      {
        DrawTextLine(buff, item->path.FileName().c_str(), pos, area.width, style_dir, icons, false);
        pos.y++;
      }
      pos.x -= (indent+2); area.width += (indent+2); // Bring back the position.

      // Draw recursively the child if they opened.
      if (item->expand) {
        pos.y += DrawDirItems(item.get(), buff, pos, area, indent+INDENTATION);
      }

    } else {
      pos.x += indent+2; area.width -= (indent+2);
      {
        DrawTextLine(buff, item->path.FileName().c_str(), pos, area.width, style_file, icons, false);
        pos.y++;
      }
      pos.x -= indent+2; area.width += (indent+2);
    }
  }

  return pos.y - y_start;
}


bool FileTreeWindow::Action_CursorUp(FileTreeWindow* self) {
  if (self->cursor == nullptr) return false;

  const FileTree::Item* curr = self->cursor;
  ASSERT(curr->parent != nullptr, OOPS); // Cursor cannot be at the root.

  const int index = curr->GetIndexInParent();

  // 1. If we're the first child of a parent which is not the root,
  //    go there.
  if (index == 0) {

    // If parent is root, we cannot go there.
    if (curr->parent->parent == nullptr) {
      return true;
    }

    self->cursor = curr->parent;
    return true;
  }

  // 2. Try the prev sibling.
  self->cursor = curr->parent->items[index-1].get();
  ASSERT(self->cursor != nullptr, OOPS);
  return true;
}


bool FileTreeWindow::Action_CursorDown(FileTreeWindow* self) {
  if (self->cursor == nullptr) return false;

  const FileTree::Item* curr = self->cursor;
  ASSERT(curr->parent != nullptr, OOPS); // Cursor cannot be at the root.

  // 1. We're in a directory which is expanded and has children go inside.
  if (curr->path.IsDirectory() && curr->expand && !curr->items.empty()) {
    self->cursor = curr->items[0].get();
    return true;
  }

  // 2. Try the next sibling.
  FileTree::Item* next = curr->GetNextSibling();
  if (next != nullptr) {
    self->cursor = next;
  }
  return true;
}


bool FileTreeWindow::Action_GotoParent(FileTreeWindow* self) {
  if (self->cursor == nullptr) return false;
  ASSERT(self->cursor->parent != nullptr, OOPS); // Cursor cannot be at the root.

  // If parent is root, we cannot go there.
  if (self->cursor->parent->parent == nullptr) return true;
  self->cursor = self->cursor->parent;
  return true;
}


bool FileTreeWindow::Action_SelectPath(FileTreeWindow* self) {
  if (self->cursor == nullptr) return false;

  if (self->cursor->path.IsDirectory()) {
    self->cursor->expand = !self->cursor->expand;
    self->cursor->LoadItems();
    return true;
  }

  // If we reached here, the selection is a regular file.
  Editor* e = Editor::Singleton().get();
  return ((Ui*)e->GetUi())->JumpToDocument(self->cursor->path, {0, 0});
}
