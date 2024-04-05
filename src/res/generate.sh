#!/bin/bash


function main() {
  xxd -i "fonts/UbuntuMonoNerdFont-Regular.ttf" "fonts/UbuntuMonoNerdFont-Regular.ttf.gen.h"
  xxd -i "fonts/UbuntuMonoNerdFont-Bold.ttf"    "fonts/UbuntuMonoNerdFont-Bold.ttf.gen.h"
  xxd -i "fonts/UbuntuMonoNerdFont-Italic.ttf"  "fonts/UbuntuMonoNerdFont-Italic.ttf.gen.h"
}


function generate_text_file() {
  local input_filename="$1"
  local output_filename="$2"

  # Read the content from the input file.
  local toml_content=$(<"$input_filename")

  # Generate the C++ header file content.
  local cpp_content="const char* text = R\"!!(\\
$toml_content)!!\";"

  # Write the C++ header file content to the output file
  echo "$cpp_content" > "$output_filename"
}


main
