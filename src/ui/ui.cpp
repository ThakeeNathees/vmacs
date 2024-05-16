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
  Ui* ui = GETUI();
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


int Split::GetChildCount() const {
  return children.size();
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


void Split::RemoveChild(int index) {
  ASSERT(index >= 0 && index < children.size(), OOPS);
  ASSERT(children.size() >= 2, "A split should contains at least 2 child or leaf node.");
  children.erase(children.begin() + index);
  if (children.size() > 1) return;

  // If we reached here we have only one child, in that case we "take ownership"
  // of that child.
  Split* child = children[0].get();

  Ui* ui = GETUI(); ASSERT(ui != nullptr, OOPS);
  Tab* tab = ui->GetSplitTab(this); ASSERT(tab, OOPS);

  this->type = child->type;
  if (tab->GetActiveSplit() == child) {
    tab->SetActiveSplit(this);
  }

  if (child->type == Split::Type::LEAF) {
    ASSERT(child->children.size() == 0, OOPS);
    this->SetWindow(std::move(child->window));
    this->children.clear();

  } else {
    ASSERT(child->children.size() >= 2, OOPS);

    // We need to store the child splits somewhere before removing the child.
    std::vector<std::unique_ptr<Split>> temp = std::move(child->children);

    // We shouldn't have a vsplit has a child which is also a vsplit and vice
    // versa. In that case we merge this with parent.
    if (parent && parent->type == child->type) {

      if (tab->GetActiveSplit() == this) tab->SetActiveSplit(parent);

      for (auto& child : temp) {
        child->parent = this->parent;
        parent->children.push_back(std::move(child));
      }

      // WARNING !! This will remove itself and this pointer will become invalid
      // after the bellow call, don't use.
      int index = GetIndexInParent();
      parent->children.erase(parent->children.begin() + index);
      return; // Return immediately.

    } else {
      for (auto& child : temp) child->parent = this;
      children = std::move(temp);
    }

  }
}


void Split::SetWindow(std::unique_ptr<Window> window) {
  ASSERT(window != nullptr, OOPS);
  ASSERT(type == Type::LEAF, OOPS);
  this->window = std::move(window);
}


Window* Split::GetWindow() {
  return window.get();
}


const Window* Split::GetWindow() const {
  return window.get();
}


Split* Split::GetParent() const {
  return parent;
}


Split* Split::GetRoot() {
  Split* curr = this;
  while (curr) {
    if (curr->parent == nullptr) return curr;
    curr = curr->parent;
  }
  UNREACHABLE();
  return nullptr;
}


const Split* Split::GetRoot() const {
  const Split* curr = this;
  while (curr) {
    if (curr->parent == nullptr) return curr;
    curr = curr->parent;
  }
  UNREACHABLE();
  return nullptr;
}



Split::Type Split::GetType() const {
  return type;
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
    ASSERT(curr->children.size() >= 2, "Invalid split configuration, non leaf"
                                       "split should contains child at least 2 child splits.");
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


void Split::Draw(FrameBuffer& buff, Position pos, Area area) {
  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  if (children.size() == 0) { // Leaf node.
    ASSERT(type == Split::Type::LEAF, OOPS);
    if (window != nullptr) window->Draw(buff, pos, area);
    return;
  }

  ASSERT(type != Split::Type::LEAF, OOPS);

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
        DrawVerticalLine(buff, Position(curr.x+width, curr.y), area.height, theme.lines, icons);
      }
      curr.x += width+1; // +1 for split line.

    } else if (type == Split::Type::HORIZONTAL) {
      int height = (area.height / child_count) + ((is_last) ? (area.height % child_count) : 0);
      if (!is_last) height--; // We'll use one row for drawing the split.
      child->Draw(buff, curr, Area(area.width, height));
      if (!is_last) {
        DrawHorizontalLine(buff, Position(curr.x, curr.y + height), area.width, theme.lines, icons);
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
    DocumentWindow* docwin = static_cast<DocumentWindow*>(window);
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


Split* Tab::GetActiveSplit() const {
  return active;
}


void Tab::SetActiveSplit(Split* split) {
  ASSERT(split != nullptr, OOPS);
  ASSERT(split->GetRoot() == GetRoot(), OOPS);
  ASSERT(split->GetType() == Split::Type::LEAF, OOPS);
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
// Ui.
// -----------------------------------------------------------------------------


KeyTree Ui::keytree; // Static memeber definition.

Ui::Ui() : cursor(&Ui::keytree) {
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
      } else if (active_tab_index >= 0) {
        Tab* active = GetActiveTab();
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
  Tab* active = GetActiveTab();
  if (active) active->Update();
  if (popup) popup->Update();
}


void Ui::Draw(FrameBuffer& buff) {

  Position pos(0, 0);
  Area area(buff.width, buff.height-1);

  Tab* active = GetActiveTab();
  ASSERT(tabs.empty() || active != nullptr, OOPS);

  if (active) {
    if (tabs.size() >= 2) {
      DrawTabsBar(buff, pos, area);
      pos.row++; area.height--; // Adjust the size after drawing the tab bar.
    }
    active->Draw(buff, pos, area);
  } else {
    DrawHomeScreen(buff, pos, area);
  }

  // FIXME: autocompletion will override the info bar. Before chaing the order
  // we need to somhow set the area of the view to buff.height-1 to the autocompletion
  // draw method so it won't draw anything on the info bar and we have a hidden
  // selection.
  DrawInfoBar(buff);
  DrawOverlays(buff);

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


void Ui::DrawOverlays(FrameBuffer& buff) {
  while (!overlays.empty()) {
    Position pos = overlays.front().first;
    FrameBuffer& overlay = overlays.front().second;

    // Adjust the width and height of the overlay according to the framebuffer.
    int width  = MIN(overlay.width, buff.width - pos.x);
    int height = MIN(overlay.height, buff.height - pos.y);

    // I'm sure there is a better way to do this like memcpy, but not now.
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        Cell& cell = BUFF_CELL(overlay, x, y);
        BUFF_CELL(buff, (pos.x+x), (pos.y+y)) = cell;
      }
    }

    overlays.pop();
  }
}


// TODO: Add tabs scroll and ensure active tab is in view.
void Ui::DrawTabsBar(FrameBuffer& buff, Position pos, Area area) {
  ASSERT(tabs.size() > 0, OOPS);

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  Position curr = pos;
  DrawRectangleFill(buff, curr, Area(area.width, 1), theme.tabbar);

  for (int i = 0; i < tabs.size(); i++) {
    auto& tab = tabs[i];

    // FIXME: This should be reviewed.
    std::string tab_name = tab->GetName();
    if (tab_name.empty()) {
      tab_name = "tab " + std::to_string(i+1);
    }

    // TODO: If current file modified we put an indicator (+).
    // This will be the displaied text of the tab bar including the padding.
    std::string tab_bar_display = std::string(" ") + tab_name + " ";

    // Draw the tab name.
    const Style& style = (i == active_tab_index) ? theme.tabbar_active : theme.tabbar;
    DrawTextLine(buff, tab_bar_display.c_str(), curr, tab_bar_display.size(), style, icons, true);
    curr.col += tab_bar_display.size();

    if (i == active_tab_index || (i+1) == active_tab_index) {
      SET_CELL(buff, curr.col, curr.row, ' ', theme.tabbar_active);
    } else {
      Style style_split_bar = theme.tabbar; // This will blend with the editor.
      style_split_bar.fg = theme.tabbar_active.bg;
      DrawVerticalLine(buff, curr, 1, style_split_bar, icons);
    }

    curr.col++;  // +1 for vertical split.
  }
}


void Ui::DrawHomeScreen(FrameBuffer& buff, Position pos, Area area) {

  const Icons& icons = Editor::GetIcons();
  const Theme& theme = Editor::GetTheme();

  // FIXME: This is temproary.
  // --------------------------------------------------------------------------
  std::vector<std::pair<std::string, std::string>> items;
  items.push_back({ Utf8UnicodeToString(icons.empty_file)   + " New File",       "<C-n>" });
  items.push_back({ Utf8UnicodeToString(icons.find)         + " Find Files",     "<C-o>" });
  items.push_back({ Utf8UnicodeToString(icons.filetree)     + " File Tree",      "<C-f>" });
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
    DrawTextLine(buff, pair.first.c_str(), curr, len_text, theme.style, icons, false);
    Position pos_binding = Position(curr.x+max_len_text+spacing, curr.y);
    DrawTextLine(buff, pair.second.c_str(), pos_binding, len_bind, theme.style, icons, false);
    curr.row += 2;
  }

  // FIXME: Store this and the version somewhere global.
  std::string copyright = "Copyright (c) 2024 Thakee Nathees";
  curr.row = pos.row + area.height - 2;
  curr.col = pos.col + (area.width - copyright.size()) / 2;
  DrawTextLine(buff, copyright.c_str(), curr, copyright.size(), theme.whitespace, icons, false);
}


// FIXME: The logic of this method is complete garbage.
void Ui::DrawInfoBar(FrameBuffer& buff) {

  const Theme& theme = Editor::GetTheme();
  const Icons& icons = Editor::GetIcons();

  // Current drawing position.
  Position curr(0, buff.height-1);

  DrawRectangleFill(buff, curr, Area(buff.width, 1), theme.statusline);

  // FIXME:
  std::string mode = "NORMAL";

  Window* win = GetActiveWindow(true);
  if (win != nullptr) {
    mode = win->GetMode();
  }
  mode = mode.empty() ? std::string("NORMAL") :  ToUpper(mode);
  mode = " " + mode + " ";

  DrawTextLine(
    buff, mode.c_str(),
    curr,
    mode.size(),
    theme.statusline, icons,
    true);
  curr.x += mode.size();

  // Draw the spenning wheel for draw indicator.
  Position spinpos = Position(buff.width-2, buff.height - 1);
  {
    const int wheel_count = sizeof icons.brail_spinning_wheel / sizeof * icons.brail_spinning_wheel;
    static int wheel_icon_index = 0;
    int wheel_icon = icons.brail_spinning_wheel[wheel_icon_index++];
    if (wheel_icon_index >= wheel_count) wheel_icon_index = 0;

    // Draw a spinning wheel which will spin every time we re-draw.
    SET_CELL(buff, spinpos.x, spinpos.y, wheel_icon, theme.statusline);
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


void Ui::PushOverlay(Position pos, FrameBuffer&& buff) {
  overlays.push(std::make_pair(pos, buff));
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

  std::unique_ptr<DocumentWindow> docwin = std::make_unique<DocumentWindow>(doc);
  DocumentWindow* pdocwin = docwin.get();  // Needed to set the view location.

  std::unique_ptr<Tab> tab = Tab::FromWindow(std::move(docwin));
  AddTab(std::move(tab));

  // We call this here since otherwise the pos/area of the window didn't updated.
  // which is needed to set the view of the coordinate.
  SetWindowActive(pdocwin);
  pdocwin->JumpTo(coord);

  return true;
}


Tab* Ui::GetActiveTab() const {
  if (active_tab_index < 0) return nullptr;
  ASSERT_INDEX(active_tab_index, tabs.size());
  Tab* tab = tabs[active_tab_index].get();
  ASSERT(tab != nullptr, OOPS);
  return tab;
}


void Ui::RemoveTab(const Tab* tab) {

  // Figure out the index of the tab.
  int index = -1;
  for (int i = 0; i < tabs.size(); i++) {
    if (tabs[i].get() == tab) {
      index = i;
      break;
    }
  }

  // Tab is not a child tab (maybe assert here?).
  if (index == -1) return;
  tabs.erase(tabs.begin() + index);

  // If the tabs.size() is 0, active_tabs will become -1 (nothing is active).
  if (index == active_tab_index && index == 0) {
    active_tab_index = tabs.size() - 1;
  } else if (index <= active_tab_index) {
    active_tab_index--;
  }
}


Window* Ui::GetWindowAt(Position pos) const {

  if (popup && popup.get()->IsPointIncluded(pos)) {
    return popup.get();
  }

  // It's possible for the active tab to be null, if we're at the wellcome screen.
  const Tab* tab = GetActiveTab();
  if (tab == nullptr) return nullptr;
  ASSERT(tab->GetRoot() != nullptr, OOPS);
  return tab->GetRoot()->GetWindowAt(pos);
}


Split* Ui::GetWindowSplit(const Window* window) const {
  for (auto& tab : tabs) {
    Split* root = tab->GetRoot();
    Split::Iterator it(root);
    while (it.Get()) {
      if (it.Get()->GetWindow() == window) {
        return it.Get();
      }
      it.Next();
    }
  }
  return nullptr;
}


Tab* Ui::GetSplitTab(const Split* split) const {
  const Split* root = split->GetRoot();
  for (auto& tab : tabs) {
    if (tab->GetRoot() == root) return tab.get();
  }
  return nullptr;
}


Window* Ui::GetActiveWindow(bool exclude_popup) const {
  // active_tab -> active_split -> window.

  // If a popup is available, it's the active window we disregard everything else.
  if (!exclude_popup && popup != nullptr) return popup.get();

  const Tab* tab = GetActiveTab();
  if (tab == nullptr) return nullptr; // Empty tabs container.

  ASSERT(tab->GetActiveSplit(), OOPS);
  ASSERT(tab->GetActiveSplit()->GetWindow(), OOPS);
  return tab->GetActiveSplit()->GetWindow();
}


void Ui::SetWindowActive(Window* window) {
  ASSERT(window != nullptr, OOPS);

  Split* split = GetWindowSplit(window);
  ASSERT(split != nullptr, OOPS);

  Tab* tab = GetSplitTab(split);
  ASSERT(tab != nullptr, OOPS);

  Window* last_active = GetActiveWindow();
  if (last_active) last_active->SetActive(false); // This will trigger the focus events.

  for (int i = 0; i < tabs.size(); i++) {
    Tab* child = tabs[i].get();
    if (child == tab) {
      active_tab_index = i;
      child->SetActiveSplit(split);
      window->SetActive(true);
      break;
    }
  }
}


DocumentWindow* Ui::GetDocumentWindow(const Path& path) const {
  // Only documents tabs will contain a document.
  for (int i = 0; i < tabs.size(); i++) {
    Split* root = tabs[i]->GetRoot();

    Split::Iterator it(root);
    while (it.Get()) {
      Window* window = it.Get()->GetWindow();
      if (window->GetType() == Window::Type::DOCUMENT) {
        DocumentWindow* docwin = static_cast<DocumentWindow*>(window);
        if (docwin->GetDocument()->GetPath() == path) return docwin;
      }
      it.Next();
    }

  }
  return nullptr;
}


bool Ui::CloseWindow(Window* window) {
  if (!window) return false;

  Split* split = GetWindowSplit(window);
  ASSERT(split != nullptr, OOPS);
  ASSERT(split->GetWindow() == window, OOPS);

  Tab* tab = GetSplitTab(split);
  Split* parent = split->GetParent();

  // if parent is nullptr, it's the root, remove the entire tab.
  if (parent == nullptr) {
    ASSERT(tab && tab->GetRoot() == split, OOPS);
    // This will delete the tab and do the cleanups and change the active tab (if has any).
    RemoveTab(tab);
    return true;
  }

  // Since this split is the current active split, we need to change the active split.
  Split::Iterator it(split);
  ASSERT(it.Get() != nullptr, OOPS);
  it.Next(); // Increment the iterator.

  // Before removing we set the active split to the next split.
  Split* next = it.Get();
  if (next == nullptr) {
    it = Split::Iterator(tab->GetRoot());
    next = it.Get();
  }
  ASSERT(next != nullptr && next != split, OOPS);
  tab->SetActiveSplit(next); // Change the next active split.

  // Now remove the split.
  int index = split->GetIndexInParent();
  parent->RemoveChild(index);

  return true;
}


bool Ui::Action_PopupFilesFinder(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindWindow>(std::make_unique<FilesFinder>());
  return true;
}


bool Ui::Action_PopupLiveGrep(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  if (self->popup != nullptr) return false;
  self->popup = std::make_unique<FindWindow>(std::make_unique<LiveGrep>());
  return true;
}


bool Ui::Action_NewDocument(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  // FIXME: Do I need to register the document at the editor registry, In that
  // case what path should i use?
  std::shared_ptr<Document> document = std::make_shared<Document>();
  std::unique_ptr<DocumentWindow> docwindow = std::make_unique<DocumentWindow>(document);

  std::unique_ptr<Tab> tab = Tab::FromWindow(std::move(docwindow));
  self->AddTab(std::move(tab));
  return true;
}


bool Ui::Action_TabNext(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  if (self->tabs.size() == 0) return false;
  self->active_tab_index++;
  self->active_tab_index %= self->tabs.size();
  return true;
}


bool Ui::Action_TabPrev(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  if (self->tabs.size() == 0) return false;
  if (self->active_tab_index == 0) {
    self->active_tab_index = self->tabs.size() - 1;
  } else {
    self->active_tab_index--;
  }
  return true;
}


bool Ui::Action_NextWindow(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  Tab* tab = self->GetActiveTab();
  if (!tab) return false;
  return tab->NextWindow();
}


bool Ui::Action_Vsplit(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  Tab* tab = self->GetActiveTab();
  if (!tab) return false;
  return tab->Vsplit();
}


bool Ui::Action_Hsplit(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  Tab* tab = self->GetActiveTab();
  if (!tab) return false;
  return tab->Hsplit();
}


bool Ui::Action_CloseWindow(ActionExecutor* ae) {
  Ui* self = static_cast<Ui*>(ae);
  Window* window = self->GetActiveWindow();
  return self->CloseWindow(window);
}

