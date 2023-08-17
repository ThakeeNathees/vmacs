#!/usr/bin/bash

function main() {
  #generate_binary_file fonts/NerdDroidSansMono.otf fonts/NerdDroidSansMono.otf.gen.h
  #generate_text_file queries/c.scm queries/c.scm.gen.h
  #generate_text_file themes/darcula.toml themes/darcula.toml.gen.h
  #generate_text_file themes/jellybeans.toml themes/jellybeans.toml.gen.h
  #generate_text_file themes/dark_plus.toml themes/dark_plus.toml.gen.h
  generate_text_file themes/everforest_dark.toml themes/everforest_dark.toml.gen.h
}


function generate_binary_file() {
  input_file=$1
  output_file=$2
  echo "Generating $output_file from $input_file"
  xxd -i "$input_file" "$output_file"
  echo "Generated $output_file"
}


# Generate a text file to a c source file with the content as const char*.
# function generate_text_file() {
#   input_file=$1
#   output_file=$2
#   file_name=$(echo "$input_file" | sed 's/.*\///')
#   var_name=$(echo "$file_name" | sed 's/\./_/g')

#   echo "Generating $output_file from $input_file"
#   echo "" > "$output_file"
#   echo "const char* $var_name = \\" >> "$output_file"

#   while IFS= read -r line; do
#     #line_escaped=$(printf '%s' "$line" | sed 's/\\/\\\\/g; s/"/\\"/g')
#     #echo "\"$line_escaped\\n\"" >> "$output_file"
#     echo "$line"
#   done < "$input_file"
    
#   echo "\"\";" >> "$output_file"

#   echo "Generated $output_file"
# }

generate_text_file() {
  input_file="$1"
  output_file="$2"

  filename="${input_file##*/}"
  var_name="${filename//./_}"

  touch "$output_file"
  echo "const char* $var_name = \\" >> "$output_file"

  while IFS= read -r line; do
    line_escaped=$(printf '%s' "$line" | sed 's/\\/\\\\/g; s/"/\\"/g')
    echo "\"$line_escaped\\n\"" >> "$output_file"
  done < "$input_file"

  echo "\"\";" >> "$output_file"
  echo "Generated $output_file"
}


main $@
