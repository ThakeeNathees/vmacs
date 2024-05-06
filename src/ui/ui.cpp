//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


// Static member initialization.
KeyTree Tab::keytree;
KeyTree Ui::keytree;

// -----------------------------------------------------------------------------
// Window.
// -----------------------------------------------------------------------------


Window::Window(const KeyTree* keytree) : EventHandler(keytree) {}

void Window::Draw(FrameBuffer buff, Position pos, Size area) {
  this->pos = pos;
  this->area = area;
  _Draw(buff, pos, area);
}


bool Window::HandleEvent(const Event& event) {
  if (event.type == Event::Type::MOUSE) {
    if (event.mouse.button == Event::MouseButton::MOUSE_WHEEL_UP ||
        event.mouse.button == Event::MouseButton::MOUSE_WHEEL_DOWN) {
      int x = event.mouse.x;
      int y = event.mouse.y;
      if (x < pos.col || x >= pos.col + area.width) return false;
      if (y < pos.row || y >= pos.row + area.height) return false;
    }
  }
  // TODO: If mouse wheel event, check bounds.
  return _HandleEvent(event);
}


// TOOD: Call OnActive callback on the inherited classes.
void Window::SetActive(bool active) {
  this->active = active;
  OnFocusChanged(active);
}


bool Window::IsActive() const {
  return active;
}


void Window::OnFocusChanged(bool focus) {}


// -----------------------------------------------------------------------------
// Split.
// -----------------------------------------------------------------------------


Split* Split::InsertChild(int index, std::unique_ptr<Split> child) {
  ASSERT(index >= 0 && index <= children.size(), OOPS);
  child->parent = this;
  children.insert(children.begin() + index, std::move(child));
  return children[index].get();
}


Split* Split::Vsplit(bool right) {
  ASSERT(type == Type::LEAF, OOPS);

  if (parent == nullptr || parent->type == Type::HORIZONTAL) {
    type = Type::VERTICAL;
    auto l = std::make_unique<Split>();
    auto r = std::make_unique<Split>();
    if (right) l->window = std::move(window);
    else r->window = std::move(window);
    Split* split_left  = InsertChild(children.size(), std::move(l));
    Split* split_right = InsertChild(children.size(), std::move(r));
    return right ? split_right : split_left;
  }

  ASSERT(parent->type == Type::VERTICAL, OOPS);
  int index = GetIndexInParent() + ((int)right);
  auto s = std::make_unique<Split>();
  return this->parent->InsertChild(index, std::move(s));
}


Split* Split::Hsplit(bool bottom) {
  ASSERT(type == Type::LEAF, OOPS);

  if (parent == nullptr || parent->type == Type::VERTICAL) {
    type = Type::HORIZONTAL;
    auto t = std::make_unique<Split>();
    auto b = std::make_unique<Split>();
    if (bottom) t->window = std::move(window);
    else b->window = std::move(window);
    Split* split_top    = InsertChild(children.size(), std::move(t));
    Split* split_bottom = InsertChild(children.size(), std::move(b));
    return bottom ? split_bottom : split_top;
  }

  ASSERT(parent->type == Type::HORIZONTAL, OOPS);
  int index = GetIndexInParent() + ((int)bottom);
  auto s = std::make_unique<Split>();
  return this->parent->InsertChild(index, std::move(s));
}


Split* Split::GetChild(int index) const {
  ASSERT(index >= 0 && index < children.size(), OOPS);
  return children[index].get();
}


int Split::GetIndexInParent() const {
  ASSERT(parent != nullptr, OOPS);
  for (int i = 0; i < parent->children.size(); i++) {
    if (parent->children[i].get() == this) return i;
  }
  UNREACHABLE();
  return -1;
}


void Split::SetWindow(std::unique_ptr<Window> window) {
  ASSERT(type == Type::LEAF, OOPS);
  this->window = std::move(window);
}


Window* Split::GetWindow() {
  return window.get();
}


Split* Split::GetRoot() {
  Split* curr = this;
  while (curr->parent) {
    curr = curr->parent;
  }
  return curr;
}


Split::Iterator Split::Iterate() {
  return Iterator(this);
}


Split::Iterator::Iterator(Split* root) {
  curr = LeftMostLeaf(root);
}


Split* Split::Iterator::LeftMostLeaf(Split* node) const {
  ASSERT(node != nullptr, OOPS);
  Split* curr = node;
  while (curr->type != Split::Type::LEAF) {
    ASSERT(!curr->children.empty(), "Invalid split configuration, non leaf"
                                    "split should contains child splits.");
    curr = curr->children[0].get();
  }
  return curr;
}


Split* Split::Iterator::GetNextChild(Split* node) const {
  if (node->parent == nullptr) return nullptr;

  int child_count = node->parent->children.size();
  int child_index = node->GetIndexInParent();

  // If the parent has another child, lets get there.
  if (child_index < child_count-1) {
    Split* next_child = node->parent->children[child_index+1].get();
    return LeftMostLeaf(next_child);
  }

  // If we reached here, we're the last child of our parent or the parn
  return GetNextChild(node->parent);
}


Split* Split::Iterator::Get() const {
  return curr;
}


void Split::Iterator::Next() {
  ASSERT(curr->type == Split::Type::LEAF, OOPS);
  ASSERT(curr != nullptr, "Iteration has finished already.");

  // If we're at the root, we're done.
  if (curr->parent == nullptr) {
    curr = nullptr;
    return;
  }

  curr = GetNextChild(curr);
}


// -----------------------------------------------------------------------------
// Tab.
// -----------------------------------------------------------------------------


Tab::Tab(std::unique_ptr<Split> root_, Split* active_)
  : EventHandler(&keytree), root(std::move(root_)) {

  // TODO: Validate the root and all of it's childs are met our invariant.
  ASSERT(root != nullptr, "Root split was nullptr.");
  if (active_ == nullptr) {
    active_ = root->Iterate().Get(); // Get the first leaf of the tree.
  }

  ASSERT(active_ != nullptr, OOPS);
  ASSERT(active_->type == Split::Type::LEAF, OOPS);
  ASSERT(active_->GetRoot() == root.get(), "Given active spit should be part of the given split tree.");

  this->active = active_;

  Window* window = this->active->GetWindow();
  ASSERT(window != nullptr, "A split with un-initialized window did you forget to set one?");
  window->SetActive(true);

  SetMode("*");
}


bool Tab::HandleEvent(const Event& event) {

  if (EventHandler::HandleEvent(event)) return true;

  // Send the event to the inner most child to handle if it cannot we do
  // event bubbling.
  if (active != nullptr && active->window != nullptr) {
    if (active->window->HandleEvent(event)) return true;
  }

  // No one consumed the event.
  return false;
}


void Tab::Update() {
  if (root == nullptr) return;
  for (auto it = root->Iterate(); it.Get() != nullptr; it.Next()) {
    Window* window = it.Get()->window.get();
    if (window == nullptr) continue;
    window->Update();
  }
}


void Tab::Draw(FrameBuffer buff, Position pos, Size area) {
  DrawSplit(buff, root.get(), pos, area);
}


void Tab::DrawSplit(FrameBuffer buff, Split* split, Position pos, Size area) {
  if (split->children.size() == 0) { // Leaf node.
    ASSERT(split->type == Split::Type::LEAF, OOPS);
    if (split->window != nullptr) split->window->Draw(buff, pos, area);
    return;
  }

  ASSERT(split->type != Split::Type::LEAF, OOPS);

  // FIXME: Properly fetch the style from the editor or somewhere else.
  Style style = Editor::GetCurrentTheme()->GetStyle("ui.background.separator");

  // The position to draw the children.
  int x = pos.col, y = pos.row;

  int child_count = split->children.size();

  for (int i = 0; i < child_count; i++) {
    Split* child = split->children[i].get();

    // If it's the last child will take the rest of the space. Not sure if this
    // is correct.
    bool is_last = i == child_count - 1;

    if (split->type == Split::Type::VERTICAL) {
      // The total width including a split line at the end.
      int width = (area.width / child_count) + ((is_last) ? (area.width % child_count) : 0);
      if (!is_last) width--;
      DrawSplit(buff, child, {.col = x, .row = y}, {.width = width, .height = area.height});
      if (!is_last) { 
        DrawVerticalLine(buff, x+width, y, area.height, style);
      }
      x += width+1; // +1 for split line.

    } else if (split->type == Split::Type::HORIZONTAL) {
      int height = (area.height / child_count) + ((is_last) ? (area.height % child_count) : 0);
      if (!is_last) height--; // We'll use one row for drawing the split.
      DrawSplit(buff, child, {.col = x, .row = y}, {.width = area.width, .height = height});
      if (!is_last) { 
        DrawHorizontalLine(buff, x, y+height, area.width, style);
      }
      y += height+1; // +1 for split line.
    }
  }
}



bool Tab::Action_NextWindow(Tab* self) {
  ASSERT(self->active != nullptr, OOPS);
  auto it = Split::Iterator(self->active);
  ASSERT(it.Get() != nullptr, OOPS);
  it.Next(); // Increment the leaf by one.

  ASSERT(self->active->window != nullptr, OOPS);
  self->active->window->SetActive(false);
  if (it.Get() != nullptr) {
    self->active = it.Get();
  } else { // Reached the end of the tree.
    self->active = self->root->Iterate().Get();
  }
  ASSERT(self->active->window != nullptr, OOPS);
  self->active->window->SetActive(true);
  return true;
}


// -----------------------------------------------------------------------------
// Window.
// -----------------------------------------------------------------------------


Ui::Ui() : EventHandler(&keytree) {
  SetMode("*");
}


bool Ui::HandleEvent(const Event& event) {

  if (EventHandler::HandleEvent(event)) return true;

  // Note that if the popup is available we won't send the event to the active
  // child split nodes.
  if (popup.get()) {
    if (popup->HandleEvent(event)) return true;
  } else if (tab && tab->HandleEvent(event)) {
    return true;
  }

  // No one consumed the event.
  return false;
}


void Ui::Update() {
  if (popup.get()) popup->Update();
  tab->Update();
}


void Ui::Draw(FrameBuffer buff) {
  // FIXME(mess): Cleanup this mess.
  tab->Draw(buff, {0, 0}, {.width = buff.width, .height = buff.height-1});
  if (popup.get()) popup->Draw(buff, {0, 0}, {.width = buff.width, .height = buff.height-1});

  Style style_text = Editor::GetCurrentTheme()->GetStyle("ui.text");
  Style style_bg   = Editor::GetCurrentTheme()->GetStyle("ui.background");

  Style style = style_bg.Apply(style_text);
  DrawTextLine(buff, info_bar_text.c_str(), 0, buff.height-1, buff.width, style, true);
}


void Ui::Info(const std::string& msg) {
  info_bar_text = msg;
}


void Ui::Success(const std::string& msg) {
  info_bar_text = msg;
}


void Ui::Warning(const std::string& msg) {
  info_bar_text = msg;
}


void Ui::Error(const std::string& msg) {
  info_bar_text = msg;
}


void Ui::AddTab(std::unique_ptr<Tab> tab) {
  this->tab = std::move(tab);
}


bool Ui::Action_ClosePopup(EventHandler* self) {
  Ui* w = (Ui*) self;
  if (w->popup.get()) {
    w->popup = nullptr; // This will destroy.
    return true;
  }
  return false;
}


bool Ui::Action_PopupFilesFinder(EventHandler* self) {
  Ui* w = (Ui*) self;
  if (w->popup != nullptr) return false;
  w->popup = std::make_unique<FindWindow>(std::make_unique<FilesFinder>());
  return true;
}
