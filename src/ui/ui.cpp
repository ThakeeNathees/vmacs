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


Window::Type Window::GetType() const {
  return Type::OTHER;
}


void Window::Update() {
  _Update();
}


void Window::Draw(FrameBuffer& buff, Position pos, Area area) {
  this->pos = pos;
  this->area = area;
  _Draw(buff, pos, area);
}


bool Window::HandleEvent(const Event& event) {

  // FIXME: scroll and mouse clicks shold be handled properly and needs to be
  // sent to the correct windows.
  if (event.type == Event::Type::MOUSE && event.mouse.button != Event::MOUSE_RELEASED) {
    ASSERT(IsPointIncluded(Position(event.mouse.x, event.mouse.y)), OOPS);
  }

  // We'll true handle the event from the key bindings and if there isn't any
  // key is binded to the event we'll pass that to the child window.
  if (EventHandler::HandleEvent(event)) return true;
  return _HandleEvent(event);
}


void Window::SetActive(bool active) {
  OnFocusChanged(active);
}


bool Window::IsActive() const {
  ASSERT(split != nullptr, OOPS);
  ASSERT(split->tab != nullptr, OOPS);
  const Split* active_split = split->tab->GetActiveSplit();
  ASSERT(active_split != nullptr, OOPS);
  ASSERT(active_split->GetWindow() != nullptr, OOPS);
  return active_split->GetWindow() == this;
}


void Window::SetShouldClose() {
  should_close = true;
}


bool Window::IsShouldClose() const {
  return should_close;
}


const Position& Window::GetPosition() const {
  return pos;
}


const Area& Window::GetArea() const {
  return area;
}


bool Window::IsPointIncluded(const Position& point) const {
  if (point.x < pos.x || point.y < pos.y) return false;
  if (pos.x + area.width <= point.x) return false;
  if (pos.y + area.height <= point.y) return false;
  return true;
}


void Window::OnFocusChanged(bool focus) {}


// -----------------------------------------------------------------------------
// Split.
// -----------------------------------------------------------------------------


Split* Split::InsertChild(int index, std::unique_ptr<Split> child) {
  ASSERT(index >= 0 && index <= children.size(), OOPS);
  child->parent = this;
  child->tab = tab;
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
  ASSERT(window != nullptr, OOPS);
  ASSERT(type == Type::LEAF, OOPS);
  this->window = std::move(window);
  this->window->split = this;
}


Window* Split::GetWindow() {
  return window.get();
}


const Window* Split::GetWindow() const {
  return window.get();
}


Split* Split::GetRoot() {
  Split* curr = this;
  while (curr->parent) {
    curr = curr->parent;
  }
  return curr;
}


void Split::SetTab(Tab* tab) {
  this->tab = tab;
  for (std::unique_ptr<Split>& child: children) {
    child->SetTab(tab);
  }
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

  root->SetTab(this);
  this->active = active_;

  Window* window = this->active->GetWindow();
  ASSERT(window != nullptr, "A split with un-initialized window did you forget to set one?");
  window->SetActive(true);
}


std::string Tab::GetName() const {
  if (!active) return "";
  ASSERT(active->type == Split::Type::LEAF, OOPS);

  Window* window = active->GetWindow();
  ASSERT(window != nullptr, OOPS);
  if (window->GetType() == Window::Type::DOCUMENT) {
    DocumentWindow* docwin = (DocumentWindow*) window;
    std::shared_ptr<Document> document = docwin->GetDocument();
    if (document == nullptr) return "";
    return document->GetPath().FileName();
  }

  return ""; // Unknown window.
}


bool Tab::HandleEvent(const Event& event) {

  // If we're at the middle of listening key combination don't send the event
  // to the bellow layers.
  if (ListeningCombination()) {
    bool ret = EventHandler::HandleEvent(event);
    ASSERT(ret == true, OOPS);
    return ret;
  }

  #define return_true do { ResetCursor(); return true; } while (false)
  // Send the event to the inner most child to handle if it cannot we do
  // event bubbling.
  if (active != nullptr && active->window != nullptr) {
    if (active->window->HandleEvent(event)) return_true;
  }
  #undef return_true

  // No one consumed the event, we'll handle ourself.
  return EventHandler::HandleEvent(event);
}


void Tab::Update() {
  if (root == nullptr) return;
  for (auto it = root->Iterate(); it.Get() != nullptr; it.Next()) {
    Window* window = it.Get()->window.get();
    if (window == nullptr) continue;
    window->Update();
  }
}


const Split* Tab::GetActiveSplit() const {
  return active;
}


Window* Tab::GetWindowAt(const Position& pos) const {
  return SplitGetWindowAt(pos, root.get());
}


Window* Tab::SplitGetWindowAt(const Position& pos, Split* split) const {
  if (split == nullptr) return nullptr;

  if (split->type == Split::Type::LEAF) {
    ASSERT(split->children.empty(), OOPS);
    if (split->GetWindow()->IsPointIncluded(pos)) return split->GetWindow();
    return nullptr;
  }

  for (auto& child : split->children) {
    Window* window = SplitGetWindowAt(pos, child.get());
    if (window) return window;
  }

  return nullptr;
}


void Tab::Draw(FrameBuffer& buff, Position pos, Area area) {
  DrawSplit(buff, root.get(), pos, area);
}


void Tab::DrawSplit(FrameBuffer& buff, Split* split, Position pos, Area area) {
  if (split->children.size() == 0) { // Leaf node.
    ASSERT(split->type == Split::Type::LEAF, OOPS);
    if (split->window != nullptr) split->window->Draw(buff, pos, area);
    return;
  }

  ASSERT(split->type != Split::Type::LEAF, OOPS);

  // FIXME: Properly fetch the style from the editor or somewhere else.
  Style style = Editor::GetTheme().GetStyle("ui.background.separator");

  // The position to draw the children.
  Position curr = pos;

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
      DrawSplit(buff, child, curr, Area(width, area.height));
      if (!is_last) {
        DrawVerticalLine(buff, Position(curr.x+width, curr.y), area.height, style, Editor::GetIcons());
      }
      curr.x += width+1; // +1 for split line.

    } else if (split->type == Split::Type::HORIZONTAL) {
      int height = (area.height / child_count) + ((is_last) ? (area.height % child_count) : 0);
      if (!is_last) height--; // We'll use one row for drawing the split.
      DrawSplit(buff, child, curr, Area(area.width, height));
      if (!is_last) {
        DrawHorizontalLine(buff, Position(curr.x, curr.y + height), area.width, style, Editor::GetIcons());
      }
      curr.y += height+1; // +1 for split line.
    }
  }
}


bool Tab::Action_NextWindow(Tab* self) {
  ASSERT(self->active != nullptr, OOPS);
  auto it = Split::Iterator(self->active);
  ASSERT(it.Get() != nullptr, OOPS);
  it.Next(); // Increment the leaf by one.

  ASSERT(self->active->GetWindow() != nullptr, OOPS);
  self->active->GetWindow()->SetActive(false);
  if (it.Get() != nullptr) {
    self->active = it.Get();
  } else { // Reached the end of the tree.
    self->active = self->root->Iterate().Get();
  }
  ASSERT(self->active->GetWindow() != nullptr, OOPS);
  self->active->GetWindow()->SetActive(true);
  return true;
}


bool Tab::Action_Vsplit(Tab* self) {
  ASSERT(self->active != nullptr, OOPS);
  ASSERT(self->active->type == Split::Type::LEAF, OOPS);
  bool right = true; // TODO: Get split pos from config like vim.
  std::unique_ptr<Window> copy = self->active->GetWindow()->Copy();

  self->active->GetWindow()->SetActive(false);
  {
    self->active = self->active->Vsplit(right);
    self->active->SetWindow(std::move(copy));
  }
  self->active->GetWindow()->SetActive(true);
  return true;
}


bool Tab::Action_Hsplit(Tab* self) {
  ASSERT(self->active != nullptr, OOPS);
  ASSERT(self->active->type == Split::Type::LEAF, OOPS);
  bool bottom = true; // TODO: Get split pos from config like vim.
  std::unique_ptr<Window> copy = self->active->GetWindow()->Copy();

  self->active->GetWindow()->SetActive(false);
  {
    self->active = self->active->Hsplit(bottom);
    self->active->SetWindow(std::move(copy));
  }
  self->active->GetWindow()->SetActive(true);
  return true;
}


// -----------------------------------------------------------------------------
// Ui.
// -----------------------------------------------------------------------------


Ui::Ui() : EventHandler(&keytree) {
}


bool Ui::HandleEvent(const Event& event) {

  // TODO: Make Splits as event listeners and pass the mouse event to splits
  // to drag the split lines to adjust the size and refactor the bellow code.

  // If the event is mouse button pressed (or scrolled and not released), we
  // don't pass it to the active widnow, instead we'll send it to the window at
  // the position of the cursor.
  if (event.type == Event::Type::MOUSE && event.mouse.button != Event::MOUSE_RELEASED) {
    Window* window = GetWindowAt(Position(event.mouse.x, event.mouse.y));
    if (window) {
      // Scrolling shouldn't reset our key combination listening.
      bool is_scroll = event.mouse.button == Event::MOUSE_WHEEL_UP ||
                       event.mouse.button == Event::MOUSE_WHEEL_DOWN;

      if (!is_scroll) ResetCursor();
      return window->HandleEvent(event);
    }

    // Not passing the event to any window since no one has the point. And sending
    // will fail an assertion.
    return false;
  }

  // If we're at the middle of listening key combination don't send the event
  // to the bellow layers.
  if (ListeningCombination()) {
    bool ret = EventHandler::HandleEvent(event);
    ASSERT(ret == true, OOPS);
    return ret;
  }

  #define return_true do { ResetCursor(); return true; } while (false)
  // Note that if the popup is available we won't send the event to the active
  // child split nodes.
  if (popup && popup->HandleEvent(event)) {
    if (popup->IsShouldClose()) popup = nullptr; // This will destroy the popup.
    return_true;
  } else if (active_tab_index >= 0) {
    ASSERT_INDEX(active_tab_index, tabs.size());
    if (tabs[active_tab_index]->HandleEvent(event)) return_true;
  }
  #undef return_true

  // No one consumed the event.
  return EventHandler::HandleEvent(event);
}


void Ui::Update() {
  if (popup) popup->Update();
  for (auto& tab : tabs) tab->Update();
}


// FIXME(mess): Cleanup this mess.
void Ui::Draw(FrameBuffer& buff) {

  Position pos(0, 0);
  Area area(buff.width, buff.height-1);

  if (tabs.size()) {
    ASSERT_INDEX(active_tab_index, tabs.size());
    std::unique_ptr<Tab>& tab = tabs[active_tab_index];

    Position curr_pos = pos;
    Area curr_area    = area;
    if (tabs.size() >= 2) {
      DrawTabsBar(buff, pos, area);
      curr_pos.row     += 1;
      curr_area.height -= 1;
    }

    tab->Draw(buff, curr_pos, curr_area);

  } else {
    DrawHomeScreen(buff, pos, area);
  }

  DrawPromptBar(buff);

  if (popup) {
    popup->Draw(buff, pos, area);
  }
}


void Ui::DrawHomeScreen(FrameBuffer& buff, Position pos, Area area) {

  const Icons& icons = Editor::GetIcons();
  const Theme& theme = Editor::GetTheme();

  // FIXME: Move this to somewhere else.
  // --------------------------------------------------------------------------
  // TODO: Use ui.cursor for secondary cursor same as selection.
  Style style_text       = theme.GetStyle("ui.text");
  Style style_bg         = theme.GetStyle("ui.background");
  Style style_whitespace = theme.GetStyle("ui.virtual.whitespace");
  Style style_cursor     = theme.GetStyle("ui.cursor.primary");
  Style style_selection  = theme.GetStyle("ui.selection.primary");
  Style style_error      = theme.GetStyle("error");
  Style style_warning    = theme.GetStyle("warning");

  Style style = style_bg.Apply(style_text);
  Style style_copyright = style_bg.Apply(style_whitespace);
  // --------------------------------------------------------------------------

  // FIXME: This is temproary.
  // --------------------------------------------------------------------------
  std::vector<std::pair<std::string, std::string>> items;
  items.push_back({ Utf8UnicodeToString(icons.empty_file)   + " New file",       "<C-n>" });
  items.push_back({ Utf8UnicodeToString(icons.find)         + " Find Files",     "<C-o>" });
  items.push_back({ Utf8UnicodeToString(icons.textbox)      + " Recent Files",   "<C-r>" });
  items.push_back({ Utf8UnicodeToString(icons.find_in_file) + " Live Grep",      "<C-g>" });
  items.push_back({ Utf8UnicodeToString(icons.palette)      + " Themes",         "<C-t>" });
  // --------------------------------------------------------------------------


  // Get the maximum length of the display text so we need the padding length.
  size_t max_len_text = 0, max_len_bind = 0;
  for (auto& pair : items) {
    max_len_text = MAX(Utf8Strlen(pair.first.c_str()), max_len_text);
    max_len_bind = MAX(Utf8Strlen(pair.second.c_str()), max_len_bind);
  }

  // FIXME: 10 is hardcoded here.
  int spacing = MIN(10, area.width - (max_len_text+max_len_bind)); // Number of column spacing between text and the binding.
  int display_width = max_len_text + spacing + max_len_bind; // for margin.
  int display_height = items.size() * 2 - 1; // *2 space between two items, -1 remove last space.

  Position curr; // Current drawing position.
  curr.col = pos.col + (area.width - display_width) / 2;
  curr.row = pos.row + (area.height - display_height) / 2;
  for (auto& pair : items) {
    size_t len_text = Utf8Strlen(pair.first.c_str());
    size_t len_bind = Utf8Strlen(pair.second.c_str());
    DrawTextLine(buff, pair.first.c_str(), curr, len_text, style, icons, false);
    Position pos_binding = Position(curr.x+max_len_text+spacing, curr.y);
    DrawTextLine(buff, pair.second.c_str(), pos_binding, len_bind, style, icons, false);
    curr.row += 2;
  }

  // FIXME: Store this and the version somewhere global.
  std::string copyright = "Copyright (c) 2024 Thakee Nathees";
  curr.row = pos.row + area.height - 1;
  curr.col = pos.col + (area.width - copyright.size()) / 2;
  DrawTextLine(buff, copyright.c_str(), curr, copyright.size(), style_copyright, icons, false);
}


void Ui::DrawPromptBar(FrameBuffer& buff) {

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  // FIXME: --------------------------------------------------------------------
  Style style_text = theme.GetStyle("ui.text");
  Style style_bg   = theme.GetStyle("ui.background");
  Style style      = style_bg.Apply(style_text);
  // ---------------------------------------------------------------------------

  DrawTextLine(
      buff, info_bar_text.c_str(),
      Position(0, buff.height-1),
      buff.width-1, // -1 for spinning wheel.
      style, icons,
      true);

  const int wheel_count = sizeof icons.brail_spinning_wheel / sizeof * icons.brail_spinning_wheel;
  static int wheel_icon_index = 0;
  int wheel_icon = icons.brail_spinning_wheel[wheel_icon_index++];
  if (wheel_icon_index >= wheel_count) wheel_icon_index = 0;

  // Draw a spinning wheel which will spin every time we re-draw.
  SET_CELL(buff, buff.width-1, buff.height-1, wheel_icon, style);
}


// TODO: Add multiple tabs and scroll and ensure active tab is in view.
void Ui::DrawTabsBar(FrameBuffer& buff, Position pos, Area area) {
  ASSERT(tabs.size() > 0, OOPS);

  const Theme& theme     = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  // FIXME: Move this.
  // --------------------------------------------------------------------------
  Style style_text       = theme.GetStyle("ui.text");
  Style style_bg         = theme.GetStyle("ui.background");
  Style style_whitespace = theme.GetStyle("ui.virtual.whitespace");
  Style style_cursor     = theme.GetStyle("ui.cursor.primary");
  Style style_selection  = theme.GetStyle("ui.selection.primary");
  Style style_error      = theme.GetStyle("error");
  Style style_warning    = theme.GetStyle("warning");
  Style style_menu       = theme.GetStyle("ui.menu");
  Style style_menu_sel   = theme.GetStyle("ui.menu.selected");

  Style style_not_active = style_menu;
  Style style_active     = style_menu_sel.Apply(style_bg);
  Style style_split      = style_not_active;
  style_split.fg         = style_active.bg;
  // --------------------------------------------------------------------------

  Position curr = pos;
  DrawRectangleFill(buff, curr, Area(area.width, 1), style_not_active);

  for (int i = 0; i < tabs.size(); i++) {
    auto& tab = tabs[i];

    std::string tab_name = tab->GetName();
    if (tab_name.empty()) {
      tab_name = "tab " + std::to_string(i+1);
    }

    // TODO: If current file modified we put an indicator (+).
    // This will be the displaied text of the tab bar including the padding.
    std::string tab_bar_display = std::string(" ") + tab_name + " ";

    Style& style = (i == active_tab_index) ? style_active : style_not_active;
    DrawTextLine(buff, tab_bar_display.c_str(), curr, tab_bar_display.size(), style, icons, true);
    curr.col += tab_bar_display.size();

    if (i == active_tab_index || (i+1) == active_tab_index) {
      SET_CELL(buff, curr.col, curr.row, ' ', style_active);
    } else {
      DrawVerticalLine(buff, curr, 1, style_split, icons);
    }

    curr.col++;  // +1 for vertical split.
  }
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
  active_tab_index = tabs.size();
  tabs.push_back(std::move(tab));
}


Window* Ui::GetWindowAt(Position pos) const {
  if (popup && popup.get()->IsPointIncluded(pos)) {
    return popup.get();
  }

  // It's possible for the active tab to be null, if we're at the wellcome screen.
  if (active_tab_index < 0 || active_tab_index >= tabs.size()) return nullptr;
  Tab* tab = tabs[active_tab_index].get();
  ASSERT(tab != nullptr, OOPS);
  return tab->GetWindowAt(pos);
}


bool Ui::Action_PopupFilesFinder(EventHandler* eh) {
  Ui* self = (Ui*) eh;
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindWindow>(std::make_unique<FilesFinder>());
  return true;
}


bool Ui::Action_PopupLiveGrep(EventHandler* eh) {
  Ui* self = (Ui*) eh;
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindWindow>(std::make_unique<LiveGrep>());
  return true;
}


bool Ui::Action_NewDocument(EventHandler* eh) {
  Ui* self = (Ui*) eh;
  // FIXME: Do I need to register the document at the editor registry, In that
  // case what path should i use?
  std::shared_ptr<Document> document = std::make_shared<Document>();
  std::unique_ptr<DocumentWindow> docwindow = std::make_unique<DocumentWindow>(document);
  std::unique_ptr<Split> root = std::make_unique<Split>();
  root->SetWindow(std::move(docwindow));
  std::unique_ptr<Tab> tab = std::make_unique<Tab>(std::move(root));
  self->AddTab(std::move(tab));
  return true;
}


bool Ui::Action_TabNext(EventHandler* eh) {
  Ui* self = (Ui*) eh;
  if (self->tabs.size() == 0) return false;
  self->active_tab_index++;
  self->active_tab_index %= self->tabs.size();
  return true;
}


bool Ui::Action_TabPrev(EventHandler* eh) {
  Ui* self = (Ui*) eh;
  if (self->tabs.size() == 0) return false;
  if (self->active_tab_index == 0) {
    self->active_tab_index = self->tabs.size() - 1;
  } else {
    self->active_tab_index--;
  }
  return true;
}
