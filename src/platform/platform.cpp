//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "platform.hpp"

#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#define PATH_SEP "/"


Path::Path(std::string path) {
  this->path = path;
}


const std::string& Path::Get() const {
  return path;
}


Path Path::operator /(const std::string& inner) const {
  return this->path + PATH_SEP + inner;
}


bool Platform::ReadFile(std::string* ret, const Path& path) {
  ASSERT(ret != nullptr, OOPS);
  std::ifstream file(path.Get());
  if (!file.is_open()) return false;
  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  *ret = buffer.str();
  return true;
}


bool Platform::ListDirectory(std::vector<std::string>* items, const Path& path) {
  ASSERT(items != nullptr, OOPS);
  // FIXME(grep): Handle if this throws.
  for (const auto & entry : fs::directory_iterator(path.Get())) {
    std::string filename = entry.path().filename().string();
    items->push_back(std::move(filename));
  }
  return true;
}
