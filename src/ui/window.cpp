//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"

// FIXME(grep, mess): Everything here are a mess.

// Initialization of the static field.
KeyTree Window::keytree;


Window::Window() : EventHandler(&keytree) {
  SetMode("*");
}


bool Window::HandleEvent(const Event& event) {
  // Note that if the popup is available we won't send the event to the active
  // child split nodes.
  if (popup.get()) {
    if (popup->HandleEvent(event)) return true;
  } else if (tab && tab->HandleEvent(event)) {
    return true;
  }

  // No one consumed the event, so we'll with the keytree.
  return EventHandler::HandleEvent(event);
}


void Window::Update() {
  if (popup.get()) popup->Update();
  tab->Update();
}


void Window::Draw(FrameBuffer buff, Position pos, Size area) {
  tab->Draw(buff, pos, area);
  if (popup.get()) popup->Draw(buff, pos, area);
}


void Window::Info(const std::string& error) {

}


void Window::Success(const std::string& error) {

}


void Window::Warning(const std::string& error) {

}


void Window::Error(const std::string& error) {

}


void Window::AddTab(std::unique_ptr<Tab> tab) {
  this->tab = std::move(tab);
}


bool Window::Action_ClosePopup(EventHandler* self) {
  Window* w = (Window*) self;
  if (w->popup.get()) {
    w->popup = nullptr; // This will destroy.
    return true;
  }
  return false;
}


bool Window::Action_PopupFilesFinder(EventHandler* self) {
  Window* w = (Window*) self;
  if (w->popup != nullptr) return false;
  w->popup = std::make_unique<FindPane>(std::make_unique<FilesFinder>());
  return true;
}
