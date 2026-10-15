#!/bin/zsh
set -euo pipefail
# Compatibility with existing local build commands.
exec "${0:A:h}/build-vita-vitagl.sh" "$@"
