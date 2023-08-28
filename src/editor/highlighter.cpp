// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

// TODO: The highlighting alorithm is not quite optimal, but did this way to
// complete the development reapidly and need to be fixed in the future.

#include "highlighter.hpp"

#include <algorithm>

#include "core/theme.hpp"


void HighlightCache::CacheThemelets(Buffer* buffer, const Highlighter* highlighter) {

  const Theme* theme = ThemeManager::GetCurrentTheme();
  if (theme == nullptr) {
    themelets.clear();
    return;
  }

  Color text = theme->GetUiEntries().text.fg;
  int themelet_size = buffer->GetSize();
  themelets.assign(themelet_size, { text, 0 });

  const std::vector<HighlightSlice>& slices = highlighter->GetHighlightSlices();

  for (auto it = slices.begin(); it != slices.end(); ++it) {

    // For a key like "keyword.control.conditional" if not exists, we again
    // search for "keyword.control" and then "keyword".
    std::string name = it->capture;
    const ThemeEntry* entry = theme->GetEntry(name);
    if (entry == nullptr) continue;

    bool done = false;
    for (int i = it->start; i < it->end; i++) {
      // Failsafe in case if something went wrong earlier.
      if (i >= themelet_size) {
        done = true;
        break;
      }
      themelets[i].color = entry->fg;
      themelets[i].modifiers = entry->modifier;
    }
    if (done) break;
  }
}


const std::vector<Themelet>& HighlightCache::GetThemeles() const {
  return themelets;
}


Highlighter::Highlighter() {
  parser = ts_parser_new();
}


Highlighter::~Highlighter() {
  if (parser) ts_parser_delete(parser);
  if (tree) ts_tree_delete(tree);
}


void Highlighter::SetLanguage(const Language* lang) {
  this->lang = lang;
}


const HighlightSlice* Highlighter::GetHighlightSlice(int index) const {
  for (const HighlightSlice& slice : slices) {
    if (slice.start <= index && index < slice.end) return &slice;
  }
  return nullptr;
}


const HighlightCache* Highlighter::GetCache() const {
  return cache.get();
}


void Highlighter::OnBufferChanged(Buffer* buffer) {
  Highlight(buffer->GetSource(), buffer->GetSize());
  cache->CacheThemelets(buffer, this);
}


void Highlighter::Highlight(const char* source, int size) {
  slices.clear();
  if (lang == nullptr || lang->GetQuery() == nullptr) return;

  ts_parser_set_language(parser, lang->GetData());

  // TODO: use ts_tree_edit and pass old tree here to make the parsing much
  // faster and efficient.
  tree = ts_parser_parse_string(parser, NULL, source, (uint32_t)size);

  TSQueryCursor* cursor = ts_query_cursor_new();
  TSNode root_node = ts_tree_root_node(tree);

  ts_query_cursor_exec(cursor, lang->GetQuery(), root_node);
  {
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
      for (int i = 0; i < match.capture_count; i++) {

        uint32_t len;
        const char* capture = ts_query_capture_name_for_id(lang->GetQuery(), match.captures[i].index, &len);
        TSNode node = match.captures[i].node;

        HighlightSlice slice;
        slice.capture = capture;
        slice.start = ts_node_start_byte(node);
        slice.end = ts_node_end_byte(node);
        slices.push_back(slice);
      }
    }
  }
  ts_query_cursor_delete(cursor);

  {
    // Remove duplicates.
    // TODO: Seemslike in C, identifier is captured as both variable and constant
    // however the constant set to match a regex all uppercase but it doesn't seems
    // like working, investigate.
    auto iter = std::unique(slices.begin(), slices.end(), [](const HighlightSlice& l, const HighlightSlice& r) {
      return l.start == r.start && l.end == r.end;
    });
    slices.erase(iter, slices.end());
  }

#ifdef DEBUG
  { // All the slices are sorted with start index if not the slice r, be a subset of slice l.
    if (!slices.empty()) {
      for (auto iter = slices.begin() + 1; iter != slices.end(); ++iter) {
        const HighlightSlice& l = *(iter - 1);
        const HighlightSlice& r = *iter;
        if (l.end <= r.start) continue;
        ASSERT(
          BETWEEN(l.start, r.start, l.end) && BETWEEN(l.start, r.end, l.end),
          "An inveriant assumption about the tree-sitter query result is failed.");
      }
    }
  }
#endif // DEBUG

}
