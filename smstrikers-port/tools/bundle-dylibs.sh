#!/bin/sh
# Make a macOS executable self-contained: copy every non-system dylib in its closure beside it,
# give each an install name of @executable_path/<name>, rewrite every reference to it and re-sign.
# Usage: bundle-dylibs.sh <executable> [package dir].

set -e

BIN="${1:?usage: bundle-dylibs.sh <executable> [package dir]}"
OUT="${2:-$(dirname "$BIN")}"

case "$(uname -s)" in
Darwin) ;;
*) echo "bundle-dylibs: nothing to do on $(uname -s)"; exit 0 ;;
esac

BIN_DIR=$(cd "$(dirname "$BIN")" && pwd -P)
OUT_REAL=$(cd "$OUT" && pwd -P)

# Load commands only: `otool -L` lists a dylib's own install name first, which is no dependency.
deps_of() {
    otool -l "$1" | awk '
        /^ *cmd LC_(LOAD|LOAD_WEAK|REEXPORT|LOAD_UPWARD)_DYLIB/ { f = 1; next }
        f && /^ *name / { sub(/^ *name /, ""); sub(/ \(offset [0-9]+\)$/, ""); print; f = 0 }'
}

rpaths_of() {
    otool -l "$1" | sed -n 's/^ *path \(.*\) (offset [0-9]*)$/\1/p'
}

is_system() {
    case "$1" in
    /usr/lib/*|/System/Library/*) return 0 ;;
    *) return 1 ;;
    esac
}

# Where dyld would find the load command "$2" of the image "$1"; nothing when it resolves nowhere.
resolve() {
    image_dir=$(cd "$(dirname "$1")" && pwd -P)
    case "$2" in
    @executable_path/*) printf '%s\n' "$BIN_DIR/${2#@executable_path/}" ;;
    @loader_path/*)     printf '%s\n' "$image_dir/${2#@loader_path/}" ;;
    @rpath/*)
        rest=${2#@rpath/}
        rpaths_of "$1" | while IFS= read -r rp; do
            case "$rp" in
            @executable_path/*) rp="$BIN_DIR/${rp#@executable_path/}" ;;
            @loader_path/*)     rp="$image_dir/${rp#@loader_path/}" ;;
            esac
            if [ -f "$rp/$rest" ]; then
                printf '%s\n' "$rp/$rest"
                break
            fi
        done ;;
    *) printf '%s\n' "$2" ;;
    esac
}

# The file behind a path, through every symlink, so one file has one name.
canon() {
    if readlink -f / >/dev/null 2>&1; then
        real=$(readlink -f "$1" 2>/dev/null)
    else
        real=$(python3 -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' "$1")
    fi
    if [ -n "$real" ] && [ -f "$real" ]; then
        printf '%s\n' "$real"
    else
        printf '%s\n' "$1"
    fi
}

# 1. The closure, by original location.
QUEUE=$(mktemp); SEEN=$(mktemp); COPIED=$(mktemp); DEPS=$(mktemp)
trap 'rm -f "$QUEUE" "$SEEN" "$COPIED" "$DEPS"' EXIT
echo "$BIN" > "$QUEUE"
while [ -s "$QUEUE" ]; do
    cur=$(head -n 1 "$QUEUE"); sed -i '' '1d' "$QUEUE"
    deps_of "$cur" > "$DEPS"
    while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        is_system "$dep" && continue
        target=$(resolve "$cur" "$dep")
        if [ -z "$target" ] || [ ! -f "$target" ]; then
            echo "==> bundle-dylibs: $dep, wanted by $(basename "$cur"), resolves nowhere on this machine; the audit will say so" >&2
            continue
        fi
        real=$(canon "$target")
        case "$real" in
        "$OUT_REAL"/*) continue ;;   # already beside the executable
        esac
        if ! grep -qxF "$real" "$SEEN"; then
            echo "$real" >> "$SEEN"
            echo "$real" >> "$QUEUE"
        fi
    done < "$DEPS"
done

COUNT=$(wc -l < "$SEEN" | tr -d '[:space:]')
if [ "$COUNT" -eq 0 ]; then
    echo "==> bundle-dylibs: $BIN links only system libraries; nothing to bundle"
    exit 0
fi

# 2. Copy, and record original path -> bundled name, because every spelling of it has to be rewritten.
while IFS= read -r lib; do
    base=$(basename "$lib")
    cp -f "$lib" "$OUT/$base"
    chmod u+w "$OUT/$base"
    echo "$lib $base" >> "$COPIED"
done < "$SEEN"

# 3. Rewrite: every dependency that resolves to a bundled file becomes @executable_path/<name>,
#    resolved against the image's original location and matched by resolved path.
rewrite() {
    target="$1"
    original="$2"
    deps_of "$target" > "$DEPS"
    while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        is_system "$dep" && continue
        case "$dep" in @executable_path/*) continue ;; esac
        resolved=$(resolve "$original" "$dep")
        [ -n "$resolved" ] || continue
        real=$(canon "$resolved")
        name=$(awk -v k="$real" '$1 == k { print $2 }' "$COPIED")
        [ -n "$name" ] || continue
        # The signature warning is expected: step 4 re-signs everything.
        install_name_tool -change "$dep" "@executable_path/$name" "$target" 2>/dev/null
    done < "$DEPS"
}

rewrite "$BIN" "$BIN"
while IFS= read -r lib; do
    base=$(basename "$lib")
    install_name_tool -id "@executable_path/$base" "$OUT/$base" 2>/dev/null
    rewrite "$OUT/$base" "$lib"
done < "$SEEN"

# 4. Re-sign everything touched; ad hoc is enough for a local archive.
codesign --force -s - "$BIN" 2>/dev/null
while IFS= read -r lib; do
    codesign --force -s - "$OUT/$(basename "$lib")" 2>/dev/null
done < "$SEEN"

TOTAL=$(du -ch $(sed 's|.*|'"$OUT"'/&|' "$SEEN" | xargs -n1 basename | sed 's|^|'"$OUT"'/|') 2>/dev/null | tail -1 | cut -f1)
echo "==> bundle-dylibs: bundled $COUNT dylib(s) ($TOTAL) beside $(basename "$BIN")"
