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


Path::Path(const char* path) : Path(std::string(path)) {
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


std::string Path::RelativeFrom(const Path& from) const {
  return path.lexically_relative(from.path).string();
}


bool Path::Empty() const {
  return path.empty();
}


bool Path::Exists() const {
  return fs::exists(path);
}


bool Path::IsDirectory() const {
  return fs::is_directory(path);
}


bool Path::IsRegularFile() const {
  return fs::is_regular_file(path);
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


void Platform::ListDirectory(std::vector<Path>& items, const Path& path) {
  ASSERT(path.IsDirectory(), "ListDirectory called on a path which is not a directory.");

  for (const auto & entry : fs::directory_iterator(path.String())) {
    Path item = Path(entry.path().string());
    items.push_back(std::move(item));
  }
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
