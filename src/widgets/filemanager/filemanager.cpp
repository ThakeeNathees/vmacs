// Copyright (c) 2023 Thakee Nathees

#include "filemanager.h"


FileManager::FileManager(Window* window) : Widget(window) {
	cwd = std::filesystem::current_path();
}


bool FileManager::HandleEvent(const Event& event) {
	return false;
}


void FileManager::_Draw(Size area) {
	
	Vector2 curr = { 0, 0 };
	int h = window->GetFontCharSize().y;
	for (auto& p : std::filesystem::directory_iterator(cwd)) {
		if (p.is_directory()) {
			DrawTextEx(window->GetFont(), (p.path().filename().string() + "/").c_str(), curr, window->GetFontSize(), 0, RED);

		} else if (p.is_regular_file()) {
			DrawTextEx(window->GetFont(), p.path().filename().string().c_str(), curr, window->GetFontSize(), 0, RED);

		}

		curr.y += h;
	}
}


void FileManager::Update() {

}


