#!/usr/bin/env bash
# This helper script calls the copyright_headers.py script on the source tree
# in update mode, parses the result to identify files with missing copyright
# headers, and runs the script again in insert mode on those which are not
# third party libraries.

export LC_ALL=C

# unbuffered so we get a fast progress view
result=$(/usr/bin/env python3 -u contrib/devtools/copyright_header.py update src | tee /dev/stderr)

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
    | grep -v "^src/bench/data.h$"
)

for file in "${files[@]}"; do
  /usr/bin/env python3 contrib/devtools/copyright_header.py insert "src/$file"
  inserted_year=$(fgrep 'The Bitcoin Core developers' "src/$file" | head -1 | cut -d ' ' -f 4)
  printf '%-53s' "$file"
  if [ -z "$inserted_year" ]; then
    echo "Not updated."
  else
    echo "Copyright inserted -> $inserted_year"
  fi
done
