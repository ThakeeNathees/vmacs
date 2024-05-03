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


void Window::Draw(FrameBuffer buff) {
  // FIXME(mess): Cleanup this mess.
  tab->Draw(buff, {0, 0}, {.width = buff.width, .height = buff.height-1});
  if (popup.get()) popup->Draw(buff, {0, 0}, {.width = buff.width, .height = buff.height-1});

  Color color_bg = Global::GetCurrentTheme()->GetStyleOr("ui.background", {.fg=0, .bg=0xffffff, .attrib=0}).bg;
  DrawTextLine(buff, info_bar_text.c_str(), 0, buff.height-1, buff.width, 0xffffff, color_bg, 0, true);
}


void Window::Info(const std::string& msg) {
  info_bar_text = msg;
}


void Window::Success(const std::string& msg) {
  info_bar_text = msg;
}


void Window::Warning(const std::string& msg) {
  info_bar_text = msg;
}


void Window::Error(const std::string& msg) {
  info_bar_text = msg;
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
