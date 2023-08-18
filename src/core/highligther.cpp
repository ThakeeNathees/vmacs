// Copyright (c) 2023 Thakee Nathees

#include <algorithm>

#include "highligther.h"


// TODO: Move these things to somewhere.
// Defined in the parsers project, statically linked.
#include "res/queries/c.scm.gen.h"

extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_python(void);


std::map<std::string, std::unique_ptr<Language>> Language::languages;


Language::~Language() {
	if (query) ts_query_delete(query);
}


void Language::LoadLanguages() {

	// TODO: Handle the errors properly Send error message to minibuffer or something.
	uint32_t offset = 0;
	TSQueryError error_type = TSQueryErrorNone;

	{
		std::unique_ptr<Language> l = Language::New("c");
		l->data = tree_sitter_c();
		l->query = ts_query_new(l->data, c_scm, (uint32_t) strlen(c_scm), &offset, &error_type);
		ASSERT(l->query != NULL, OOPS);
		languages[l->name] = std::move(l);
	}

	{
		std::unique_ptr<Language> l = Language::New("python");
		l->data = tree_sitter_c();
		l->query = nullptr;
		languages[l->name] = std::move(l);
	}

}


const Language* Language::GetLanguage(const std::string& name) {
	auto iter = languages.find(name);
	if (iter == languages.end()) return nullptr;
	return iter->second.get();
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


void Highlighter::Highlight(const char* source, int size) {
	slices.clear();
	if (lang == nullptr || lang->GetQuery() == nullptr) return;

	ts_parser_set_language(parser, lang->Get());

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


//void Highlighter::_SortSlices() {
//
//	// Sort in a way that smaller slices comes before the large enclosing slices.
//	//
//	// Example:
//	//
//	// (1, 2) (3, 10) (5, 9) (7, 8) (11, 12)
//	//
//	// Sorting above will become.
//	//
//	// (1, 2) (7, 8) (5, 9) (3, 10) (11, 12)
//	//
//
//	for (auto iter = slices.begin() + 1; iter != slices.end(); ++iter) {
//
//		auto it = iter;
//		while (true) {
//			const HighlightSlice& l = *(it - 1);
//			const HighlightSlice& r = *it;
//			if (r.start >= l.end) break;
//			// It means r.start < l.end.
//			ASSERT(r.end <= l.end, "Collition between slices. Interesting.");
//			std::iter_swap(it, it - 1);
//			--it;
//			if (it == slices.begin()) break;
//		}
//
//	}
//}
