
#define LANGUAGE(X) \
  X("c", c_query_highlight, tree_sitter_c) \
  X("cpp", c_query_highlight, tree_sitter_c) \
  X("js", NULL, tree_sitter_javascript) \


static const char* c_query_highlight =
  #include "resources/queries/c_highlight.scm.inl"
;

