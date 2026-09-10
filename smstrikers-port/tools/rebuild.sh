#!/bin/sh
# Compile, generate stubs, link, in the order the steps depend on each other.

set -e

BUILD="${1:-build-aur}"   # usage: tools/rebuild.sh [build-dir]

LOG=$(mktemp)

echo "==> compile"
# Expected to fail at the *link* on a clean tree, the stubs do not exist yet.
set +e
cmake --build "$BUILD" -j10 >"$LOG" 2>&1
set -e
# Match compiler diagnostics anchored to a source location, so the link error this pass is
# *expected* to end with does not read as a compile failure.
DIAG='\.(c|cc|cpp|h|hpp)[:(][0-9]+[:,][0-9]+\)?: (fatal )?error:'
if grep -qE "$DIAG" "$LOG"; then
    echo "==> COMPILE FAILED" >&2
    grep -E "$DIAG" "$LOG" | head -20 >&2
    rm -f "$LOG"
    exit 1
fi

# A step that fails with no compiler diagnostic reaches genstubs with almost nothing compiled, so
# every failed edge but the link of the binary is fatal. tr -d because a Windows build tool writes
# CRLF.
OTHER=$(sed -n 's/^FAILED: \(\[code=[0-9]*\] \)\{0,1\}//p' "$LOG" \
        | tr -d '\r' \
        | sed 's/[[:space:]]*$//' \
        | grep -vxE 'strikers(\.exe)?' || true)
if [ -n "$OTHER" ]; then
    echo "==> COMPILE FAILED: a build step failed with no compiler diagnostic" >&2
    echo "    failed edges, the link of the binary aside:" >&2
    echo "$OTHER" | sed 's/^/      /' | head -10 >&2
    grep -A4 '^FAILED: ' "$LOG" | head -40 >&2
    rm -f "$LOG"
    exit 1
fi

echo "==> generate stubs"
# The leading underscore is Mach-O's and must come from where genstubs.py derives it: a hard-coded
# '^_GX' matched nothing on ELF or COFF, so GX became an abort-on-call with no build error to say
# so.
U=$(python3 tools/genstubs.py --print-symbol-prefix)
# NOT >/dev/null: genstubs names every mangled symbol it stubs, and a stub over a function the
# source already has is invisible until the game aborts on it.
STRIKERS_BUILD_DIR="$BUILD" python3 tools/genstubs.py \
    --noop "^${U}GX|^${U}snd|^${U}AI|^${U}AR"

echo "==> link"
set +e
cmake --build "$BUILD" --target strikers -j10 >"$LOG" 2>&1
STATUS=$?
set -e
if [ "$STATUS" -ne 0 ]; then
    echo "==> LINK FAILED" >&2
    grep -E 'error|Undefined|undefined' "$LOG" | head -20 >&2
    rm -f "$LOG"
    exit 1
fi
rm -f "$LOG"

# The link above is fatal on failure, so what is left to check is that the thing it claimed to
# produce is actually there, a stale executable from a previous run being mistaken for a good build
# is what this script exists to stop.
BIN="$BUILD/strikers"
if [ -f "$BIN.exe" ]; then
    BIN="$BIN.exe"
fi
if [ ! -f "$BIN" ]; then
    echo "==> FAILED: no binary produced" >&2
    exit 1
fi
echo "==> ok: $BIN"
