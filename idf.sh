#!/bin/zsh
set -euo pipefail

# Uses ~/esp/esp-idf-v5.5.1 unless IDF_PATH names another ESP-IDF 5.5.1 checkout.
idf_path="${IDF_PATH:-$HOME/esp/esp-idf-v5.5.1}"
source "$idf_path/export.sh" >/dev/null 2>&1
exec idf.py "$@"
