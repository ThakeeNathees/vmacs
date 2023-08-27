#!/usr/bin/bash

xxd -i "fonts/UbuntuMonoNerdFont-Regular.ttf" "fonts/UbuntuMonoNerdFont-Regular.ttf.gen.h"
xxd -i "fonts/UbuntuMonoNerdFont-Bold.ttf"    "fonts/UbuntuMonoNerdFont-Bold.ttf.gen.h"
xxd -i "fonts/UbuntuMonoNerdFont-Italic.ttf"  "fonts/UbuntuMonoNerdFont-Italic.ttf.gen.h"
