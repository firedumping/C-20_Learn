#!/usr/bin/env bash

set -euo pipefail

preset="${1:-dev}"

case "${preset}" in
    dev|release|sanitize)
        ;;
    *)
        printf 'usage: %s [dev|release|sanitize]\n' "$0" >&2
        exit 2
        ;;
esac

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

cmake --preset "${preset}"
cmake --build --preset "${preset}"
ctest --preset "${preset}" --output-on-failure
