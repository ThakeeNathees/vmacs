//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


// Static member initialize.
KeyTree Tab::keytree;

// -----------------------------------------------------------------------------
// Pane.
// -----------------------------------------------------------------------------


Pane::Pane(const KeyTree* keytree) : EventHandler(keytree) {}

void Pane::Draw(FrameBuffer buff, Position pos, Size area) {
  this->pos = pos;
  this->area = area;
  _Draw(buff, pos, area);
}


bool Pane::HandleEvent(const Event& event) {
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
void Pane::SetActive(bool active) {
  this->active = active;
  OnFocusChanged(active);
}


bool Pane::IsActive() const {
  return active;
}


void Pane::OnFocusChanged(bool focus) {}


// -----------------------------------------------------------------------------
// Split.
// -----------------------------------------------------------------------------


void Split::InsertChild(int index, std::unique_ptr<Split> child) {
  ASSERT(index >= 0 && index <= children.size(), OOPS);
  child->parent = this;
  children.insert(children.begin() + index, std::move(child));
}


void Split::Vsplit(bool right) {
  ASSERT(type == Type::LEAF, OOPS);

  if (parent == nullptr || parent->type == Type::HORIZONTAL) {
    type = Type::VERTICAL;
    auto l = std::make_unique<Split>();
    auto r = std::make_unique<Split>();
    if (right) l->pane = std::move(pane);
    else r->pane = std::move(pane);
    InsertChild(children.size(), std::move(l));
    InsertChild(children.size(), std::move(r));
    return;
  }

  ASSERT(parent->type == Type::VERTICAL, OOPS);
  int index = GetIndexInParent() + ((int)right);
  auto s = std::make_unique<Split>();
  this->parent->InsertChild(index, std::move(s));
}


void Split::Hsplit(bool bottom) {
  ASSERT(type == Type::LEAF, OOPS);

  if (parent == nullptr || parent->type == Type::VERTICAL) {
    type = Type::HORIZONTAL;
    auto t = std::make_unique<Split>();
    auto b = std::make_unique<Split>();
    if (bottom) t->pane = std::move(pane);
    else b->pane = std::move(pane);
    InsertChild(children.size(), std::move(t));
    InsertChild(children.size(), std::move(b));
    return;
  }

  ASSERT(parent->type == Type::HORIZONTAL, OOPS);
  int index = GetIndexInParent() + ((int)bottom);
  auto s = std::make_unique<Split>();
  this->parent->InsertChild(index, std::move(s));
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


void Split::SetPane(std::unique_ptr<Pane> pane) {
  ASSERT(type == Type::LEAF, OOPS);
  this->pane = std::move(pane);
}


Pane* Split::GetPane() {
  return pane.get();
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

  Pane* pane = this->active->GetPane();
  ASSERT(pane != nullptr, "A split with un-initialized pane did you forget to set one?");
  pane->SetActive(true);

  SetMode("*");
}


bool Tab::HandleEvent(const Event& event) {

#define return_handled do { ResetCursor(); return true; } while (false)
  // Send the event to the inner most child to handle if it cannot we do
  // event bubbling.
  if (active != nullptr && active->pane != nullptr) {
    if (active->pane->HandleEvent(event)) return_handled;
  }
#undef return_handled

  // No one consumed the event, so we'll with the keytree.
  return EventHandler::HandleEvent(event);
}


void Tab::Update() {
  if (root == nullptr) return;
  for (auto it = root->Iterate(); it.Get() != nullptr; it.Next()) {
    Pane* pane = it.Get()->pane.get();
    if (pane == nullptr) continue;
    pane->Update();
  }
}


void Tab::Draw(FrameBuffer buff, Position pos, Size area) {
  DrawSplit(buff, root.get(), pos, area);
}


void Tab::DrawSplit(FrameBuffer buff, Split* split, Position pos, Size area) {
  if (split->children.size() == 0) { // Leaf node.
    ASSERT(split->type == Split::Type::LEAF, OOPS);
    if (split->pane != nullptr) split->pane->Draw(buff, pos, area);
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



bool Tab::Action_NextPane(Tab* self) {
  ASSERT(self->active != nullptr, OOPS);
  auto it = Split::Iterator(self->active);
  ASSERT(it.Get() != nullptr, OOPS);
  it.Next(); // Increment the leaf by one.

  ASSERT(self->active->pane != nullptr, OOPS);
  self->active->pane->SetActive(false);
  if (it.Get() != nullptr) {
    self->active = it.Get();
  } else { // Reached the end of the tree.
    self->active = self->root->Iterate().Get();
  }
  ASSERT(self->active->pane != nullptr, OOPS);
  self->active->pane->SetActive(true);
  return true;
}
