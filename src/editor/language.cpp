// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#include "language.hpp"


std::map<std::string, std::unique_ptr<Language>> LanguageManager::languages;


void Language::SetData(const TSLanguage* data) {
  this->data = data;
}


void Language::SetQuery(const char* source) {
  // TODO: If error send error message.
  uint32_t offset = 0;
  TSQueryError error_type = TSQueryErrorNone;

  query = ts_query_new(data, source, (uint32_t) strlen(source), &offset, &error_type);
}


const TSLanguage* Language::GetData() const {
  return data;
}


const std::string& Language::GetName() const {
  return name;
}


const TSQuery* Language::GetQuery() const {
  return query;
}


Language::~Language() {
  if (query) ts_query_delete(query);
}


void LanguageManager::Init() {

}


void LanguageManager::AddLanguage(std::unique_ptr<Language> lang) {
  languages[lang->GetName()] = std::move(lang);
}


const Language* LanguageManager::GetLanguage(const std::string& name) {
  auto it = languages.find(name);
  if (it == languages.end()) return nullptr;
  return it->second.get();
}
