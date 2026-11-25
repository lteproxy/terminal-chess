#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
missing=()

for command in cmake c++ pkg-config; do
    if ! command -v "$command" >/dev/null 2>&1; then
        missing+=("$command")
    fi
done

if command -v pkg-config >/dev/null 2>&1 && ! pkg-config --exists sdl2; then
    missing+=("sdl2")
fi

if ((${#missing[@]})); then
    printf 'es fehlen abhaengigkeiten: %s\n' "${missing[*]}" >&2
    printf 'installiere sie mit: sudo pacman -S --needed base-devel cmake sdl2\n' >&2
    exit 1
fi

cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$project_dir/build" --parallel
exec "$project_dir/build/terminal-chess" "$@"
