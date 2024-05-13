//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "ui.hpp"


// -----------------------------------------------------------------------------
// Window.
// -----------------------------------------------------------------------------


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

  return _HandleEvent(event);
}


void Window::SetActive(bool active) {
  OnFocusChanged(active);
}


bool Window::IsActive() const {
  Ui* ui = (Ui*)Editor::Singleton()->GetUi();
  ASSERT(ui != nullptr, OOPS);
  return ui->GetActiveWindow() == this;
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


Split* Window::GetSplit() const {
  return split;
}


void Window::SetSplit(Split* split) {
  this->split = split;
}


std::unique_ptr<Window> Window::Copy() const {
  return nullptr;
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
    if (right) l->SetWindow(std::move(window));
    else r->SetWindow(std::move(window));
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
    if (bottom) t->SetWindow(std::move(window));
    else b->SetWindow(std::move(window));
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
  this->window->SetSplit(this);
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


Split::Type Split::GetType() const {
  return type;
}


Tab* Split::GetTab() const {
  return tab;
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


Window* Split::GetWindowAt(const Position& pos) {
  if (type == Split::Type::LEAF) {
    ASSERT(children.empty(), OOPS);
    if (GetWindow()->IsPointIncluded(pos)) return GetWindow();
    return nullptr;
  }

  for (auto& child : children) {
    Window* window = child->GetWindowAt(pos);
    if (window) return window;
  }

  return nullptr;
}


DocumentWindow* Split::GetDocumentWindow(const Path& path) {
  if (children.size() == 0) { // Leaf node.
    ASSERT(type == Split::Type::LEAF, OOPS);
    if (window != nullptr && window->GetType() == Window::Type::DOCUMENT) {
      DocumentWindow* docwin = (DocumentWindow*) window.get();
      if (docwin->GetDocument()->GetPath() == path) return docwin;
    }
    return nullptr;
  }

  ASSERT(type != Split::Type::LEAF, OOPS);

  for (auto& child : children) {
    DocumentWindow* docwin = child->GetDocumentWindow(path);
    if (docwin) return docwin;
  }

  return nullptr;
}


void Split::Draw(FrameBuffer& buff, Position pos, Area area) {
  if (children.size() == 0) { // Leaf node.
    ASSERT(type == Split::Type::LEAF, OOPS);
    if (window != nullptr) window->Draw(buff, pos, area);
    return;
  }

  ASSERT(type != Split::Type::LEAF, OOPS);

  // FIXME: Properly fetch the style from the editor or somewhere else.
  Style style = Editor::GetTheme().GetStyle("ui.background.separator");

  // The position to draw the children.
  Position curr = pos;

  int child_count = children.size();

  for (int i = 0; i < child_count; i++) {
    Split* child = children[i].get();

    // If it's the last child will take the rest of the space. Not sure if this
    // is correct.
    bool is_last = i == child_count - 1;

    if (type == Split::Type::VERTICAL) {
      // The total width including a split line at the end.
      int width = (area.width / child_count) + ((is_last) ? (area.width % child_count) : 0);
      if (!is_last) width--;
      child->Draw(buff, curr, Area(width, area.height));
      if (!is_last) {
        DrawVerticalLine(buff, Position(curr.x+width, curr.y), area.height, style, Editor::GetIcons());
      }
      curr.x += width+1; // +1 for split line.

    } else if (type == Split::Type::HORIZONTAL) {
      int height = (area.height / child_count) + ((is_last) ? (area.height % child_count) : 0);
      if (!is_last) height--; // We'll use one row for drawing the split.
      child->Draw(buff, curr, Area(area.width, height));
      if (!is_last) {
        DrawHorizontalLine(buff, Position(curr.x, curr.y + height), area.width, style, Editor::GetIcons());
      }
      curr.y += height+1; // +1 for split line.
    }
  }
}


// -----------------------------------------------------------------------------
// Tab.
// -----------------------------------------------------------------------------


Tab::Tab(std::unique_ptr<Split> root_) : root(std::move(root_)) {

  // TODO: Validate the root and all of it's childs are met our invariant.
  ASSERT(root != nullptr, "Root split was nullptr.");

  // Set the first leaf node as the active split.
  active = root->Iterate().Get();
  ASSERT(active != nullptr, OOPS);
  ASSERT(active->GetType() == Split::Type::LEAF, OOPS);
  ASSERT(active->GetRoot() == root.get(), "Given active spit should be part of the given split tree.");

  root->SetTab(this);

  Window* window = this->active->GetWindow();
  ASSERT(window != nullptr, "A split with un-initialized window did you forget to set one?");
  window->SetActive(true);
}


std::unique_ptr<Tab> Tab::FromWindow(std::unique_ptr<Window> window) {
  std::unique_ptr<Split> root = std::make_unique<Split>();
  root->SetWindow(std::move(window));
  return std::make_unique<Tab>(std::move(root));
}


std::string Tab::GetName() const {
  if (!active) return "";
  ASSERT(active->GetType() == Split::Type::LEAF, OOPS);

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
  if (active != nullptr && active->GetWindow() != nullptr) {
    return active->GetWindow()->HandleEvent(event);
  }
  return false;
}


void Tab::Update() {
  if (root == nullptr) return;
  for (auto it = root->Iterate(); it.Get() != nullptr; it.Next()) {
    Window* window = it.Get()->GetWindow();
    if (window == nullptr) continue;
    window->Update();
  }
}


Split* Tab::GetRoot() const {
  return root.get();
}


Tabs* Tab::GetTabs() const {
  return tabs;
}


void Tab::SetTabs(Tabs* tabs) {
  this->tabs = tabs;
}


Split* Tab::GetActive() const {
  return active;
}


void Tab::SetActive(Split* split) {
  ASSERT(split != nullptr, OOPS);
  ASSERT(split->GetTab() == this, OOPS);
  active = split;
}


void Tab::Draw(FrameBuffer& buff, Position pos, Area area) {
  ASSERT(root != nullptr, OOPS);
  root->Draw(buff, pos, area);
}


bool Tab::NextWindow() {
  ASSERT(active != nullptr, OOPS);
  auto it = Split::Iterator(active);
  ASSERT(it.Get() != nullptr, OOPS);
  it.Next(); // Increment the leaf by one.

  ASSERT(active->GetWindow() != nullptr, OOPS);
  active->GetWindow()->SetActive(false);
  if (it.Get() != nullptr) {
    active = it.Get();
  } else { // Reached the end of the tree.
    active = root->Iterate().Get();
  }
  ASSERT(active->GetWindow() != nullptr, OOPS);
  active->GetWindow()->SetActive(true);
  return true;
}


bool Tab::Vsplit() {
  ASSERT(active != nullptr, OOPS);
  ASSERT(active->GetType() == Split::Type::LEAF, OOPS);
  bool right = true; // TODO: Get split pos from config like vim.
  std::unique_ptr<Window> copy = active->GetWindow()->Copy();

  // FIXME: Show an error to the ui, that the window cannot be splitted.
  if (copy == nullptr) return false;

  active->GetWindow()->SetActive(false);
  {
    active = active->Vsplit(right);
    active->SetWindow(std::move(copy));
  }
  active->GetWindow()->SetActive(true);
  return true;
}


bool Tab::Hsplit() {
  ASSERT(active != nullptr, OOPS);
  ASSERT(active->GetType() == Split::Type::LEAF, OOPS);
  bool bottom = true; // TODO: Get split pos from config like vim.
  std::unique_ptr<Window> copy = active->GetWindow()->Copy();
  if (copy == nullptr) return false;

  active->GetWindow()->SetActive(false);
  {
    active = active->Hsplit(bottom);
    active->SetWindow(std::move(copy));
  }
  active->GetWindow()->SetActive(true);
  return true;
}


// -----------------------------------------------------------------------------
// Tabs.
// -----------------------------------------------------------------------------


void Tabs::AddTab(std::unique_ptr<Tab> tab) {
  tab->SetTabs(this);
  active_tab_index = tabs.size();
  tabs.push_back(std::move(tab));
}


Tab* Tabs::GetActive() const {
  if (active_tab_index >= 0 && active_tab_index < tabs.size()) {
    return tabs[active_tab_index].get();
  }
  ASSERT(active_tab_index < 0, OOPS);
  return nullptr;
}


void Tabs::SetActive(int index) {
  ASSERT_INDEX(index, tabs.size());
  active_tab_index = index;
}


int Tabs::Count() const {
  return tabs.size();
}


Tab* Tabs::Child(int index) const {
  if (0 <= index && index < tabs.size()) {
    return tabs[index].get();
  }
  return nullptr;
}


Window* Tabs::GetWindowAt(Position pos) const {
  // It's possible for the active tab to be null, if we're at the wellcome screen.
  const Tab* active = GetActive();
  if (active == nullptr) return nullptr;
  Split* root = active->GetRoot();
  ASSERT(root != nullptr, OOPS);
  return root->GetWindowAt(pos);
}


bool Tabs::HandleEvent(const Event& event) {
  Tab* active = GetActive();
  if (active == nullptr) return false; // Empty tabs.
  return active->HandleEvent(event);
}


void Tabs::Update() {
  for (auto& tab : tabs) tab->Update();
}


void Tabs::Draw(FrameBuffer& buff, Position pos, Area area) {
  Tab* active = GetActive();
  if (active == nullptr) return; // Empty tabs.
  if (tabs.size() >= 2) {
    DrawTabsBar(buff, pos, area);
    pos.row++; area.height--; // Adjust the size after drawing the tab bar.
  }
  active->Draw(buff, pos, area);
}


// TODO: Add multiple tabs and scroll and ensure active tab is in view.
void Tabs::DrawTabsBar(FrameBuffer& buff, Position pos, Area area) {
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

  Style style_not_active = style_menu.Apply(style_whitespace);
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


bool Tabs::Action_TabNext(Tabs* self) {
  if (self->tabs.size() == 0) return false;
  self->active_tab_index++;
  self->active_tab_index %= self->tabs.size();
  return true;
}


bool Tabs::Action_TabPrev(Tabs* self) {
  if (self->tabs.size() == 0) return false;
  if (self->active_tab_index == 0) {
    self->active_tab_index = self->tabs.size() - 1;
  } else {
    self->active_tab_index--;
  }
  return true;
}


// -----------------------------------------------------------------------------
// Ui.
// -----------------------------------------------------------------------------


KeyTree Ui::keytree; // Static memeber definition.

Ui::Ui() : cursor(&Ui::keytree) {}


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

      if (!is_scroll) cursor.ResetCursor();
      return window->HandleEvent(event);
    }

    // Not passing the event to any window since no one has the point. And sending
    // will fail an assertion.
    return false;
  }

  // Try to consume the event for the active window (which could also be the popup)
  // and if it cannot consumed, we try to consume the event to the ui as long as
  // we don't have a popup.
  Window* window = GetActiveWindow();
  bool consumed = cursor.ConsumeEvent(window, event) ||
                  (!popup && cursor.ConsumeEvent(this, event));

  if (!consumed) {
    bool listening = !cursor.IsCursorRoot();
    cursor.ResetCursor();

    // Sent the event to the child windows to manually handle without bindnigs.
    if (!listening) {
      if (popup) {
        if (popup->HandleEvent(event)) {
          if (popup->IsShouldClose()) popup = nullptr;
          return true;
        }
      } else if (active) {
        if (active->HandleEvent(event)) return true;
      }
    }

    // If it was listening, we reset the cursor and accept the event as handled.
    return listening;
  }

  #define return_true do { cursor.ResetCursor(); return true; } while (false)
  if (window && cursor.TryEvent(window)) { // Window could also be the popup.
    if (popup && popup->IsShouldClose()) popup = nullptr;
    return_true;
  }
  // If a popup is available we don't pass the event to the ui.
  if (!popup && cursor.TryEvent(this)) return_true;
  #undef return_true

  if (cursor.HasMore()) return true;

  if (!cursor.IsCursorRoot()) {
    cursor.ResetCursor();
    return true;
  }

  return false;
}


void Ui::Update() {
  left.Update();
  right.Update();
  documents.Update();
  if (popup) popup->Update();
}


void Ui::Draw(FrameBuffer& buff) {

  Position pos(0, 0);
  Area area(buff.width, buff.height-1);

  if (left.Count()) {

    const int percent = 25; // FIXME: The size is hardcoded.
    const int w = (area.width * percent) / 100;
    left.Draw(buff, pos, Area(w-1, area.height)); // -1 for vertical split.

    const Theme& theme = Editor::GetTheme();

    // FIXME: Properly fetch the style from the editor or somewhere else.
    Style style_sep = theme.GetStyle("ui.text")
      .Apply(theme.GetStyle("ui.background"))
      .Apply(theme.GetStyle("ui.background.separator"));
    DrawVerticalLine(buff, Position(pos.x + w-1, pos.y), area.height, style_sep, Editor::GetIcons());

    pos.x += w;
    area.width -= w;
  }

  if (documents.Count()) {
    documents.Draw(buff, pos, area);
  } else {
    DrawHomeScreen(buff, pos, area);
  }

  DrawPromptBar(buff);

  // FIXME: The size is "hardcoded".
  if (popup) {
    const int percent = 70;
    const int W = buff.width;
    const int H = buff.height;
    const int w = (W * percent) / 100;
    const int h = (H * percent) / 100;
    const int x = (W - w) / 2;
    const int y = (H - h) / 2;
    popup->Draw(buff, Position(x, y), Area(w, h));
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


void Ui::AddTab(std::unique_ptr<Tab> tab, int container) {
  ASSERT(-1 <= container && container <= 1, "Invalid container id.");
  switch (container) {
    case -1: left.AddTab(std::move(tab)); return;
    case  0: documents.AddTab(std::move(tab)); return;
    case +1: right.AddTab(std::move(tab)); return;
  }
}


bool Ui::JumpToDocument(const Path& path, Coord coord) {

  // TODO: Error to editor.
  if (!path.Exists()) return false;

  // Check if a window opened for the given path, in that case just make it
  // active.
  {
    DocumentWindow* docwin = GetDocumentWindow(path);
    if (docwin) {
      SetWindowActive(docwin);
      docwin->JumpTo(coord);
      return true;
    }
  }

  // TODO: If we're in a split, don't create a new tab. Set the split's window
  // as the document.

  // Note that if opening the file failed, an error message was already set
  // by the OpenDocument method.
  Editor* e = Editor::Singleton().get();
  std::shared_ptr<Document> doc = e->OpenDocument(path);
  if (doc == nullptr) return false;

  // TODO: Implement tab from window and call it here.
  std::unique_ptr<DocumentWindow> docwin = std::make_unique<DocumentWindow>(doc);
  DocumentWindow* ptr_docwin = docwin.get();  // Needed to set the view location.
  std::unique_ptr<Tab> tab = Tab::FromWindow(std::move(docwin));
  AddTab(std::move(tab));

  // We call this here since otherwise the pos/area of the window didn't updated.
  // which is needed to set the view of the coordinate.
  SetWindowActive(ptr_docwin);
  ptr_docwin->JumpTo(coord);

  return true;
}


Window* Ui::GetWindowAt(Position pos) const {

  if (popup && popup.get()->IsPointIncluded(pos)) {
    return popup.get();
  }

  Window* window = nullptr;
  if (!window) window = left.GetWindowAt(pos);
  if (!window) window = right.GetWindowAt(pos);
  if (!window) window = documents.GetWindowAt(pos);
  return window;
}


Window* Ui::GetActiveWindow() const {
  // active_tabs -> active_tab -> active_split -> window.
  if (active == nullptr) return nullptr; // No tabs is active.

  // If a popup is available, it's the active window we disregard everything else.
  if (popup != nullptr) return popup.get();

  const Tab* tab = active->GetActive();
  if (tab == nullptr) return nullptr; // Empty tabs container.

  ASSERT(tab->GetActive(), OOPS);
  ASSERT(tab->GetActive()->GetWindow(), OOPS);
  return tab->GetActive()->GetWindow();
}


void Ui::SetWindowActive(Window* window) {
  ASSERT(window != nullptr, OOPS);
  ASSERT(window->GetSplit() != nullptr, OOPS);
  ASSERT(window->GetSplit()->GetTab() != nullptr, OOPS);
  ASSERT(window->GetSplit()->GetTab()->GetTabs() != nullptr, OOPS);

  Tabs* next_active = window->GetSplit()->GetTab()->GetTabs();
  for (int i = 0; i < next_active->Count(); i++) {
    Tab* child = next_active->Child(i);
    if (child == window->GetSplit()->GetTab()) {
      next_active->SetActive(i);
      child->SetActive(window->GetSplit());
      window->SetActive(true);
      break;
    }
  }
  active = next_active;
}


DocumentWindow* Ui::GetDocumentWindow(const Path& path) const {
  for (int i = 0; i < documents.Count(); i++) {
    Split* root = documents.Child(i)->GetRoot();
    DocumentWindow* window = root->GetDocumentWindow(path);
    if (window) return window;
  }
  return nullptr;
}


bool Ui::Action_PopupFilesFinder(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindWindow>(std::make_unique<FilesFinder>());
  return true;
}


bool Ui::Action_PopupLiveGrep(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindWindow>(std::make_unique<LiveGrep>());
  return true;
}


bool Ui::Action_NewDocument(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  // FIXME: Do I need to register the document at the editor registry, In that
  // case what path should i use?
  std::shared_ptr<Document> document = std::make_shared<Document>();
  std::unique_ptr<DocumentWindow> docwindow = std::make_unique<DocumentWindow>(document);

  std::unique_ptr<Tab> tab = Tab::FromWindow(std::move(docwindow));
  self->AddTab(std::move(tab));
  return true;
}


bool Ui::Action_TabNext(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (!self->active) return false;
  return self->active->Action_TabNext(self->active);
}


bool Ui::Action_TabPrev(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (!self->active) return false;
  return self->active->Action_TabPrev(self->active);
}


bool Ui::Action_NextWindow(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (!self->active) return false;
  Tab* tab = self->active->GetActive();
  if (!tab) return false;
  return tab->NextWindow();
}


bool Ui::Action_Vsplit(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (!self->active) return false;
  Tab* tab = self->active->GetActive();
  if (!tab) return false;
  return tab->Vsplit();
}


bool Ui::Action_Hsplit(ActionExecutor* ae) {
  Ui* self = (Ui*) ae;
  if (!self->active) return false;
  Tab* tab = self->active->GetActive();
  if (!tab) return false;
  return tab->Hsplit();
}
