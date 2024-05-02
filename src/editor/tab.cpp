//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "editor.hpp"


// Static member initialize.
KeyTree Tab::keytree;


Tab::Tab() : EventHandler(&keytree) {
  SetMode("*");
}


std::unique_ptr<Tab> Tab::FromPane(std::unique_ptr<Pane> pane) {
  std::unique_ptr<Tab>tab = std::make_unique<Tab>();
  tab->panes.push_back(std::move(pane));
  Pane* p = tab->panes[tab->panes.size()-1].get();
  tab->active = p;
  tab->root.pane = p;
  return std::move(tab);
}


bool Tab::HandleEvent(const Event& event) {
  // Note that if the popup is available we won't send the event to the active
  // child split nodes.
  if (popup.get()) {
    if (popup->HandleEvent(event)) return true;
  } else if (active != nullptr) {
    // Send the event to the inner most child to handle if it cannot we do
    // event bubbling.
    if (active->HandleEvent(event)) return true;
  }

  // No one consumed the event, so we'll with the keytree.
  return EventHandler::HandleEvent(event);
}


void Tab::Update() {
  if (popup.get()) popup->Update();
  for (auto& pane : panes) {
    pane->Update();
  }
}


void Tab::Draw(FrameBuffer buff, Position pos, Size area) {
  DrawSplit(buff, &root, pos, area);
  if (popup.get()) popup->Draw(buff, pos, area);
}


void Tab::DrawSplit(FrameBuffer buff, Split* split, Position pos, Size area) {
  if (split->children.size() == 0) { // Leaf node.
    ASSERT(split->type == Split::Type::LEAF, OOPS);
    if (split->pane != nullptr) split->pane->Draw(buff, pos, area);
    return;
  }

  ASSERT(split->type != Split::Type::LEAF, OOPS);

  // The position to draw the children.
  int x = pos.col, y = pos.row;

  int child_count = split->children.size();

  for (int i = 0; i < child_count; i++) {
    Split* child = split->children[i].get();

    // If it's the last child will take the rest of the space. Not sure if this
    // is correct.
    bool is_last = i == child_count - 1;

    if (split->type == Split::Type::VERTICAL) {
      int width = (area.width / child_count) + ((is_last) ? (area.width % child_count) : 0);
      DrawSplit(buff, child, {.col = x, .row = y}, {.width = width, .height = area.height});
      x += width;
    } else if (split->type == Split::Type::HORIZONTAL) {
      int height = (area.height / child_count) + ((is_last) ? (area.height % child_count) : 0);
      DrawSplit(buff, child, {.col = x, .row = y}, {.width = area.width, .height = height});
      y += height;
    }
  }
}


bool Tab::Action_ClosePopup(Tab* self) {
  if (self->popup.get()) {
    self->popup = nullptr; // This will destroy.
    return true;
  }
  return false;
}


// FIXME(mess): This is temp.
#include "ui/ui.hpp"
bool Tab::Action_PopupFilesFinder(Tab* self) {
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindPane>(std::make_unique<FilesFinder>());
  return true;
}
