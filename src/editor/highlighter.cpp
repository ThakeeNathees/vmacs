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
#include <regex>

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


HighlightCache* Highlighter::GetCache() const {
  return cache.get();
}


const std::vector<HighlightSlice>& Highlighter::GetHighlightSlices() const {
  return slices;
}


void Highlighter::OnBufferChanged(Buffer* buffer) {
  Highlight(buffer->GetSource(), buffer->GetSize());
  cache->CacheThemelets(buffer, this);
}


// TODO: Move this to a general place to parse tree-sitter predicates.
// The logic is stolen from: https://github.com/tree-sitter/tree-sitter/blob/a0cf0a7104f4a64eac04bd916297524db83d09c0/lib/binding_web/binding.js#L844
bool TreeSitterCheckPredicate(const char* source, const TSQuery* query, TSQueryMatch match) {

  // Incase of error we'll return true ignoring the predicate.
#define return_error return true
#define get_str(id) ts_query_string_value_for_id(query, (id), &length)
#define get_capture(id) ts_query_capture_name_for_id(query, (id), &length)

  uint32_t step_count;
  const TSQueryPredicateStep* steps = ts_query_predicates_for_pattern(query, match.pattern_index, &step_count);

  uint32_t i = 0;
  for (; i < step_count; i++) {
    if (steps[i].type == TSQueryPredicateStepTypeDone) break;
  }
  step_count = i;

  if (step_count == 0) return true; // Nothing to check.
  if (steps[0].type != TSQueryPredicateStepTypeString) return_error;

  uint32_t length;
  const char* predicate_name = get_str(steps[0].value_id);

  bool is_positive = true;
  switch (ConstHash(predicate_name)) {

    case "not-eq?"_hash: {
      is_positive = false;
      [[fallthrough]];

    case "eq?"_hash:
      if (step_count != 3) return_error;
      TODO;
    } break;

    case "not-match?"_hash: {
      is_positive = false;
      [[fallthrough]];

    case "match?"_hash:

      if (step_count != 3) return_error;
      if (steps[1].type != TSQueryPredicateStepTypeCapture) return_error;
      if (steps[2].type != TSQueryPredicateStepTypeString) return_error;

      const char* capture_name = get_capture(steps[1].value_id);
      std::regex re(get_str(steps[2].value_id));

      for (int i = 0; i < match.capture_count; i++) {
        TSQueryCapture capture = match.captures[i];

        if (strcmp(get_capture(capture.index), capture_name) == 0) {

          uint32_t start = ts_node_start_byte(capture.node);
          uint32_t end = ts_node_end_byte(capture.node);
          std::string text(source + start, end - start);
          return std::regex_match(text, re) == is_positive;
        }

      }
    } break;

    case "set!"_hash:
      TODO;
    break;

    case "is?"_hash: {
      [[fallthrough]];

    case "is-not?"_hash:
      TODO;
    } break;
    
  }

  return true;

#undef get_capture
#undef get_str
#undef return_error
}


void Highlighter::Highlight(const char* source, int size) {
  slices.clear();
  if (lang == nullptr || lang->GetQuery() == nullptr) return;

  ts_parser_set_language(parser, lang->GetData());
  const TSQuery* query = lang->GetQuery();

  // TODO: use ts_tree_edit and pass old tree here to make the parsing much
  // faster and efficient.
  tree = ts_parser_parse_string(parser, NULL, source, (uint32_t)size);

  TSQueryCursor* cursor = ts_query_cursor_new();
  TSNode root_node = ts_tree_root_node(tree);

  ts_query_cursor_exec(cursor, query, root_node);
  {
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {

      if (!TreeSitterCheckPredicate(source, query, match)) continue;
      if (match.capture_count == 0) continue; // We'll create a slice for the first capture.

      uint32_t len;
      TSQueryCapture capture = match.captures[0];
      TSNode node = capture.node;

      // TODO: Maybe we can do a red underline to indicate syntax error.
      if (ts_node_is_error(node)) continue;

      HighlightSlice slice;
      slice.capture = ts_query_capture_name_for_id(query, capture.index, &len);
      slice.start = ts_node_start_byte(node);
      slice.end = ts_node_end_byte(node);
      slices.push_back(slice);
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
