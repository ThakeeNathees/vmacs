// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#pragma once

#include "core/common.hpp"
#include "core/widget.hpp"


class Split : public Widget {
public:
  enum Type {
    VSPLIT,
    HSPLIT,
  };

protected: Split(Split::Type type);
public:	static std::unique_ptr<Split> New(Split::Type type);

public:

  bool IsSplit() const override;
  Type GetType() const;

  std::vector<Widget*> Traverse();

  // Split the widget from it's parent and return the split pointer.
  // This will return split container of the widget.
  static Widget* SplitChild(Widget* widget, Split::Type type);

private:
  Type type;
};


class VSplit : public Split {
public:
  VSplit();

private:
  void _Draw(Size area) override;
};


class HSplit : public Split {
public:
  HSplit();

private:
  void _Draw(Size area) override;
};
