// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <memory>

#include "core/window.h"
#include "core/widget.h"

class HSplit : public Widget {
public:
  HSplit(Window* window,
         std::unique_ptr<Widget> first,
         std::unique_ptr<Widget> second);

private:
  void _Draw(Size area) override;
};
