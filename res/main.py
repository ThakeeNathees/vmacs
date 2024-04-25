
import json
import toml
import os


HELIX_THEMES_DIR = "./runtime/themes/"
OUTPUT_DIR       = "./themes/"


def main():
  for item in os.listdir(HELIX_THEMES_DIR):
    if not item.endswith(".toml"): continue
    file_path = os.path.join(HELIX_THEMES_DIR, item)
    with open(file_path, 'r') as fi:
      content = fi.read()
      data = toml.loads(content)
      with open(OUTPUT_DIR + item.replace(".toml", ".json"), 'w') as fo:
        fo.write(dump(data))


# https://stackoverflow.com/a/68426598/10846399
def dumps_json(data, indent=2, depth=2):
    assert depth > 0
    space = ' '*indent
    s = json.dumps(data, indent=indent)
    lines = s.splitlines()
    N = len(lines)
    # determine which lines to be shortened
    is_over_depth_line = lambda i: i in range(N) and lines[i].startswith(space*(depth+1))
    is_open_bracket_line = lambda i: not is_over_depth_line(i) and is_over_depth_line(i+1)
    is_close_bracket_line = lambda i: not is_over_depth_line(i) and is_over_depth_line(i-1)
    # 
    def shorten_line(line_index):
        if not is_open_bracket_line(line_index):
            return lines[line_index]
        # shorten over-depth lines
        start = line_index
        end = start
        while not is_close_bracket_line(end):
            end += 1
        has_trailing_comma = lines[end][-1] == ','
        _lines = [lines[start][-1], *lines[start+1:end], lines[end].replace(',','')]
        d = json.dumps(json.loads(' '.join(_lines)))
        return lines[line_index][:-1] + d + (',' if has_trailing_comma else '')
    # 
    s = '\n'.join([
        shorten_line(i)
        for i in range(N) if not is_over_depth_line(i) and not is_close_bracket_line(i)
    ])
    #
    return s


# This dumps palette as expanded but everything else as collapsed.
def dump(data):
  output = ''
  palette = data.get('palette', None)
  for line in dumps_json(data, 2, 1).splitlines():
    if line.strip().startswith('"palette"'):
      output += '  "palette" :\n    '
      palette_dump = json.dumps(palette, indent=2)
      palette_dump = '\n    '.join(palette_dump.split('\n'))
      output += palette_dump + '\n'
    else: output += line + '\n'
  return output


if __name__ == "__main__":
  main()

