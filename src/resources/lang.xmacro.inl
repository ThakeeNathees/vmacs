
#define LANGUAGE(X) \
  X("c",      c_query_highlight, tree_sitter_c) \
  X("cpp",    cpp_query_highlight, tree_sitter_cpp) \
  X("python", python_query_highlight, tree_sitter_python) \


static const char* c_query_highlight =
  #include "resources/languages/c/highlight.scm.inl"
;


static const char* cpp_query_highlight =
  #include "resources/languages/cpp/highlight.scm.inl"
;


static const char* python_query_highlight =
  #include "resources/languages/python/highlight.scm.inl"
;

