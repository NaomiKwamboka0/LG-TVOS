#!/bin/sh
# Downloads pinned dependencies (a lock file, default tools/win-deps.lock)
# into deps/<name>/, checking each SHA-256 first. Each archive is unpacked into
# its own new, empty folder; nothing downloaded is ever run here.
# Needs curl, shasum, tar, unzip and 7zz (brew install sevenzip).
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
LOCK="${1:-$ROOT/tools/win-deps.lock}"
DEPS="$ROOT/deps"

grep -v '^#' "$LOCK" | grep -v '^$' | while IFS='|' read -r name version url sha; do
    dir="$DEPS/$name"
    if [ -f "$dir/.ok" ] && [ "$(cat "$dir/.ok")" = "$version $sha" ]; then
        echo "$name $version: already here"
        continue
    fi
    rm -rf "$dir"
    mkdir -p "$dir/download" "$dir/x"
    file="$dir/download/$(basename "$url")"
    echo "$name $version: downloading"
    curl -fsSL --retry 3 --proto '=https' -o "$file" "$url"
    got=$(shasum -a 256 "$file" | cut -d' ' -f1)
    if [ "$got" != "$sha" ]; then
        echo "$name: SHA-256 mismatch (got $got, want $sha); refusing it" >&2
        rm -rf "$dir"
        exit 1
    fi
    case "$file" in
    *.tar.gz) tar -xzf "$file" -C "$dir/x" ;;
    *.zip) unzip -q "$file" -d "$dir/x" ;;
    *.7z) 7zz x -bd -y "-o$dir/x" "$file" > /dev/null ;;
    *) cp "$file" "$dir/x/" && chmod +x "$dir/x/$(basename "$file")" ;;
    esac
    echo "$version $sha" > "$dir/.ok"
done
echo "deps: OK ($DEPS)"
