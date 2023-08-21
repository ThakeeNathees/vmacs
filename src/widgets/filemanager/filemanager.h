// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <filesystem>

#include "core/window.h"


class FileManager : public Widget {

public:
	FileManager(Window* window);

	bool HandleEvent(const Event& event) override;
	void Update() override;

private:
	void _Draw(Size area) override;

	std::filesystem::path cwd;
};
