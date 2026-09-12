#!/bin/zsh
set -euo pipefail

source /Users/a58/esp/esp-idf-v5.5.1/export.sh >/dev/null 2>&1
exec idf.py "$@"
