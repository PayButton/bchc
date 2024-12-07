#!/usr/bin/env bash
# This helper script calls the copyright_headers.py script on the source tree
# in update mode, parses the result to identify files with missing copyright
# headers, and runs the script again in insert mode on those which are not
# third party libraries.

export LC_ALL=C

echo "# Updating existing copyrights:" >&2

path="src"

# unbuffered so we get a fast progress view
result=$(/usr/bin/env python3 -u contrib/devtools/copyright_header.py update "$path" | tee /dev/stderr)

# list files the script didn't update, and exclude libraries
mapfile -t files < <(
  grep 'No updatable copyright.' <<< "$result" \
    | cut -c 1-52 \
    | sed 's/ *$//' \
    | grep -v "^crypto" \
    | grep -v "^leveldb" \
    | grep -v "^secp256k1" \
    | grep -v "^univalue" \
    | grep -v "^crc32c" \
    | grep -v "^bench/data.h$"
)

echo >&2
if [ -z "$files" ]; then
  echo "# No other copyrights to insert, finished."
  exit
fi
echo "# Inserting new copyrights:" >&2

for file in "${files[@]}"; do
  file="$path/$file"
  if git log -1 --pretty=%s "$file" | grep -Fq "copyright"; then
    # check if the last commit was an update to the copyright, and don't insert
    printf '%-53s' "$file" >&2
    echo "Copyright updated in the last commit." >&2
    continue
  fi

  /usr/bin/env python3 contrib/devtools/copyright_header.py insert "$file"
  inserted_year=$(grep -F 'The Bitcoin developers' "$file" | head -1 | cut -d ' ' -f 4)
  printf '%-53s' "$file" >&2
  if [ -z "$inserted_year" ]; then
    echo "Not updated." >&2
  else
    echo "Copyright inserted -> $inserted_year" >&2
  fi
done
