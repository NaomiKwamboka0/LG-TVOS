#!/bin/sh
# dylibbundler adds its rpath to a library each time it rewrites it, and
# macOS refuses to load a file with the same LC_RPATH twice ("duplicate
# LC_RPATH"). Keeps one copy of each rpath in every file given.
set -eu

for f in "$@"; do
    otool -l "$f" | awk '/cmd LC_RPATH/ {r = 1} r && /path / {print $2; r = 0}' | sort | uniq -d |
        while read -r path; do
            n=$(otool -l "$f" | awk -v p="$path" '/cmd LC_RPATH/ {r = 1} r && /path / {if ($2 == p) c++; r = 0} END {print c}')
            while [ "$n" -gt 1 ]; do
                install_name_tool -delete_rpath "$path" "$f" 2> /dev/null
                n=$((n - 1))
            done
        done
done
