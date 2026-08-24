#!/usr/bin/env bash
set -euo pipefail

declare -a dirs=("Crowny/Source" "Crowny-Editor/Source")

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is required" >&2
  exit 1
fi

format_file() {
  local file="$1"
  if command -v dos2unix >/dev/null 2>&1; then
    dos2unix "$file" >/dev/null 2>&1
  else
    sed -i 's/\r$//' "$file"
  fi
  clang-format -i "$file"
}

if (($# > 0)); then
  for file in "$@"; do
    case "$file" in
      *.h|*.cpp) format_file "$file" ;;
    esac
  done
  exit 0
fi

for dir in "${dirs[@]}"; do
  echo "Formatting $dir"
  while IFS= read -r -d '' file; do
    format_file "$file"
  done < <(find "$dir" -type f \( -name '*.h' -o -name '*.cpp' \) -print0)
done
