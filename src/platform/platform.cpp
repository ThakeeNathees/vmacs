//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#include "platform.hpp"

#include <cstdlib>
#include <fstream>


Path::Path(std::string path) {
  this->path = Normalize(fs::path(path));
}


std::string Path::String() const {
  return path.string();
}


std::string Path::Uri() const {
  return std::string("file://") + path.string();
}


std::string Path::FileName() const {
  return path.filename();
}


bool Path::Empty() const {
  return path.empty();
}


bool Path::Exists() const {
  return fs::exists(path);
}


bool Path::operator ==(const Path& other) const {
  return path == other.path;
}


Path Path::operator /(const std::string& inner) const {
  return Path(path/inner);
}


bool Path::operator <(const Path& other) const {
  return path.string() < other.path.string();
}


Path Path::FromUri(std::string uri) {
  // TODO: It's not a valid uri, handle here or assert.
  if (!StartsWith(uri, "file://")) return Path(uri);
  return Path(uri.erase(0, strlen("file://")));
}


fs::path Path::Normalize(const fs::path& path) {
  fs::path canonical = std::filesystem::weakly_canonical(path);
  return canonical.make_preferred();
}


bool Platform::ReadFile(std::string* ret, const Path& path) {
  ASSERT(ret != nullptr, OOPS);
  std::ifstream file(path.String());
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
  for (const auto & entry : fs::directory_iterator(path.String())) {
    std::string filename = entry.path().filename().string();
    items->push_back(std::move(filename));
  }
  return true;
}


bool Platform::IsCommandExists(const std::string& command) {
  if (StartsWith(command, "./") || StartsWith(command, "../")) {
    return Path(command).Exists();
  }
  const char* PATH = std::getenv("PATH");
  if (PATH == NULL) return false;

  char sep = GetPathSeparator(); // Either ':' or ';'.
  std::vector<std::string> paths = StringSplit(PATH, sep);
  for (std::string& path : paths) {
    if ((Path(path) / command).Exists()) return true;
  }
  return false;
}
