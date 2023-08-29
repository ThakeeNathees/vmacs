
#include "lsp.hpp"

bool RequestId::operator==(const RequestId& other) {
  return (_str == other._str && _int == other._int);
}


bool RequestId::operator==(int val) {
  return (_str.length() == 0 && _int == val);
}


bool RequestId::operator==(const std::string& val) {
  return (_str.length() > 0 && _str == val);
}


bool RequestId::operator==(const char* val) {
  return (_str.length() > 0 && strcmp(val, _str.data()) == 0);
}


void to_json(json& j, const RequestId& t) {
  if (t._str != "") j = t._str;
  else j = t._int;
}


void from_json(const json& j, RequestId& t) {
  if (j.is_null()) return;
  if (j.is_string()) t._str = j.template get<std::string>();
  else t._int = j.template get<int>();
}
