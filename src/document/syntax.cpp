//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "document.hpp"

// Treesitter docs:
//
//   Note - Predicates are not handled directly by the Tree-sitter C library.
//   They are just exposed in a structured form so that higher-level code can
//   perform the filtering. However, higher-level bindings to Tree-sitter like
//   the Rust Crate or the WebAssembly binding do implement a few common
//   predicates like the #eq?, #match?, and #any-of? predicates explained above.
//
// Here is our "high-level-language" implementation.
static bool TreeSitterCheckPredicate(const Buffer* buff, const TSQuery* query, TSQueryMatch match);


Language::Language (Language&& other) {
  id               = other.id;
  query_highlight  = other.query_highlight;
  query_textobject = other.query_textobject;

  other.query_highlight  = nullptr;
  other.query_textobject = nullptr;
}


Language::~Language() {
  if (query_highlight) ts_query_delete(query_highlight);
  if (query_textobject) ts_query_delete(query_textobject);
}


Syntax::~Syntax() {
  if (tree) ts_tree_delete(tree);
  if (parser) ts_parser_delete(parser);
}


const std::vector<Style>& Syntax::GetHighlights() const {
  return highlights;
}


void Syntax::Parse(const Language* language, const Buffer* buffer, const Theme* theme) {

  if (language == nullptr) {
    slices.clear();
    return;
  }

  // Parse the source code.
  if (parser == NULL) parser = ts_parser_new();
  ts_parser_set_language(parser, language->data);

  // TODO: Use ts_tree_edit and pass old tree here to make the parsing much
  // faster and efficient.
  if (tree) ts_tree_delete(tree);
  tree = ts_parser_parse_string(parser, NULL, buffer->GetData(), buffer->GetSize());

  // Cache highlight slices.
  CacheHighlightSlices(language->query_highlight, buffer);
  CacheBufferStyle(theme, buffer);

  // TODO: Cache textobjects (functions, classes, enums, parameters, etc to navigate/view-all-symbols).
}


void Syntax::CacheHighlightSlices(const TSQuery* query, const Buffer* buffer) {

  slices.clear();
  if (query == nullptr) return;

  TSQueryCursor* cursor = ts_query_cursor_new();
  TSNode root_node = ts_tree_root_node(tree);

  ts_query_cursor_exec(cursor, query, root_node);
  {
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {

      if (!TreeSitterCheckPredicate(buffer, query, match)) continue;
      if (match.capture_count == 0) continue; // We'll create a slice for the first capture.

      uint32_t len;
      TSQueryCapture capture = match.captures[0];
      TSNode node = capture.node;

      // TODO: Maybe we can do a red underline to indicate syntax error. if the
      // language server isn't available.
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
    std::sort(slices.begin(), slices.end(), [](const HighlightSlice& l, const HighlightSlice& r) {
      return l.start < r.start;
    });

    auto iter = std::unique(slices.begin(), slices.end(), [](const HighlightSlice& l, const HighlightSlice& r) {
      return l.start == r.start && l.end == r.end;
    });
    slices.erase(iter, slices.end());
  }
}


// The logic is stolen from: https://github.com/tree-sitter/tree-sitter/blob/a0cf0a7104f4a64eac04bd916297524db83d09c0/lib/binding_web/binding.js#L844
static bool TreeSitterCheckPredicate(const Buffer* buff, const TSQuery* query, TSQueryMatch match) {

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
      if (steps[1].type != TSQueryPredicateStepTypeCapture) return_error;

      if (steps[2].type == TSQueryPredicateStepTypeString) {
        const char* capture_name_1 = get_capture(steps[1].value_id);
        const char* capture_name_2 = get_capture(steps[2].value_id);

        TSNode* node1 = NULL;
        TSNode* node2 = NULL;

        for (int i = 0; i < match.capture_count; i++) {
          TSQueryCapture capture = match.captures[i];
          if (strcmp(get_capture(capture.index), capture_name_1) == 0) {
            node1 = &capture.node;
          }
          if (strcmp(get_capture(capture.index), capture_name_2) == 0) {
            node2 = &capture.node;
          }
        }

        if (node1 == NULL || node2 == NULL) return true;

        uint32_t start1 = ts_node_start_byte(*node1);
        uint32_t end1 = ts_node_end_byte(*node1);
        String text1 = buff->GetSubString(start1, end1-start1);

        uint32_t start2 = ts_node_start_byte(*node2);
        uint32_t end2 = ts_node_end_byte(*node2);
        String text2 = buff->GetSubString(start2, end2-start2);

        return (text1 == text2) == is_positive;

      } else {
        const char* capture_name = get_capture(steps[1].value_id);
        const char* str = get_str(steps[2].value_id);

        for (int i = 0; i < match.capture_count; i++) {
          TSQueryCapture capture = match.captures[i];
          if (strcmp(get_capture(capture.index), capture_name) == 0) {
            uint32_t start = ts_node_start_byte(capture.node);
            uint32_t end = ts_node_end_byte(capture.node);
            String text = buff->GetSubString(start, end - start);
            return (text == str) == is_positive;
          }
        }

      }
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
          String text = buff->GetSubString(start, end - start);
          return std::regex_match(text.Data(), re) == is_positive;
        }
      }

    } break;

    // Not supporting these predicates, If needed someone contribute.
    //  case "set!"_hash:
    //  break;
    //
    //  case "is?"_hash: {
    //    [[fallthrough]];
    //
    //  case "is-not?"_hash:
    //  } break;

  }

  return true;

  #undef get_capture
  #undef get_str
  #undef return_error
}


void Syntax::CacheBufferStyle(const Theme* theme, const Buffer* buffer) {
  ASSERT(buffer != nullptr, OOPS);

  if (theme == nullptr) {
    highlights.clear();
    return;
  }

  size_t buffer_size = buffer->GetSize();
  highlights.assign(buffer_size, theme->text);

  for (auto it = slices.begin(); it != slices.end(); ++it) {
    Style style = theme->GetStyle(it->capture);
    for (int i = it->start; i < it->end; i++) {
      if (i >= buffer_size) return; // Failsafe.
      highlights[i].ApplyInplace(style);
    }
  }
}
